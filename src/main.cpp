#include <cwchar>
#include <cstdio>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <ctime>
#include <string>
#include <windows.h>
#include <shlobj.h>
#include <climits>
#include <map>
#include <excpt.h>

//project headers
#include "sqlite3.h"
#include "GUI.h"
#include "main.h"
#include "SQLFunctions.h"
#include "options.h"
#include "base64.h"
#include "VICS.h"
#include "XML.h"
#include "debugMessage.h"
#include "FileOutput.h"
#include "utility.h"
#include "ArchiveWriter.h"
#include "ReportTableAssociations.h"

using namespace std;

#define MaxVICSRecords 28435456LL
#define MaxVICSMovies 100000LL

//define error text
const wchar_t* const errorValues[] = {L"No MD5 Hash Value Located", L"No SHA1 Hash Value Located", L"Cannot Read File", L"File Size Zero", L"No Hash Available"};

//define hashvalues
#define hashTypeMD5 1
#define hashTypeSHA1 2
#define hashTypePDNA 3

#define TYPE_MEDIA                  0
#define TYPE_OTHER                  1
#define ERROR_GETITEMTYPE           2
#define ERROR_GETITEMTYPEDESC       3

//global variables
INT64 pictureCount,movieCount,picCount,vidCount,tmpPicCount,tmpVidCount, recordNum, vicPicCounter, vicMovieCounter;
FILE* currPicFile,*currVidFile, *picResults,*vidResults, *vicPicFile, *vicMovieFile;
INT64 MD5Hash=0,SHA1Hash = 0;
wchar_t caseTitle[64];
//add int's to track errors
INT64 noErrorHash;
int firstTime = 0,currentFileObject,itemCompleted=0, noNames=0;
LONG lastItemID;
CRITICAL_SECTION lockC4All, lockVics, updateLock, xwfOutputLock;
//VICSRecord* VICSPics, *VICSMovie;

HANDLE MainWnd, hdlCurrVol;
DWORD currEvidence;
WORD versionNo;
wchar_t* currSrcID;
ExtractionDetails extractInfo;
ExtractOptions extractOpt;
wchar_t currentEvObject[2048];
wchar_t XT_PATH[2048];
LPWSTR txtCurrObj;
sqlite3 *vicsDB;
const wchar_t* progVersion = C4A_VERSION_W;
const wchar_t* INFO_DELETION[] =    {L"Existing",
                                    L"Previously existing, possibly recoverable",
                                    L"Previously existing, first cluster overwritten or unknown",
                                    L"Renamed/moved, possibly recoverable",
                                    L"Renamed/moved, first cluster overwritten or unknown",
                                    L"Carved File"};
const int max_deletion = 5;

//prototyping
INT64 filetime2Unix(INT64 fTime);
wchar_t* getFullPath(LPWSTR evObject,LONG nItemID, BOOL isVIC);
int createC4POutput();
void removeInvalidChars(wchar_t* strIn);

int createC4AllRecord(LONG nItemID, int picture, wchar_t MD5Hash[33]);
LONG mainItemProcess(LONG nItemID, int picture, INT64 fileSize);
int updateRecords(int picture, long nItemID,hashValueStruct currHash);

int createVICSRecord(LONG nItemID, int picture,hashValueStruct hashVals);

void fillItemDetails();
int getCaseOptions();
void createSQLNameList(HANDLE evObj);
int returnHashValue(LONG nItemID, wchar_t* md5Buffer, wchar_t* SHA1Buffer, wchar_t* PDNABuffer);
wchar_t* replaceInvalidXMLChars(wchar_t* strIn);
int getCommandLineOptions();
void addCaseDetail(wchar_t* strArg);
void freeVicsCaseData();

int determineHashTypes();
int determineColumnNumber(wchar_t* compareStr);
int checkItemType(LONG nItemID, int* picture);
int checkParentType(LONG nItemID);
int caseCleanup();

int firstRunSetup();
int volumePrepare(HANDLE hEvidence);
bool checkItemExport(LONG nItemID, int* picture, INT64* fileSize);

//VCIS Stuff

//int writeRecords(FILE* vicFile, int picture);
int outputVICSFile();
int writeRecords(sqlite3* database,FILE* vicFile, int picture);

int DeviceTypeCol = -1;
INT64 getPhysicalOffset(DWORD nItemID);
INT64 getPhysicalOffset(DWORD nItemID, BOOL* unallocated, BOOL* deleted);

/**
 * @brief DLL entry point.
 *
 * On DLL_PROCESS_ATTACH, stores the DLL instance handle and module path, and
 * initialises all CRITICAL_SECTION locks used across the DLL. On
 * DLL_PROCESS_DETACH, releases those same locks.
 *
 * @return TRUE always.
 */
BOOL APIENTRY DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        extractInfo.thisDLL = hInstDLL;
        GetModuleFileNameW(hInstDLL, XT_PATH, MAX_PATH);
        InitializeCriticalSection(&lockC4All);
        InitializeCriticalSection(&lockVics);
        InitializeCriticalSection(&updateLock);
        InitializeCriticalSection(&xwfOutputLock);
        initFileOutputLocks();
        initArchiveLocks();
        initDebugLocks();
        break;
    case DLL_PROCESS_DETACH:
        DeleteCriticalSection(&lockC4All);
        DeleteCriticalSection(&lockVics);
        DeleteCriticalSection(&updateLock);
        DeleteCriticalSection(&xwfOutputLock);
        destroyFileOutputLocks();
        destroyArchiveLocks();
        destroyDebugLocks();
        break;
    }
    return TRUE;
}


// X-Ways Functions

/**
 * @brief Init function called when X-Tension is loaded.
 *
 * Checks the X-Ways version and decides if the DLL can be used. Also loads options from SQLite database.
 *
 * @return -1 to prevent use of DLL, 2 if X-Tension is not thread-safe (always returned on success).
 *
 * @see loadOrCreateOptions
 */
LONG DLL_EXPORT XT_Init(CallerInfo info, DWORD nFlags, HANDLE hMainWnd, void* lpReserved)
{
    if (extractInfo.debugSet){debugWriteDetails(0,L"XT_Init Start");}
    //required if testing command line to set debugger
    //MessageBox(NULL,"Test","Test",MB_OK);
    XT_RetrieveFunctionPointers();
    int retVal = sqlInit();
    if (retVal !=0){
        //error with SQL
        return -1;
    }
    //get main window handle
    MainWnd = hMainWnd;
    versionNo = info.version;
    if (info.version < 1920)
    {
        XWF_OutputMessage(L"X-Ways version below 19.2, this is not supported",0);
        return -1;
    }
    else if (info.version < 1930)
    {
        XWF_OutputMessage(L"X-Ways version below 19.3, some features are disabled",0);
    }
    else if (info.version < 1940)
    {
        XWF_OutputMessage(L"X-Ways version below 19.4, some features are disabled",0);
    }
    else if (info.version < 1970)
    {
        XWF_OutputMessage(L"X-Ways version below 19.7, some features are disabled",0);
    }
    else if (info.version < 2030)
    {

        XWF_OutputMessage(L"X-Ways version below 20.3, some features are disabled",0);
        XWF_OutputMessage(L"Specifically Device type detection is disabled",0);
    }
    else if (info.version < 2050)
    {

        XWF_OutputMessage(L"X-Ways version below 20.5, some features are disabled",0);
        XWF_OutputMessage(L"Specifically 'Extracting thumbnails embedded in picture files only if mismatched' will not be available",0);
    }
    extractInfo.C4PPath = new wchar_t[1024];
    extractInfo.C4PPath[0] =L'\0';
    extractInfo.C4MPath = new wchar_t[1024];
    extractInfo.C4MPath[0] =L'\0';
    extractInfo.outputFileCounter = 0;
    extractInfo.processStart = FALSE;
    extractInfo.debugSet = FALSE;
    extractInfo.VICExport = TRUE;
    extractInfo.C4ALLExport = TRUE;
    extractInfo.createGriffeye = TRUE;
    extractInfo.GriffeyeCaseLocation = NULL;
    extractInfo.GriffeyeCaseName = NULL;

    //get extraction options
    BOOL loadSuccess;
    extractOpt = loadOrCreateOptions(&loadSuccess);

    loadLastExtractionSettings(&extractInfo);
    if (extractInfo.debugSet){debugWriteDetails(0,L"XT_Init End");}
    return 2;
}

/**
 * @brief Called by X-Ways to display the options window.
 *
 * @return 0 always.
 *
 * @see createOptionsWindow
 */
LONG DLL_EXPORT XT_About(HANDLE hParentWnd, PVOID lpReserved)
{
    //setup options
    createOptionsWindow();
    return 0;
}


/**
 * @brief Called at the start of each new volume being processed.
 *
 * Displays the configuration window on first call, then prepares hash types and volume state.
 *
 * @return -4 if VICS setup fails, -3 if not run from RVS or MD5 hash type not configured or getCaseOptions fails, 3 (XT_PREPARE_CALLPI | XT_PREPARE_CALLPILATE) on success.
 *
 * @see determineHashTypes
 * @see getCaseOptions
 */
LONG DLL_EXPORT XT_Prepare(HANDLE hVolume, HANDLE hEvidence, DWORD nOpType,void* lpReserved)
{
    if (extractInfo.debugSet){debugWriteDetails(0,L"XT_Prepare Start");}
    //check if run from RVS
    if (nOpType != XT_ACTION_RVS)
    {
        //not rvs- kill X-Tension
        XWF_OutputMessage(L"X-Tension must be run from RVS screen",0);
        return -3;
    }
    //start doing useful stuff
    //figure out hash values
    int retVal = determineHashTypes();
    if (MD5Hash == 0 || retVal!=0)
    {
        //MD5 hash not available
        XWF_OutputMessage(L"MD5 Hash not selected as primary or secondary hash. MD5 hash is mandatory",0);
        return -3;
    }
    //get evidence object details
    hdlCurrVol = hVolume;
    currEvidence= (DWORD)XWF_GetEvObjProp(hEvidence,1,NULL);
    txtCurrObj = (LPWSTR)XWF_GetEvObjProp(hEvidence,6,NULL);
    tmpPicCount = 0;
    tmpVidCount = 0;
    if (firstTime == 0)
    {
        int firstRun = firstRunSetup();
        firstTime = 1;
        if (firstRun != 0) { return firstRun;}
    }

    int result = volumePrepare(hEvidence);
    if (extractInfo.debugSet){debugWriteDetails(0,L"XT_Prepare End");}
    return 0x03;
}


/**
 * @brief Main worker function called per item during RVS processing.
 *
 * Checks whether the item is an exportable media file and calls mainItemProcess if so.
 *
 * @return 0 always.
 *
 * @see checkItemExport
 * @see mainItemProcess
 */
LONG DLL_EXPORT XT_ProcessItem(LONG nItemID, void* lpReserved)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"XT_ProcessItem Start");}
    int picture;
    INT64 fileSize;
    if (checkItemExport(nItemID,&picture,&fileSize)){
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"XT_ProcessItem - Valid Item");}
        return mainItemProcess(nItemID, picture, fileSize);
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"XT_ProcessItem End");}
    return 0;
}


/**
 * @brief Called at the end of processing an evidence object.
 *
 * Resets per-volume counters and writes the media item counts to the results text files.
 *
 * @return 0 always.
 */
LONG DLL_EXPORT XT_Finalize(HANDLE hVolume, HANDLE hEvidence, DWORD nOpType,void* lpReserved)
{
    if (extractInfo.debugSet){debugWriteDetails("XT_Finalize Function Start");}
    //Move counters to records and set FILE pointers to NULL
    currPicFile = NULL;
    currVidFile = NULL;
    extractInfo.outputFiles[currentFileObject].picCounter = picCount;
    extractInfo.outputFiles[currentFileObject].vidCounter = vidCount;
    if (extractInfo.extractPictures)
    {
        fprintf(picResults,"%ls: %llu\r\n",txtCurrObj,tmpPicCount);
        tmpPicCount = 0;
    }
    if (extractInfo.extractVideos)
    {
        fprintf(vidResults,"%ls: %llu\r\n",txtCurrObj,tmpVidCount);
        tmpVidCount = 0;
    }
    if (extractInfo.debugSet){debugWriteDetails("XT_Finalize Function End");}
    return 0;
}


/**
 * @brief Called by X-Ways before the X-Tension is unloaded.
 *
 * Triggers case cleanup and error reporting.
 *
 * @return 0 always.
 *
 * @see caseCleanup
 * @see errorReport
 */

LONG DLL_EXPORT XT_Done(void* lpReserved)
{
    //check if process was exited
    if (extractInfo.debugSet){debugWriteDetails("XT_Done Function Start");}
    if (extractInfo.processStart == FALSE)
    {
        return 0;
    }
    int retVal = caseCleanup();
    errorReport();
    if (extractInfo.debugSet){debugWriteDetails("XT_Done Function End");}
    return 0;
}

// Initial Setup Functions

/**
 * @brief Performs one-time setup on the first XT_Prepare call.
 *
 * Initialises the VICS database, reads case options, and identifies report tables.
 *
 * @return 0 on success, -4 if VICS setup fails, -3 if getCaseOptions fails.
 *
 * @see setupVics
 * @see getCaseOptions
 */

int firstRunSetup()
{
    if (extractInfo.debugSet){debugWriteDetails("firstRunSetup Function Start");}
    noErrorHash = 0;
    int checkVics = setupVics(&vicsDB);
    if (checkVics !=0)
    {
        return -4;
    }
    int result = getCaseOptions();
    if (result !=0) { return -3;}
    INT64 strLen = XWF_GetCaseProp(NULL, 1, &caseTitle,64);
    if (strLen < 0)
    {
        outputErrorMessage(L"XWF_GetCaseProp failed in firstRunSetup");
    }
    else if (strLen > 64)
    {
        XWF_OutputMessage(L"Case Title is bigger than 64 chars, truncated",0);
    }
    writeExtractionDetails(extractInfo);
    result = identifyReportTables();
    if (extractInfo.debugSet){debugWriteDetails("firstRunSetup Function End");}
    return 0;
}


/**
 * @brief Creates the C4P/C4M XML output files for each evidence object.
 *
 * Creates a picture and/or video XML file per evidence name, depending on which export types are enabled.
 *
 * @return 0 on success, -1 if a file could not be created.
 *
 * @see getCaseOptions
 */
int createC4POutput()
{
    if (extractInfo.debugSet){debugWriteDetails("createC4POutput Function Start");}
    wchar_t buffer[2048];
    for (int i =0;i<extractInfo.noNames;i++)
    {
        char filepath[2048];
        wcscpy(buffer, extractInfo.nameList[i].prefName);
        int evObjLength = wcslen(buffer);
        for (int j=0; j<evObjLength;j++)
        {
            if (buffer[j] == L'<' || buffer[j] == L'>' ||buffer[j] == L':' ||buffer[j] == L'\"' || buffer[j] == L'/' || buffer[j] == L'\\' || buffer[j] == L'|' || buffer[j] == L'?' || buffer[j] == L'*')
            {
                buffer[j]=L'_';
            }
        }
        if (extractInfo.extractPictures)
        {
            snprintf(filepath,2048,"%ls%ls C4P Index.xml",extractInfo.C4PPath,buffer);
            extractInfo.outputFiles[extractInfo.outputFileCounter].picOutput = createXML(filepath, progVersion);
            if (extractInfo.outputFiles[extractInfo.outputFileCounter].picOutput == NULL)
            {
                wchar_t errorMessage[2048];
                swprintf(errorMessage,L"Unable to open file for picture output. Filepath : %s",filepath);
                XWF_OutputMessage(errorMessage,0);
                if (extractInfo.debugSet){debugWriteDetails("createC4POutput Function End - Return -1");}
                return -1;
            }
            extractInfo.outputFiles[extractInfo.outputFileCounter].picCounter = 0;
            updateFileNumber(vicsDB, extractInfo.nameList[i].objectID,extractInfo.outputFileCounter);
            filepath[0]='\0';
        }
        if (extractInfo.extractVideos)
        {
            snprintf(filepath,2048,"%ls%ls C4M Index.xml",extractInfo.C4MPath,buffer);
            extractInfo.outputFiles[extractInfo.outputFileCounter].vidOutput = createXML(filepath, progVersion);
            if (extractInfo.outputFiles[extractInfo.outputFileCounter].vidOutput == NULL)
            {
                wchar_t errorMessage[2048];
                swprintf(errorMessage,L"Unable to open file for movie output. Filepath : %s",filepath);
                XWF_OutputMessage(errorMessage,0);
                if (extractInfo.debugSet){debugWriteDetails("createC4POutput Function End - Return -1");}
                return -1;
            }
            extractInfo.outputFiles[extractInfo.outputFileCounter].vidCounter= 0;
            filepath[0]='\0';
        }
        extractInfo.outputFileCounter++;
    }
    if (extractInfo.debugSet){debugWriteDetails("createC4POutput Function End");}
    return 0;
}



/**
 * @brief Builds the evidence-object name list in the VICS database.
 *
 * @param evObj X-Ways evidence object handle.
 * @return 0 always.
 */
int createNameList(HANDLE evObj)
{
    createSQLNameList(vicsDB, evObj);
    return 0;
}

/**
 * @brief Determines which hash slots (primary/secondary) are MD5 or SHA1.
 *
 * Stores results in the global variables MD5Hash and SHA1Hash.
 * 1 = primary slot, 2 = secondary slot, 0 = not computed.
 *
 * @return 0 always.
 */

int determineHashTypes()
{
    if (extractInfo.debugSet){debugWriteDetails("determineHashTypes Function Start");}
    MD5Hash = 0;
    SHA1Hash = 0;
    INT64 hashtype = XWF_GetVSProp(XWF_VSPROP_HASHTYPE1,NULL);
    if (hashtype == 7)
    {
        MD5Hash = 1;
    }
    else if (hashtype == 8)
    {
        SHA1Hash = 1;
    }
    hashtype = XWF_GetVSProp(XWF_VSPROP_HASHTYPE2,NULL);
    if (hashtype == 7)
    {
        MD5Hash = 2;
    }
    else if (hashtype == 8)
    {
        SHA1Hash = 2;
    }
    if (extractInfo.debugSet){debugWriteDetails("determineHashTypes Function End");}
    return 0;
}

/**
 * @brief Creates the per-run picture and video results text files.
 *
 * Also opens the debug log file if debug mode is enabled.
 *
 * @return 0 on success, 1 if a file could not be opened.
 *
 * @see getCaseOptions
 */

int setupResultsFiles()
{
    if (extractInfo.debugSet){debugWriteDetails("Start of setupResultsFiles Function");}
    char filePath[2048]={0};
    if (extractInfo.extractPictures)
    {
        snprintf(filePath,sizeof(filePath),"%ls Pictures Results.txt",extractInfo.C4PPath);
        picResults=fopen(filePath,"w");
        if (picResults == NULL) { return 1; }
        filePath[0]='\0';
        if (extractInfo.debugSet)
        {
            //open debug file
            snprintf(filePath,sizeof(filePath),"%lsdebug.log",extractInfo.C4PPath);
            int retVal = startDebugLog(filePath);
            if (retVal !=0)
            {
                if (extractInfo.debugSet){debugWriteDetails("Error creating Debug log file in Pictures output folder");}
                return 1;
            }
            filePath[0]='\0';
        }
    }
    if (extractInfo.extractVideos)
    {
        snprintf(filePath,sizeof(filePath),"%ls Video Results.txt",extractInfo.C4MPath);
        vidResults=fopen(filePath,"w");
        if (vidResults == NULL) { return 1; }
        filePath[0]='\0';
        if (extractInfo.debugSet && (!extractInfo.extractPictures))
        {
            //open debug file
            snprintf(filePath,sizeof(filePath),"%lsdebug.log",extractInfo.C4MPath);
            int retVal = startDebugLog(filePath);
            if (retVal !=0)
            {
                if (extractInfo.debugSet){debugWriteDetails("Error creating Debug log file in Videos output folder");}
                return 1;
            }
            filePath[0]='\0';
        }
    }
    if (extractInfo.debugSet){debugWriteDetails("End of setupResultsFiles Function");}
    return 0;
}

/**
 * @brief Opens the VICS JSON output files for pictures and/or videos.
 *
 * @return 0 on success, -1 if the picture file could not be opened, -2 if the video file could not be opened.
 *
 * @see getCaseOptions
 */

int setupVicsExport()
{
    char filePath[2048]={0};
    if (extractInfo.debugSet){debugWriteDetails("Start of setupVicsExport Function");}
    if (extractInfo.extractPictures)
    {
        snprintf(filePath,2048,"%lsVICS_Pictures_Results.json",extractInfo.C4PPath);
        vicPicFile = openVICSFile(filePath, progVersion);
        if (vicPicFile == NULL)
        {
            if (extractInfo.debugSet){debugWriteDetails("Error opening VICS Picture file");}
            outputErrorMessage(L"Error opening VICS Picture file:",extractInfo.C4PPath);
            return -1;
        }
        filePath[0] = '\0';
    }
    if (extractInfo.extractVideos)
    {
        snprintf(filePath,2048,"%lsVICS_Movies_Results.json",extractInfo.C4MPath);
        vicMovieFile = openVICSFile(filePath, progVersion);
        if (vicMovieFile == NULL)
        {
            if (extractInfo.debugSet){debugWriteDetails("Error opening VICS Picture file");}
            outputErrorMessage(L"Error opening VICS Video file:",extractInfo.C4MPath);
            return -2;
        }
        filePath[0] = '\0';
    }
    //had to remove as it was printing to vicsmovie file somehow!
    //if (extractInfo.debugSet){debugWriteDetails("End of setupVicsExport Function");}
    return 0;
}

/**
 * @brief Performs first-run case setup: shows the GUI or processes command-line args, then creates output files.
 *
 * @return 0 on success, -1 on generic failure, -4 if the process could not start.
 *
 * @see setupVicsExport
 * @see setupResultsFiles
 */

int getCaseOptions()
{
    if (extractInfo.debugSet){debugWriteDetails("Start of getCaseOptions Function");}
    vicPicCounter = 0;
    extractInfo.extractPictures= TRUE;
    extractInfo.extractVideos= TRUE;
    //before we create window, get evidence object list
    HANDLE evObj = XWF_GetFirstEvObj(NULL);
    if (evObj==NULL){
        //no evidence objects, don't run
        XWF_OutputMessage(L"No evidence objects",0);
        return -1;
    }
    do
    {
        int result = createNameList(evObj);
        if (result!=0)
        {
            return result;
        }
        //move to next evidence object
        evObj = XWF_GetNextEvObj(evObj,NULL);
    }
    while (evObj!=NULL);
    //update parent objects to selected where not actually selected
    checkParentObjectsSelected(vicsDB);
    //get name list
    extractInfo.nameList = retrieveEvidenceNames(vicsDB, &extractInfo.noNames);
    //first time prepare has been called, display window
    int optionsDone = 0;
    if (versionNo >= 1950)
    {
        optionsDone = getCommandLineOptions();
    }
    if (optionsDone == 0)
    {
        createWindow(versionNo);
        if (!extractInfo.processStart)
        {
            XWF_OutputMessage(L"Error in process start",0);
            return -4;
        }
    }
    else
    {
        char buffer[1024];
        if (extractInfo.C4ALLExport || extractInfo.VICExport)
        {
            snprintf(buffer,sizeof(buffer),"%lsFiles",extractInfo.C4PPath);
            CreateDirectory(buffer,NULL);
        }
        if (extractInfo.C4ALLExport || extractInfo.VICExport)
        {
            snprintf(buffer,sizeof(buffer),"%lsFiles",extractInfo.C4MPath);
            CreateDirectory(buffer,NULL);
        }
    }
    //update database with new names
    updateEvidenceNames(vicsDB, extractInfo.nameList, extractInfo.noNames);
    char filePath[2048]={0};
    if (extractInfo.debugSet)
    {
        XWF_OutputMessage(L"Debug mode on",0);
    }
    //open results files.
    int retVal = setupResultsFiles();
    if (retVal !=0)
    {
        outputErrorMessage(L"Error setting up results files");
        return -1;
    }
    //create C4P/M output files if applicable
    if (extractInfo.C4ALLExport)
    {
        int check = createC4POutput();
        if (check !=0)
        {
            //error creating files
            return -1;
        }
    }
    if (extractInfo.VICSCompressed)
    {
        setArchivePath(extractInfo.C4PPath, SET_PIC_PATH);
        setArchivePath(extractInfo.C4MPath, SET_VID_PATH);
        setupZipArchives();
    }
    if (extractInfo.debugSet){debugWriteDetails("End of GetCaseOptions function");}
    return 0;
}

/**
 * @brief Parses XTparam command-line arguments into extractInfo.
 *
 * @return 0 if no usable arguments found, 1 if arguments were extracted successfully.
 *
 * @see getCaseOptions
 */

int getCommandLineOptions()
{
    if (extractInfo.debugSet){debugWriteDetails(0, L"getCommandLineOptions Start");}
    int numArgv;
    //get commandline options and change to lower case
    LPWSTR cmdLine = GetCommandLineW();
    int cmdLineLen = wcslen(cmdLine);
    for (int j = 0;j<cmdLineLen;j++)
    {
        cmdLine[j]=towlower(cmdLine[j]);
    }
    //split options into delimited sets
    LPWSTR* argv = CommandLineToArgvW(cmdLine,&numArgv);
    if (argv == NULL)
    {
        XWF_OutputMessage(L"CommandLineToArgvW Failed\n",0);
    }
    if (numArgv == 1)
    {
        if (extractInfo.debugSet){debugWriteDetails(0, L"getCommandLineOptions End - No Args");}
        LocalFree(argv);
        return 0;
    }
    //check for XTparam
    for (int i = 0;i<numArgv;i++)
    {
        if (wcsncmp(L"xtparam:",argv[i],8)==0)
        {
            size_t argLen = wcslen(argv[i]);
            if (argLen <= 8) continue;
            size_t valueLen = argLen - 8;
            wchar_t* tempArg = new wchar_t[valueLen + 1];
            memcpy(tempArg, argv[i] + 8, valueLen * sizeof(wchar_t));
            tempArg[valueLen] = L'\0';
            addCaseDetail(tempArg);
            delete[] tempArg;
        }
    }
    if ((extractInfo.C4MPath[0] == L'\0') && (extractInfo.C4PPath[0] == L'\0'))
    {
        if (extractInfo.debugSet){debugWriteDetails(0, L"getCommandLineOptions End - No paths provided");}
        LocalFree(argv);
        return 0;
    }
    if (extractInfo.C4PPath[0] == L'\0')
    {
        extractInfo.extractPictures = FALSE;
    }
    if (extractInfo.C4MPath[0] == L'\0')
    {
        extractInfo.extractVideos = FALSE;
    }
    if (extractInfo.GriffeyeCaseLocation == NULL || extractInfo.GriffeyeCaseName == NULL)
    {
        extractInfo.createGriffeye = FALSE;
    }
    if (extractInfo.VICSCompressed == FALSE)
    {
        extractInfo.C4ALLExport = TRUE;
        extractInfo.VICExport = TRUE;
    }
    for (int i = 0;i<extractInfo.noNames;i++)
    {
        swprintf(extractInfo.nameList[i].prefName,L"%ls",extractInfo.nameList[i].actualName);
        XWF_OutputMessage(extractInfo.nameList[i].prefName,0);
    }
    HRESULT error = CoCreateGuid(&vCaseData.caseGuid);
    if (extractInfo.GriffeyeCaseName == NULL)
    {
        vCaseData.CaseNumber = new wchar_t[128];
        INT64 result = XWF_GetCaseProp(NULL,1,vCaseData.CaseNumber,128);
        if (result < 0)
        {
            outputErrorMessage(L"XWF_GetCaseProp failed in getCommandLineOptions");
            vCaseData.CaseNumber[0] = L'\0';
        }
    }
    extractInfo.processStart = TRUE;
    LocalFree(argv);
    if (extractInfo.debugSet){debugWriteDetails(0, L"getCommandLineOptions End - return 1");}
    return 1;
}

/**
 * @brief Parses a single XTparam key:value argument and applies it to extractInfo.
 *
 * Paths are normalised to end with a backslash. Unknown keys produce a warning message.
 *
 * @param strArg Null-terminated wide string containing one key:value parameter.
 *
 * @see getCommandLineOptions
 */

void addCaseDetail(wchar_t* strArg)
{
    if (extractInfo.debugSet){debugWriteDetails(0, L"addCaseDetail Start");}
    if (wcsncmp(L"picpth:",strArg,7)==0)
    {
        size_t valueLen = wcslen(strArg) - 7;
        if (valueLen == 0 || valueLen >= MAX_PATH - 2)
        {
            XWF_OutputMessage(L"Error: picpth value empty or too long (max 256 chars)",0);
            return;
        }
        memcpy(extractInfo.C4PPath, strArg + 7, valueLen * sizeof(wchar_t));
        if (extractInfo.C4PPath[valueLen - 1] != L'\\')
        {
            extractInfo.C4PPath[valueLen] = L'\\';
            extractInfo.C4PPath[valueLen + 1] = L'\0';
        }
        else
        {
            extractInfo.C4PPath[valueLen] = L'\0';
        }
        SHCreateDirectoryExW(NULL,extractInfo.C4PPath,NULL);
        XWF_OutputMessage(extractInfo.C4PPath,0);
    }
    else if (wcsncmp(L"vidpth:",strArg,7)==0)
    {
        if (wcsncmp(L"*",&strArg[7],1) == 0)
        {
            //use same as picpth
            if (extractInfo.C4PPath[0] !=  L'\0')
            {
                wcscpy(extractInfo.C4MPath, extractInfo.C4PPath);
                SHCreateDirectoryExW(NULL,extractInfo.C4MPath,NULL);
                XWF_OutputMessage(extractInfo.C4MPath,0);
            }
        }
        else
        {
            size_t valueLen = wcslen(strArg) - 7;
            if (valueLen == 0 || valueLen >= MAX_PATH - 2)
            {
                XWF_OutputMessage(L"Error: vidpth value empty or too long (max 256 chars)",0);
                return;
            }
            memcpy(extractInfo.C4MPath, strArg + 7, valueLen * sizeof(wchar_t));
            if (extractInfo.C4MPath[valueLen - 1] != L'\\')
            {
                extractInfo.C4MPath[valueLen] = L'\\';
                extractInfo.C4MPath[valueLen + 1] = L'\0';
            }
            else
            {
                extractInfo.C4MPath[valueLen] = L'\0';
            }
            SHCreateDirectoryExW(NULL,extractInfo.C4MPath,NULL);
            XWF_OutputMessage(extractInfo.C4MPath,0);
        }
    }
    else if (wcsncmp(L"grfpth:",strArg,7)==0)
    {
        size_t valueLen = wcslen(strArg) - 7;
        extractInfo.GriffeyeCaseLocation = new wchar_t[valueLen + 1];
        memcpy(extractInfo.GriffeyeCaseLocation, strArg + 7, valueLen * sizeof(wchar_t));
        extractInfo.GriffeyeCaseLocation[valueLen] = L'\0';
        SHCreateDirectoryExW(NULL,extractInfo.GriffeyeCaseLocation,NULL);
        XWF_OutputMessage(extractInfo.GriffeyeCaseLocation,0);
    }
    else if (wcsncmp(L"grfcse:",strArg,7)==0)
    {
        size_t valueLen = wcslen(strArg) - 7;
        extractInfo.GriffeyeCaseName = new wchar_t[valueLen + 1];
        memcpy(extractInfo.GriffeyeCaseName, strArg + 7, valueLen * sizeof(wchar_t));
        extractInfo.GriffeyeCaseName[valueLen] = L'\0';
        XWF_OutputMessage(extractInfo.GriffeyeCaseName,0);
    }
    else if (wcsncmp(L"excludeFromVid",strArg,14)==0)
    {
        //set option
        extractInfo.checkParent = true;
    }
    else if (wcsncmp(L"compressVICS",strArg,12)==0){
        extractInfo.C4ALLExport = false;
        extractInfo.VICExport = false;
        extractInfo.VICSCompressed = true;
    }
    else if (wcsncmp(L"grfset:",strArg,7)==0){
        size_t valueLen = wcslen(strArg) - 7;
        if (valueLen == 0)
        {
            XWF_OutputMessage(L"Error: grfset value empty",0);
        }
        else
        {
            extractInfo.GriffeyeSettingsName = new wchar_t[valueLen + 1];
            memcpy(extractInfo.GriffeyeSettingsName, strArg + 7, valueLen * sizeof(wchar_t));
            extractInfo.GriffeyeSettingsName[valueLen] = L'\0';
            XWF_OutputMessage(extractInfo.GriffeyeSettingsName,0);
        }
    }
    else if (wcsncmp(L"exthmbs",strArg,7)==0){
        extractInfo.ignoreThumbs = TRUE;
    }
    else if (wcsncmp(L"exthmbex",strArg,8)==0){
        extractInfo.ignoreThumbs = TRUE;
        extractInfo.exceptMismatch = TRUE;
    }
    else
    {
        XWF_OutputMessage(L"Unknown X-tension parameter",0);
    }
    if (extractInfo.debugSet){debugWriteDetails(0, L"addCaseDetail End");}
}

// Volume Setup Functions

/**
 * @brief Prepares state for the next volume being processed.
 *
 * Resolves the current evidence object's output file handles, counters, and source ID.
 *
 * @param hEvidence Handle to the X-Ways evidence object for this volume.
 * @return 0 always.
 *
 * @see XT_Prepare
 */

int volumePrepare(HANDLE hEvidence)
{
    if (extractInfo.debugSet){debugWriteDetails(0, L"volumePrepare Start");}
    //determine column name each time
    if (versionNo >= 2030){
        DeviceTypeCol = determineColumnNumber(L"Device type");
    }
    LPWSTR evObjName = (LPWSTR)XWF_GetEvObjProp(hEvidence,6,NULL);
    if (evObjName == NULL)
    {
        outputErrorMessage(L"XWF_GetEvObjProp failed in volumePrepare");
    }
    else
    {
        XWF_OutputMessage(evObjName,0);
    }
    //need to fill target value
    DWORD target = getRootObj(vicsDB,currEvidence);

    //locate fileNo for ID for C4P/M XML Export
    int fileNumber = getFileNumber(vicsDB,target);
    if (fileNumber >= 0)
    {
        //we have match
        currentFileObject=fileNumber;
        wcsncpy(currentEvObject,extractInfo.outputFiles[fileNumber].evidenceObj,2048);
        currentEvObject[2047] = L'\0';
        currPicFile = extractInfo.outputFiles[fileNumber].picOutput;
        currVidFile = extractInfo.outputFiles[fileNumber].vidOutput;
        picCount = extractInfo.outputFiles[fileNumber].picCounter;
        vidCount = extractInfo.outputFiles[fileNumber].vidCounter;
    }
    if (((currPicFile==NULL && extractInfo.extractPictures) ||
         (currVidFile == NULL && extractInfo.extractVideos))
        && (extractInfo.C4ALLExport))
    {
        XWF_OutputMessage(L"Didn't find a valid FILE",0);
    }
    //end of file matching

    //find srcID name
    if (currSrcID != NULL) { delete[] currSrcID; currSrcID = NULL; }
    if (target !=0)
    {
        currSrcID = getSourceIDName(vicsDB, target);
        if (currSrcID == NULL)
        {
            XWF_OutputMessage(L"Couldn't retrieve evidence object name",0);
        }
    }
    else
    {
        //not found
        XWF_OutputMessage(L"Couldn't retrieve evidence object",0);
        currSrcID = NULL;
    }
    if (extractInfo.debugSet){debugWriteDetails(0, L"volumePrepare End");}
    return 0;
}

/**
 * @brief Returns the column index whose title matches the supplied wide string.
 *
 * @param compareStr Null-terminated column title to search for.
 * @return Column index (>=0) if found, -1 if not located.
 */

int determineColumnNumber(wchar_t* compareStr)
{
    if (extractInfo.debugSet){debugWriteDetails(0,L"determineColumnNumber Start");}
    //set up a buffer to get column titles to
    wchar_t retBuffer[256];
    LPWSTR bufferPtr = (LPWSTR)&retBuffer;
    for (int i=0;i<256;i++)
    {
        BOOL retVal = XWF_GetColumnTitle(65535,i,bufferPtr);
        //check function success
        if (retVal)
        {
            //compare against
            if (wcscmp(bufferPtr,compareStr)==0)
            {
                if (extractInfo.debugSet){debugWriteDetails(0,L"determineColumnNumber End - Column located");}
                return i;
            }
        }
    }
    if (extractInfo.debugSet){debugWriteDetails(0,L"determineColumnNumber End - Not located");}
    return -1;
}

// Main Processing

/**
 * @brief Checks whether the parent item is a signature-confirmed video file.
 *
 * Used to exclude embedded frames or sub-videos from extraction.
 *
 * @param nItemID X-Ways item ID of the child item to check.
 * @return 1 if the parent is a confirmed video, 0 otherwise.
 */

int checkParentType(LONG nItemID)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkParentType Start");}
    LONG parentItemID = XWF_GetItemParent(nItemID);
    if (parentItemID == -1)
    {
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkParentType Exit - Either root item or error");}
        return 0;
    }
    wchar_t typeBuffer[128]={0};
    DWORD length = 0x40000080;
    LONG typeStatus = XWF_GetItemType(parentItemID,(wchar_t*)&typeBuffer,length);
    if (typeStatus == -1)
    {
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkParentType Exit - Type Status error");}
        return 0;
    }
    if (wcscmp(typeBuffer,(LPWSTR)L"Video")==0)
    {
        //check it is a verified type
        if (typeStatus < 3 || typeStatus == 4)
        {
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkParentType Exit - Type Status unverified");}
            return 0;
        }
    }
    else{
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkParentType Exit - Parent not Video");}
        return 0;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkParentType End");}
    return 1;
}

/**
 * @brief Determines whether an item is a picture or video (or neither).
 *
 * Sets @p picture to 1 if the item is a picture, 0 if it is a video. Macromedia Flash items
 * in the Internet category are treated as video.
 *
 * @param nItemID  X-Ways item ID.
 * @param picture  Output: 1 for picture, 0 for video.
 * @return TYPE_MEDIA if item is a media file, TYPE_OTHER if not.
 */

int checkItemType(LONG nItemID, int* picture)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkItemType Start");}
    wchar_t type[128] = {0};
    DWORD length = 0x40000080;

    LONG retVal = XWF_GetItemType(nItemID,type,length);
    //return error val if function fails.
    if (retVal == -1)    {
            return ERROR_GETITEMTYPE;
    }
    if (!(((wcscmp(type,(LPWSTR)L"Pictures")==0) && extractInfo.extractPictures)||((wcscmp(type,(LPWSTR)L"Video")==0) && extractInfo.extractVideos)))
    {
        if (wcscmp(type,(LPWSTR)L"Internet")==0)
        {
            long descLen = 0x20000080;
            wchar_t descr[128]={0};
            LONG retVal = XWF_GetItemType(nItemID,(wchar_t*)&descr,descLen);
            if (retVal == -1){
                    return ERROR_GETITEMTYPEDESC;
            }
            if (wcsncmp(descr,L"Macromedia Flash",16)!=0 || !extractInfo.extractVideos)
            {
                if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkItemType End Return TYPE_OTHER");}
                return TYPE_OTHER;
            }
        }
        else{
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkItemType End Return TYPE_OTHER");}
            return TYPE_OTHER;
        }
    }
    //must be picture or movie
    *picture = 0;
    if (wcscmp(type,(LPWSTR)L"Pictures")==0)
    {
        *picture = 1;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkItemType End - return TYPE_MEDIA");}
    return TYPE_MEDIA;
}

/**
 * @brief Returns true if the file size falls within the user-configured min/max range.
 *
 * @param opt      Current extraction options containing the size limits.
 * @param fileSize File size in bytes.
 * @param picture  1 if picture limits apply, 0 for movie limits.
 * @return true if within range, false otherwise.
 */

bool validFileSize(ExtractOptions opt, INT64 fileSize, int picture)
{
    if (extractInfo.debugSet){debugWriteDetails(0, L"validFileSize - Start");}
    uint64_t minSize, maxSize;
    if (picture == 1){
        minSize = opt.minPictureSize;
        maxSize = opt.maxPictureSize;
    }
    else{
        minSize = opt.minMovieSize;
        maxSize = opt.maxMovieSize;
    }
    //if no maxsize set, set to -1 (unsigned so that will be huge)
    if (maxSize == 0){ maxSize = -1;}
    if (fileSize <= maxSize && fileSize >= minSize)
    {
        if (extractInfo.debugSet){debugWriteDetails(0, L"validFileSize - End Return True");}
        return true;
    }
    else
    {
        if (extractInfo.debugSet){debugWriteDetails(0, L"validFileSize - End Return False");}
        return false;
    }
}


/**
 * @brief Returns true if the item is a recognised media type (picture or video).
 *
 * @param nItemID  X-Ways item ID.
 * @param picture  Output: 1 if picture, 0 if video.
 * @return true if the item is a media file, false otherwise.
 */
bool validType(LONG nItemID, int* picture)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"validType - Start");}
    int typeValid = checkItemType(nItemID,picture);
    if (typeValid != TYPE_MEDIA){
        if (typeValid == TYPE_OTHER)    {return 0;}
        else{
        //some kind of error
            if (typeValid == ERROR_GETITEMTYPE){
                //add file to report table for easier finding
                errorRaised(nItemID,REPORT_ERROR_TYPE);
                if (extractInfo.debugSet){debugWriteDetails(nItemID, L"validType - End return false");}
                return false;
            }
        }
    }
    else if (typeValid == TYPE_OTHER){return false;}
    else{
        //some kind of error
        if (typeValid == ERROR_GETITEMTYPE){
            //add file to report table for easier finding
            errorRaised(nItemID,REPORT_ERROR_TYPE);
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"validType - End return false");}
            return false;
        }
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"validType - End return true");}
    return true;
}

/**
 * @brief Returns true if the item is a thumbnail embedded within a picture file.
 *
 * When the exceptMismatch option is set, items associated with a mismatch report table
 * are exempted and returned as false (i.e. included in extraction).
 *
 * @param nItemID X-Ways item ID.
 * @return true if the item should be excluded as a thumbnail, false otherwise.
 */
bool isThumbnailObject(LONG nItemID)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isThumbnailObject - Start of Function");}
    const wchar_t* itemName = XWF_GetItemName(nItemID);
    if (itemName == NULL)
    {
        outputErrorMessage(L"XWF_GetItemName returned NULL for itemID: ", nItemID);
        return false;
    }
    if (wcscmp(itemName, L"Thumbnail.jpg") != 0){
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isThumbnailObject - End Return False");}
        return false;
    }
    LONG parentID = XWF_GetItemParent(nItemID);
    if (parentID == -1)
    {
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isThumbnailObject - End Return False (no parent)");}
        return false;
    }
    DWORD buffLen = 1024 | 0x40000000;
    wchar_t buffer[1024] = {0};
    LONG itemStatus = XWF_GetItemType(parentID,buffer,buffLen);
    if (itemStatus == -1)
    {
        outputErrorMessage(L"XWF_GetItemType failed in isThumbnailObject for itemID: ", nItemID);
        return false;
    }
    if (wcscmp(buffer, L"Pictures")!=0){return false;}
    if (itemStatus == 3 || itemStatus == 5 || itemStatus == 6 ){
        if (extractInfo.exceptMismatch){
            //need to add code to check for report table association here
            buffer[0] = L'\0';
            LONG numTables = XWF_GetReportTableAssocs(parentID,buffer,1024);
            if (numTables < 0)
            {
                outputErrorMessage(L"XWF_GetReportTableAssocs failed in isThumbnailObject for itemID: ", nItemID);
                return true;
            }
            //return false if its contains mismatch table (i.e. include file), true if not (exclude)
            if (containsThumbnailMismatchTable(buffer,1024)){
                if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isThumbnailObject - End Return False");}
                return false;}
            else {
                    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isThumbnailObject - End Return True");}
                    return true;
            }
        }
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isThumbnailObject - End Return True");}
        return true;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isThumbnailObject - End Return False");}
    return false;
}

/**
 * @brief Returns true if the item's file type status matches the user-selected flags.
 *
 * @param nItemID X-Ways item ID.
 * @return true if the type status is one the user has chosen to include, false otherwise.
 */
bool isSelectedFileTypeStatus(LONG nItemID)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isSelectedFileTypeStatus - Start of Function");}
    LONG typeStatus = XWF_GetItemType(nItemID,NULL,0);
    if (typeStatus != -1){
        int typeVal=0;
        switch(typeStatus){
        case 0:
            typeVal = NOT_VERIFIED;
            break;
        case 1:
            typeVal = IRRELEVANT;
            break;
        case 2:
            typeVal = NOT_IN_LIST;
            break;
        case 3:
            typeVal = CONFIRMED;
            break;
        case 4:
            typeVal = NOT_CONFIRMED;
            break;
        case 5:
            typeVal = NEWLY_IDENTIFIED;
            break;
        case 6:
            typeVal = MISMATCH_DETECTED;
            break;
        }
        if (!(typeVal & extractOpt.TypeStatusFlags)){
            return false;
        }
    }
    else{
        debugWriteDetails(nItemID,L"Error determining File Type Status");
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isSelectedFileTypeStatus - End of Function");}
    return true;
}

/**
 * @brief Returns true if the item's file format/consistency status matches the user-selected flags.
 *
 * @param nItemID X-Ways item ID.
 * @return true if the format status is one the user has chosen to include, false otherwise.
 */
bool isSelectedFileFormatStatus(LONG nItemID)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isSelectedFileFormatStatus - Start of Function");}
    LONG rawFileFormat = XWF_GetItemType(nItemID,NULL,0x80000000);
    if (rawFileFormat == -1)
    {
        outputErrorMessage(L"XWF_GetItemType returned error in isSelectedFileFormatStatus for itemID: ", nItemID);
        return true;
    }
    LONG fileFormat = (rawFileFormat & 0xff00) >> 8;
    int typeVal=0;
    switch(fileFormat){
    case 0:
        typeVal = UNKNOWN;
        break;
    case 1:
        typeVal = OK;
        break;
    case 2:
        typeVal = IRREGULAR;
        break;
    case 3:
        typeVal = CORRUPT;
        break;
    }
    if (!(typeVal & extractOpt.FileTypeFlag)){
        return false;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isSelectedFileFormatStatus - End of Function");}
    return true;
}

/**
 * @brief Runs all exclusion checks and returns true if the item should be exported.
 *
 * Checks type, parent type, file size, thumbnail status, type status, and format consistency.
 *
 * @param nItemID   X-Ways item ID.
 * @param picture   Output: 1 if picture, 0 if video.
 * @param fileSize  Output: file size in bytes.
 * @return true if the item should be exported, false if it should be skipped.
 *
 * @see XT_ProcessItem
 */

bool checkItemExport(LONG nItemID, int* picture, INT64* fileSize)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkItemExport - Start of Function");}
    if (!validType(nItemID,picture)) {return false;}

    if (extractInfo.checkParent){
        int result = checkParentType(nItemID);
        if (result != 0)
        {
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"XT_ProcessItem - Item excluded based on parent");}
            errorRaised(nItemID, REPORT_EXCLUDED_PARENT);
            return false;
        }
    }
    *fileSize = XWF_GetItemSize(nItemID);
    if (*fileSize < 0){
        wchar_t errMsg[256];
        swprintf(errMsg, 256, L"XWF_GetItemSize returned %lld in checkItemExport for itemID: %ld", *fileSize, nItemID);
        outputErrorMessage(errMsg);
        errorRaised(nItemID,REPORT_UNKNOWN_FILESIZE);
        return false;
    }
    if (!validFileSize(extractOpt,*fileSize,*picture)){
        errorRaised(nItemID,REPORT_EXCLUDED_FILESIZE);
        return false;
    }
    if (extractInfo.ignoreThumbs)
    {
        if (isThumbnailObject(nItemID)) {
            errorRaised(nItemID,REPORT_EXCLUDED_THUMBNAIL);
            return false;
        }
    }
    if (!isSelectedFileTypeStatus(nItemID))
    {
        errorRaised(nItemID,REPORT_EXCLUDED_TYPESTATUS);
        return false;
    }
    if (!isSelectedFileFormatStatus(nItemID))
    {
        errorRaised(nItemID,REPORT_EXCLUDED_FILECONSISTENCY);
        return false;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkItemExport - End of Function");}
    return true;
}



/**
 * @brief Retrieves hash values and writes output records for a qualifying media item.
 *
 * @param nItemID  X-Ways item ID.
 * @param picture  1 if picture, 0 if video.
 * @param fileSize File size in bytes.
 * @return 0 always.
 *
 * @see returnHashValue
 * @see writeOutputFile
 * @see updateRecords
 */

LONG mainItemProcess(LONG nItemID, int picture, INT64 fileSize)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"mainItemProcess Start");}
    hashValueStruct currHash;
    //start processing item
    int result = returnHashValue(nItemID,(wchar_t*)&currHash.MD5,(wchar_t*)&currHash.SHA1,(wchar_t*)&currHash.photoDNA);
    if (result == 0)
    {
        if (extractInfo.VICExport || extractInfo.C4ALLExport){
            int result = writeOutputFile(nItemID,picture,currHash.MD5,fileSize, hdlCurrVol);
            if (result != SUCCESS) {return 0;}
        }
        if (extractInfo.VICSCompressed)
        {//create compressed file here
            if (picture ==1){
                writeArchiveFile(nItemID,true,currHash.MD5,fileSize, hdlCurrVol);
            }
            else{
                writeArchiveFile(nItemID,false,currHash.MD5,fileSize, hdlCurrVol);
            }
        }
    }
    else{
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"mainItemProcess End");}
        return 0;
    }
    updateRecords(picture,nItemID, currHash);
    //reset completed flag to 0
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"mainItemProcess End");}
    return 0;
}

/**
 * @brief Creates XML and/or VICS database records for a processed media item and increments counters.
 *
 * @param picture   1 if picture, 0 if video.
 * @param nItemID   X-Ways item ID.
 * @param currHash  Hash values for the item.
 * @return 0 always.
 *
 * @see mainItemProcess
 */

int updateRecords(int picture, long nItemID, hashValueStruct currHash)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"updateRecords Start");}
    if (extractInfo.C4ALLExport)
    {
        EnterCriticalSection(&lockC4All);
        createC4AllRecord(nItemID,picture,currHash.MD5);
        LeaveCriticalSection(&lockC4All);
    }
    if (extractInfo.VICExport || extractInfo.VICSCompressed)
    {
        EnterCriticalSection(&lockVics);
        createVICSRecord(nItemID,picture,currHash);
        LeaveCriticalSection(&lockVics);
    }
    EnterCriticalSection(&updateLock);
    if (picture == 1)
    {
        pictureCount++;
        tmpPicCount++;
    }
    else
    {
        tmpVidCount++;
        movieCount++;
    }
    LeaveCriticalSection(&updateLock);
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"updateRecords End");}
    return 0;
}

/**
 * @brief Launches the Griffeye CLI to import VICS results into a new or existing case.
 *
 * @return 0 on success, 1 if the Griffeye executable was not found or CreateProcess failed.
 *
 * @see caseCleanup
 */
int createGriffeyeCase()
{
    if (extractInfo.debugSet){debugWriteDetails(0, L"createGriffeyeCase Start");}
    const char* griffeyeExe = findGriffeyeExe(extractOpt.GriffeyePath);
    if (griffeyeExe == NULL)
    {
        XWF_OutputMessage(L"Griffeye CLI executable not found, cannot create case",0);
        return 1;
    }
    char cmdOutput[8192];
    char tempString[1024];
    size_t pathLen = wcslen(extractOpt.GriffeyePath);
    bool pathHasSlash = (pathLen > 0 && extractOpt.GriffeyePath[pathLen-1] == L'\\');
    snprintf(cmdOutput,sizeof(cmdOutput),pathHasSlash ? "\"%ls%s\" import --case-folder \"%ls\" --name \"%ls\"" : "\"%ls\\%s\" import --case-folder \"%ls\" --name \"%ls\"",extractOpt.GriffeyePath,griffeyeExe,extractInfo.GriffeyeCaseLocation, extractInfo.GriffeyeCaseName);
    int sourceNo = 1;

    wchar_t path[32768] ={0};
    swprintf(path,32768,L"%ls\\%ls\\%ls.ANCF",extractInfo.GriffeyeCaseLocation, extractInfo.GriffeyeCaseName,extractInfo.GriffeyeCaseName);
    if (ifFileExistsW((wchar_t*)&path))
    {
        strncat(cmdOutput, " --add-source",sizeof(cmdOutput)-strlen(cmdOutput)-1);
    }

    if (extractInfo.extractPictures)
    {
        tempString[0] = '\0';
        snprintf(tempString,sizeof(tempString)," --source-id source%d --source-path \"%lsVICS_Pictures_Results.json\" --source-type vics --include-vics-data all",sourceNo++,extractInfo.C4PPath);
        strncat(cmdOutput,tempString,sizeof(cmdOutput)-strlen(cmdOutput)-1);
    }
    if (extractInfo.extractVideos)
    {
        tempString[0] = '\0';
        snprintf(tempString,sizeof(tempString)," --source-id source%d --source-path \"%lsVICS_Movies_Results.json\" --source-type vics --include-vics-data all", sourceNo++,extractInfo.C4MPath);
        strncat(cmdOutput,tempString,sizeof(cmdOutput)-strlen(cmdOutput)-1);
    }
    if (extractInfo.GriffeyeSettingsName != nullptr)
    {
        tempString[0] = '\0';
        snprintf(tempString,sizeof(tempString)," --import-settings-file %ls", extractInfo.GriffeyeSettingsName);
        strncat(cmdOutput,tempString,sizeof(cmdOutput)-strlen(cmdOutput)-1);
    }
    XWF_OutputMessage((wchar_t*)cmdOutput,4);
    PROCESS_INFORMATION ProcessInfo;
    STARTUPINFO StartupInfo;
    ZeroMemory(&StartupInfo, sizeof(StartupInfo));
    StartupInfo.cb = sizeof StartupInfo;
    if (CreateProcess(NULL,cmdOutput,NULL,NULL,FALSE,CREATE_NEW_PROCESS_GROUP,NULL,
                      NULL, &StartupInfo,&ProcessInfo))
    {
        //WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
        CloseHandle(ProcessInfo.hThread);
        CloseHandle(ProcessInfo.hProcess);
    }
    else
    {
        XWF_OutputMessage(L"Failed to create Griffeye case",0);
        if (extractInfo.debugSet){debugWriteDetails(0, L"createGriffeyeCase End - Return 1");}
        return 1;
    }
    if (extractInfo.debugSet){debugWriteDetails(0, L"createGriffeyeCase End - Return 0");}
    return 0;
}

/**
 * @brief Performs end-of-run cleanup: closes output files, writes VICS records, and launches Griffeye.
 *
 * @return 0 always.
 *
 * @see XT_Done
 */

int caseCleanup()
{
    if (extractInfo.debugSet){debugWriteDetails(0, L"caseCleanup Start");}
    for (int i=0;i<extractInfo.outputFileCounter;i++)
    {
        if (extractInfo.extractPictures)
        {
            closeXML(extractInfo.outputFiles[i].picOutput);

        }
        if (extractInfo.extractVideos)
        {
            closeXML(extractInfo.outputFiles[i].vidOutput);

        }
        firstTime = 0;
    }
    if (picResults) { fclose(picResults); picResults = NULL; }
    if (vidResults) { fclose(vidResults); vidResults = NULL; }
    if (extractInfo.VICExport || extractInfo.VICSCompressed)
    {
        outputVICSFile();
    }
    //used for debug
    if (extractInfo.debugSet)
    {
        wchar_t wSqlOutputPath[4096] = {0};
        swprintf(wSqlOutputPath, 4096, L"%ls%ls", extractOpt.errorReportPath, caseTitle);
        if (!dirExistsW(wSqlOutputPath))
        {
            CreateDirectoryW(wSqlOutputPath, NULL);
        }
        wcsncat(wSqlOutputPath, L"\\errorOutput.sqlite", 4095);
        char sqlOutputPath[4096] = {0};
        WideCharToMultiByte(CP_UTF8, 0, wSqlOutputPath, -1, sqlOutputPath, sizeof(sqlOutputPath), NULL, NULL);
        char dbPathMsg[4096+32] = {0};
        snprintf(dbPathMsg, sizeof(dbPathMsg), "Debug SQLite path: %s\r\n", sqlOutputPath);
        debugWriteDetails(dbPathMsg);
        loadOrSaveDb(vicsDB, sqlOutputPath, 1);
    }
    if (extractInfo.VICExport || extractInfo.VICSCompressed)
    {
        sqlite3_close(vicsDB);
        if (vicPicFile)   { fclose(vicPicFile);   vicPicFile   = NULL; }
        if (vicMovieFile) { fclose(vicMovieFile);  vicMovieFile = NULL; }
        if (extractInfo.VICSCompressed)
        {
            char filePath[2048]={0};
            if (extractInfo.extractPictures)
            {
                snprintf(filePath,2048,"%lsVICS_Pictures_Results.json",extractInfo.C4PPath);
                writeJSONFile(filePath,"VICS_Pictures_Results.json",true);
                DeleteFileA(filePath);
                filePath[0] = '\0';
            }
            if (extractInfo.extractVideos)
            {
                snprintf(filePath,2048,"%lsVICS_Movies_Results.json",extractInfo.C4MPath);
                writeJSONFile(filePath,"VICS_Movies_Results.json",false);
                DeleteFileA(filePath);
                filePath[0] = '\0';
            }
        }
    }
    if (extractInfo.createGriffeye)
    {
        int retVal = createGriffeyeCase();
    }
    //clear up extraction names
    if (extractInfo.nameList !=  NULL)
    {
        delete[] extractInfo.nameList;
        extractInfo.nameList = NULL;
    }
    if (extractInfo.VICSCompressed)
    {
        closeZipArchives();
    }
    cleanupArchivePaths();
    extractInfo.noNames = 0;
    freeVicsCaseData();
    if (extractInfo.C4PPath != NULL) {
        delete[] extractInfo.C4PPath;
        extractInfo.C4PPath = NULL;
    }
    if (extractInfo.C4MPath != NULL) {
        delete[] extractInfo.C4MPath;
        extractInfo.C4MPath = NULL;
    }
    //clean up Griffeye case variables
    if (extractInfo.GriffeyeCaseLocation != NULL) {
        delete[] extractInfo.GriffeyeCaseLocation;
        extractInfo.GriffeyeCaseLocation = NULL;
    }
    if (extractInfo.GriffeyeCaseName != NULL) {
        delete[] extractInfo.GriffeyeCaseName;
        extractInfo.GriffeyeCaseName = NULL;
    }
    if (extractInfo.GriffeyeSettingsName != NULL) {
        delete[] extractInfo.GriffeyeSettingsName;
        extractInfo.GriffeyeSettingsName = NULL;
    }
    clearReportTableDetails();
    if (currSrcID != NULL) { delete[] currSrcID; currSrcID = NULL; }
    cleanupGUI();
    cleanupOptions();
    if (extractInfo.debugSet)
    {
        debugWriteDetails(0, L"caseCleanup End");
        endDebugLog();
    }
    return 0;
}

// Hash Value Functions

/**
 * @brief Retrieves a hash value for the specified item from X-Ways.
 *
 * @param nItemID    X-Ways item ID.
 * @param hashValue  Output buffer for the hex hash string.
 * @param hashSize   Number of hex characters expected (32 for MD5, 40 for SHA1, 144 for PhotoDNA).
 * @param hashNumber Hash slot: 1 = primary, 2 = secondary, 3 = PhotoDNA.
 * @param forced     If TRUE, instructs X-Ways to compute the hash if not already done.
 * @return 0 on success, 1 on failure.
 */

int getHashValue(LONG nItemID, wchar_t* hashValue, int hashSize, int hashNumber, BOOL forced)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getHashValue Start");}
    BYTE hashBuffer[512] = {0};
    if (hashNumber == 1)
    {
        hashBuffer[0] = 0x1;
        hashBuffer[1] = 0x0;
    }
    else if (hashNumber == 2)
    {
        hashBuffer[0] = 0x2;
        hashBuffer[1] = 0x0;
    }
    else if (hashNumber == 3)
    {
        hashBuffer[0] = 0x0;
        hashBuffer[1] = 0x1;
    }
    hashBuffer[2] = 0x0;
    hashBuffer[3] = 0x0;
    if (forced)
    {
        memcpy(&hashBuffer[4],&nItemID, sizeof(LONG));
    }
    if (!XWF_GetHashValue(nItemID,hashBuffer))
    {
        if (extractInfo.debugSet)
        {
            debugWriteDetails(nItemID,L"GetFileDetails - Failed to get hash value");
        }
        return 1;
    }
    else
    {
        for (int i=0;i<(hashSize/2);i++)
        {
            swprintf(hashValue+(i*sizeof(wchar_t)),L"%02X",hashBuffer[i]);
        }
        hashValue[hashSize] = L'\0';
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getHashValue End");}
    return 0;
}

/**
 * @brief Reads a pre-computed primary or secondary hash (without forcing computation).
 *
 * @param nItemID    X-Ways item ID.
 * @param md5Buffer  Output buffer for the MD5 hex string.
 * @param SHA1Buffer Output buffer for the SHA1 hex string.
 * @param hashType   Hash slot to read (1 = primary, 2 = secondary).
 * @return 0 on success, 1 on failure.
 *
 * @see getHashValue
 */

int getSingleHash(LONG nItemID,wchar_t* md5Buffer, wchar_t* SHA1Buffer, int hashType)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getSingleHash Start");}
    int checkVal = 0;
    if (MD5Hash ==hashType)
    {
        //this is primary hash
        checkVal = getHashValue(nItemID,md5Buffer,32,MD5Hash,false);
    }
    else if (SHA1Hash == hashType)
    {
        checkVal = getHashValue(nItemID,SHA1Buffer,40,SHA1Hash,false);
    }
    if (checkVal !=0)
    {
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getSingleHash End - return 1");}
        return 1;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getSingleHash End Return 0");}
    return 0;
}

/**
 * @brief Forces X-Ways to compute and return a hash for an item that has not yet been hashed.
 *
 * @param nItemID    X-Ways item ID.
 * @param md5Buffer  Output buffer for the MD5 hex string.
 * @param SHA1Buffer Output buffer for the SHA1 hex string.
 * @param md5Type    Non-zero if MD5 should be forced.
 * @param sha1Type   Non-zero if SHA1 should be forced.
 * @return 0 on success, 1 on failure.
 *
 * @see getHashValue
 */

int forceHashes(LONG nItemID,wchar_t* md5Buffer, wchar_t* SHA1Buffer, int md5Type,int sha1Type)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"forceHashes Start");}
    //check if
    int checkVal = 0;
    if (md5Type !=0)
    {
        //this is primary hash
        checkVal = getHashValue(nItemID,md5Buffer,32,MD5Hash,true);
    }
    else if (sha1Type != 0)
    {
        checkVal = getHashValue(nItemID,SHA1Buffer,40,SHA1Hash,true);
    }
    if (checkVal !=0)
    {
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"forceHashes End Return 1");}
        return 1;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"forceHashes End Return 0");}
    return 0;
}

/**
 * @brief Returns MD5, SHA1, and PhotoDNA hashes for an item, forcing computation if needed.
 *
 * @param nItemID    X-Ways item ID.
 * @param md5Buffer  Output buffer for the MD5 hex string.
 * @param SHA1Buffer Output buffer for the SHA1 hex string.
 * @param PDNABuffer Output buffer for the base64-encoded PhotoDNA string.
 * @return 0 on success, non-zero if a required hash could not be retrieved.
 *
 * @see getSingleHash
 * @see forceHashes
 */

int returnHashValue(LONG nItemID, wchar_t* md5Buffer, wchar_t* SHA1Buffer, wchar_t* PDNABuffer)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"returnHashValue Start");}
    INT64 flags;
    BOOL complete=FALSE;
    flags = XWF_GetItemInformation(nItemID,3,&complete);
    if (!complete)
    {
        outputErrorMessage(L"XWF_GetItemInformation failed in returnHashValue for itemID: ", nItemID);
        return 4;
    }
    if (flags & 0x00040000)
    {
        //extract primary hash value
        int retVal = getSingleHash(nItemID,md5Buffer,SHA1Buffer,1);
        if (retVal !=0)
        {
            outputErrorMessage(L"Unable to retrieve primary hash for itemID: ",nItemID);
            EnterCriticalSection(&xwfOutputLock);
            recordError(vicsDB,ERROR_NO_MD5_HASH, nItemID, currSrcID);
            LeaveCriticalSection(&xwfOutputLock);
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"returnHashValue 1");}
            return 1;
        }
    }
    if (flags & 0x00100000)
    {
        //extract secondary hash value
        int retVal = getSingleHash(nItemID,md5Buffer,SHA1Buffer,2);
        if (retVal !=0)
        {
            outputErrorMessage(L"Unable to retrieve secondary hash for itemID: ",nItemID);
            EnterCriticalSection(&xwfOutputLock);
            recordError(vicsDB,ERROR_NO_MD5_HASH, nItemID, currSrcID);
            LeaveCriticalSection(&xwfOutputLock);
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"returnHashValue 2");}
            return 2;
        }
    }
    else
    {
        //no hash computed - can we compute?
        int retVal = forceHashes(nItemID,md5Buffer,SHA1Buffer, MD5Hash,SHA1Hash);
        if (retVal !=0)
        {
            //still failed
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"GetFileDetails - No hash computed for file");}
            errorRaised(nItemID,REPORT_NOHASH);
            EnterCriticalSection(&xwfOutputLock);
            recordError(vicsDB, ERROR_HASH_NOT_COMPUTED,nItemID,L"No hash computed for item");
            LeaveCriticalSection(&xwfOutputLock);
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"returnHashValue 3");}
            return 3;
        }
        else
        {
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"GetFileDetails - hash successfully forced");}
        }
    }
    //look at returning PhotoDNA hash here
    flags = XWF_GetItemInformation(nItemID,2,&complete);
    if (!complete)
    {
        outputErrorMessage(L"XWF_GetItemInformation failed in returnHashValue (PhotoDNA check) for itemID: ", nItemID);
    }
    else if (flags & 0x80000000)
    {
        //photoDNA Computed
        wchar_t tempBuffer[145]={0};
        int checkVal = getHashValue(nItemID,tempBuffer,144,hashTypePDNA,false);
        unsigned char* tBuffer = new unsigned char[145];
        sprintf((char*)tBuffer,"%ls",tempBuffer);
        char* result = b64Encode(tBuffer, strlen((char*)tBuffer));
        swprintf(PDNABuffer,L"%s",result);
        delete[] tBuffer;
        delete[] result;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"returnHashValue 0");}
    return 0;

}

// Writing VICS records

/**
 * @brief Writes all accumulated VICS records to the JSON output files.
 *
 * @return 0 on success, 1 if picture write failed, 2 if video write failed, 3 if both failed.
 *
 * @see caseCleanup
 */

int outputVICSFile()
{
    if (extractInfo.debugSet){debugWriteDetails(0, L"outputVICSFile Start");}
    int retVal = 0;
    XWF_ShowProgress(L"Collating VICS Records (May take some time)",1);
    time_t result= time(NULL);
    wchar_t strTime[128];
    swprintf(strTime,L"Starting JSON Creation: %s",asctime(localtime(&result)));
    XWF_OutputMessage(strTime,0);
    retVal = setupVicsExport();
    if (extractInfo.VICExport || extractInfo.VICSCompressed)
    {
        if (extractInfo.extractPictures)
        {
            int check = writeRecords(vicsDB,vicPicFile, 1);
            if (check != 0 )
            {
                XWF_OutputMessage(L"Error writing Picture VICS data",0);
                retVal = retVal | 0x01;
            }
        }
        if (extractInfo.extractVideos)
        {
            int check = writeRecords(vicsDB,vicMovieFile, 0);
            if (check != 0 )
            {
                XWF_OutputMessage(L"Error writing Movie VICS data",0);
                retVal = retVal | 0x02;
            }
        }
    }
    result= time(NULL);
    swprintf(strTime,L"JSON Creation Complete:%s",asctime(localtime(&result)));
    XWF_OutputMessage(strTime,0);
    XWF_HideProgress();
    outputErrorStats(vicsDB,versionNo);
    if (extractInfo.debugSet){debugWriteDetails(0, L"outputVICSFile End");}
    return retVal;
}

/**
 * @brief Inserts a VICS media hash record into the SQLite database.
 *
 * @param nItemID   X-Ways item ID.
 * @param hashVals  Hash values for the item.
 * @param picture   1 if picture, 0 if video.
 *
 * @see createVICSRecord
 */

void writeSQLMediaRecord(LONG nItemID, hashValueStruct hashVals, int picture)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"writeSQLMediaRecord Start");}
    VICSMedia currentRecord;
    initializeMediaRecord(currentRecord);
    //set up VICSMedia record variables
    wcscpy(currentRecord.MD5,hashVals.MD5);
    if (SHA1Hash !=0)
    {
        wcscpy(currentRecord.SHA1,hashVals.SHA1);
    }
    if (hashVals.photoDNA[0]!=L'\0')
    {
        wcscpy(currentRecord.PhotoDNA,hashVals.photoDNA);
    }
    if (picture==1)
    {
        currentRecord.MediaID = vicPicCounter++;
    }
    else
    {
        currentRecord.MediaID = vicMovieCounter++;
    }
    currentRecord.RelativeFilePath = new wchar_t[128];
    //edit to include new folder path
    char relativeBuffer[128]={0};
    int retVal = generateRelativeFilePath(&relativeBuffer[0],128,currentRecord.MD5,false);
    //merge paths
    swprintf(currentRecord.RelativeFilePath,L"%s\\%ls",relativeBuffer,currentRecord.MD5);
    INT64 sizeResult = XWF_GetItemSize(nItemID);
    if (sizeResult < 0)
    {
        wchar_t errMsg[256];
        swprintf(errMsg, 256, L"XWF_GetItemSize returned %lld in writeSQLMediaRecord for itemID: %ld", sizeResult, nItemID);
        outputErrorMessage(errMsg);
        sizeResult = 0;
    }
    currentRecord.MediaSize = sizeResult;
    insertMediaRecord(vicsDB,currentRecord, picture);
    deallocateMediaRecord(currentRecord);
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"writeSQLMediaRecord End");}
}


/**
 * @brief Returns the physical byte offset of an item.
 *
 * @param nItemID    X-Ways item ID.
 * @param unallocated Output: set to true if the item is in unallocated space.
 * @param deleted     Output: set to true if the item has no file record.
 * @return Physical offset in bytes, or 0 if not available.
 */

INT64 getPhysicalOffset(DWORD nItemID, BOOL* unallocated, BOOL* deleted)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getPhysicalOffset Start");}
    INT64 PhysOffset=0;
    INT64 StartSector = 0;
    INT64 retValue=0;
    XWF_GetItemOfs(nItemID,&PhysOffset,&StartSector);
    if (PhysOffset<-1)
    {
        //no file record, can't be live. Set deleted and Unallocated flags
        *deleted = true;
        *unallocated = true;
        retValue = PhysOffset * -1;
    }
    else if(PhysOffset == 0 || PhysOffset == 0xFFFFFFFF)
    {
        retValue = 0;
    }
    else
    {
        retValue = StartSector * 512;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getPhysicalOffset End");}
    return retValue;
}

/**
 * @brief Returns the physical byte offset of an item without setting deleted/unallocated flags.
 *
 * @param nItemID X-Ways item ID.
 * @return Physical offset in bytes, or 0 if not available.
 */
INT64 getPhysicalOffset(DWORD nItemID)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getPhysicalOffset Start");}
    INT64 PhysOffset=0;
    INT64 StartSector = 0;
    INT64 retValue=0;
    XWF_GetItemOfs(nItemID,&PhysOffset,&StartSector);
    if (PhysOffset<-1)
    {
        //no file record, can't be live. Set deleted and Unallocated flags
        retValue = PhysOffset * -1;
    }
    else if(PhysOffset == 0 || PhysOffset == 0xFFFFFFFF)
    {
        retValue = 0;
    }
    else
    {
        retValue = StartSector * 512;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getPhysicalOffset End");}
    return retValue;
}


/**
 * @brief Reads a file timestamp from X-Ways and stores it in a FILETIME struct.
 *
 * Timestamps that exceed a sanity threshold are treated as invalid and left unchanged.
 *
 * @param timeValue Output FILETIME to populate.
 * @param nItemID   X-Ways item ID.
 * @param time_type X-Ways time info index (32=created, 33=written, 34=accessed).
 * @return 0 always.
 */
int getFileTimestamp(FILETIME* timeValue, long nItemID, int time_type)
{
    INT64 timeTmp = 0;
    FILETIME tmpFileTime;
    BOOL bSuccess;
    timeTmp = XWF_GetItemInformation(nItemID,time_type,&bSuccess);
    if (bSuccess){
        if (timeTmp > 145452016110000000) {
                timeTmp = 0;
        }
        else{
            memcpy(&tmpFileTime,&timeTmp,sizeof(tmpFileTime));
            *timeValue = tmpFileTime;
        }
    }
    else{
        //maybe add debug message. Some issues here
        if (extractInfo.debugSet){
            wchar_t output_message[1024];
            swprintf(output_message,L"Error extracting timestamp: %i",time_type);
            debugWriteDetails(nItemID, output_message);
        }
    }
    return 0;
}

/**
 * @brief Returns true if the item has a deleted status in X-Ways.
 *
 * @param nItemID     X-Ways item ID.
 * @param unallocated Output: set to TRUE if the item is in unallocated space (deletion code 5).
 * @return true if deleted, false if live.
 */
bool getDeletedStatus(long nItemID, BOOL* unallocated)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getDeletedStatus Start");}
    BOOL boolCheck=0;
    INT64 delStatus = XWF_GetItemInformation(nItemID,4,&boolCheck);
    if (!boolCheck)
    {
        outputErrorMessage(L"XWF_GetItemInformation failed in getDeletedStatus for itemID: ", nItemID);
        return false;
    }
    if (delStatus == 0)
    {
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getDeletedStatus return false");}
        return false;
    }
    else
    {
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getDeletedStatus return true");}
        if (delStatus == 5)
        {
            *unallocated = true;
        }
        else
        {
            *unallocated = false;
        }
        return true;
    }
}
/**
 * @brief Retrieves the item filename from X-Ways and stores it in a VICSMediaFile record.
 *
 * Falls back to "-noName-" if the filename is empty. Removes invalid characters in-place.
 *
 * @param nItemID X-Ways item ID.
 * @param record  Output record whose fileName field is populated.
 */
void getItemFileName(long nItemID, VICSMediaFile* record)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getItemFileName Start");}
    LPWSTR xName;
    try{
        xName = (LPWSTR)XWF_GetItemName(nItemID);
    }
    catch (...)
    {
        XWF_OutputMessage(L"Error retrieving item name. Item will have --noName--",0);
        xName = new wchar_t[8];
        xName[0] = L'\0';
    }
    int nameLen = wcslen(xName);
    if (nameLen == 0)
    {
        nameLen = wcslen(L"-noName-");
        record->fileName = new wchar_t[nameLen + 1];
        wcscpy(record->fileName,L"-noName-");
    }
    else
    {
        removeInvalidChars(xName);
        record->fileName = new wchar_t[nameLen + 1];
        wcscpy(record->fileName,xName);
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getItemFileName End");}
}

/**
 * @brief Populates a VICSMediaFile record with metadata for the given item.
 *
 * @param nItemID  X-Ways item ID.
 * @param MD5Hash  MD5 hex string for the item.
 * @param picture  1 if picture, 0 if video.
 * @param record   Output struct to populate.
 * @return 0 always.
 *
 * @see createVICSRecord
 */

int extractMediaFileRecordDetails(LONG nItemID,wchar_t MD5Hash[33], int picture, VICSMediaFile* record)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"extractMediaFileRecordDetails Start");}
    wcscpy(record->MD5,MD5Hash);
    try{
        record->filePath = getFullPath(currSrcID,nItemID, true);
    }
    catch (...){
        //shit hit fan!
        XWF_OutputMessage(L"Error retrieving fullpath. File path replaced with \'Error\'",0);
        record->filePath = new wchar_t[16];
        wcscpy(record->filePath,L"Error");
    }
    getItemFileName(nItemID,record);
    //MAC times
    getFileTimestamp(&record->created,nItemID,32);
    getFileTimestamp(&record->written,nItemID,33);
    getFileTimestamp(&record->accessed,nItemID,34);
    //get deleted status
    record->deleted = getDeletedStatus(nItemID, &record->unallocated);
    //getPhysical Sector
    record->physicalLocation = getPhysicalOffset(nItemID,&record->unallocated,&record->deleted);
    //add source
    record->sourceID = new wchar_t[128];
    record->sourceID[0] = L'\0';
    swprintf(record->sourceID,L"%ls",currSrcID);
    record->XWFitemID = nItemID;
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"extractMediaFileRecordDetails End");}
    return 0;
}

/**
 * @brief Extracts media file metadata and inserts a VICSMediaFile record into the database.
 *
 * @param nItemID  X-Ways item ID.
 * @param MD5Hash  MD5 hex string for the item.
 * @param picture  1 if picture, 0 if video.
 *
 * @see createVICSRecord
 */

void writeSQLMediaFileRecord(LONG nItemID,wchar_t MD5Hash[33], int picture)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"writeSQLMediaFileRecord Start");}
    VICSMediaFile currentMedia;
    initializeMediaFileRecord(currentMedia);
    extractMediaFileRecordDetails(nItemID, MD5Hash, picture, &currentMedia);
    insertMediaFileRecord(vicsDB,currentMedia, picture);
    //clear up memory
    deallocateMediaFileRecord(currentMedia);
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"writeSQLMediaFileRecord End");}
}

/**
 * @brief Inserts a VICS media metadata (property name/value) record into the database.
 *
 * Skips insertion if an identical record already exists.
 *
 * @param nItemID        X-Ways item ID.
 * @param MD5Hash        MD5 hex string for the item.
 * @param PropertyName   Null-terminated property name.
 * @param PropertyValue  Null-terminated property value.
 *
 * @see createVICSRecord
 */

void writeSQLMediaMetadataRecord(LONG nItemID,wchar_t MD5Hash[33], wchar_t* PropertyName, wchar_t* PropertyValue)
{
    //check if record already exists
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"writeSQLMediaMetadataRecord Start");}
    int recExists = existsMediaMetadata(vicsDB,MD5Hash, PropertyName);
    if (recExists == 1){
        //already exists
        return;
    }
    if (recExists < 0)
    {
        //error
        outputErrorMessage(L"Error checking if Metadata Record exists",nItemID);
        return;
    }
    VICSMediaMetadata currentMedia;
    wcscpy(currentMedia.MD5,MD5Hash);
    int NameLength = wcslen(PropertyName);
    currentMedia.PropertyName = new wchar_t[NameLength+2];
    wcsncpy(currentMedia.PropertyName,PropertyName, NameLength+1);
    int ValueLength = wcslen(PropertyValue);
    currentMedia.PropertyValue = new wchar_t[ValueLength+2];
    wcsncpy(currentMedia.PropertyValue,PropertyValue, ValueLength+1);
    int result = insertMediaMetadataRecord(vicsDB,currentMedia);
    deallocateMediaMetadataRecord(currentMedia);
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"writeSQLMediaMetadataRecord End");}
}

/**
 * @brief Returns true if the new item should replace the existing duplicate in the VICS record.
 *
 * Compares deletion flags: an item with a higher deletion status is considered less preferred.
 *
 * @param origID X-Ways item ID of the existing record.
 * @param newID  X-Ways item ID of the candidate replacement.
 * @return true if @p newID should replace @p origID, false otherwise.
 */
bool replaceOriginalItem(LONG origID, LONG newID)
{
    if (extractInfo.debugSet){debugWriteDetails(0, L"replaceOriginalItem Start");}
    BOOL origSuccess = FALSE, newSuccess = FALSE;
    INT64 originalDeletedFlags = XWF_GetItemInformation(origID, XWF_ITEM_INFO_DELETION, &origSuccess);
    INT64 newDeletedFlags = XWF_GetItemInformation(newID, XWF_ITEM_INFO_DELETION, &newSuccess);
    if (!origSuccess)
    {
        outputErrorMessage(L"XWF_GetItemInformation failed in replaceOriginalItem for itemID: ", origID);
        return false;
    }
    if (!newSuccess)
    {
        outputErrorMessage(L"XWF_GetItemInformation failed in replaceOriginalItem for itemID: ", newID);
        return false;
    }
    if (originalDeletedFlags > newDeletedFlags)
    {
        if (extractInfo.debugSet){debugWriteDetails(0, L"replaceOriginalItem End - True");}
        return true;
    }
    else
    {
        if (extractInfo.debugSet){debugWriteDetails(0, L"replaceOriginalItem End - False");}
        return false;
    }
}

/**
 * @brief Creates or updates all VICS SQLite records for a processed media item.
 *
 * Handles deduplication by physical offset and MD5, and optionally writes device-type and
 * report-table metadata records.
 *
 * @param nItemID  X-Ways item ID.
 * @param picture  1 if picture, 0 if video.
 * @param hashVals Hash values for the item.
 * @return 0 always.
 *
 * @see updateRecords
 */

int createVICSRecord(LONG nItemID, int picture, hashValueStruct hashVals)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"createVICSRecord Start");}
    INT64 recordID = getVicsRecord(vicsDB, hashVals.MD5, picture);
    if (recordID == 0){
        //needs adding
        writeSQLMediaRecord(nItemID, hashVals, picture);
    }
    INT64 offset = getPhysicalOffset(nItemID);
    long duplicateItemID = 0;
    int duplicate = checkDuplicateFile(vicsDB, offset, hashVals.MD5, currSrcID, &duplicateItemID, picture);
    if (duplicate ==1){
        //is a duplicate, see which one to keep!
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Duplicate Item Located");}
        if (replaceOriginalItem(duplicateItemID,nItemID))
        {
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Duplicate Item Located - to be replaced");}
            errorRaised(duplicateItemID,REPORT_EXCLUDED_DUPLICATE);
            VICSMediaFile recUpdate;
            initializeMediaFileRecord(recUpdate);
            int rc = extractMediaFileRecordDetails(nItemID,hashVals.MD5,picture,&recUpdate);
            updateMediaFileRecord(vicsDB,&recUpdate,picture,hashVals.MD5, duplicateItemID);
        }
        else
        {
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Duplicate Item Located - not to replaced");}
            errorRaised(nItemID,REPORT_EXCLUDED_DUPLICATE);
        }
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"createVICSRecord End- Duplicate");}
        return 0;
    }
    //if not duplicate (will have returned) add media record
    writeSQLMediaFileRecord(nItemID, hashVals.MD5, picture);

    if (DeviceTypeCol != -1)
    {
        //check if device type column is blank
        wchar_t buffer[128] ={0};
        LONG result = XWF_GetCellText(nItemID,NULL,0,DeviceTypeCol,(wchar_t*)&buffer,127);
        if (extractInfo.debugSet)
        {
            wchar_t dbgMsg[200] = {0};
            swprintf(dbgMsg, 200, L"Device Type value: '%ls'", buffer);
            debugWriteDetails(nItemID, dbgMsg);
        }
        if (result < 0)
        {
            //error
            outputErrorMessage(L"Unable to get Device Type Column Data for Item: ",nItemID);
        }
        else{
            if (buffer[0] != L'\0' &&
                _wcsicmp(buffer, L"unknown")      != 0 &&
                _wcsicmp(buffer, L"undetermined") != 0 &&
                _wcsicmp(buffer, L"no device")    != 0)
            {
                writeSQLMediaMetadataRecord(nItemID, hashVals.MD5, L"Device Type",(wchar_t*)buffer);
            }
            else if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Device Type blank or non-informative value, skipping");}
        }
    }
    else if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Device Type column unidentified");}

    if (extractInfo.exportReportTables){
        wchar_t* buffer = retrieveUserReportTableAssociations(nItemID);
        if (buffer!= nullptr)
        {
            writeSQLMediaMetadataRecord(nItemID, hashVals.MD5, L"XWF Report Table",(wchar_t*)buffer);
            delete[] buffer;
        }
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"createVICSRecord End");}
    return 0;
}

// C4All XML Records

/**
 * @brief Creates and writes a C4All XML record for a media item.
 *
 * @param nItemID  X-Ways item ID.
 * @param picture  1 if picture, 0 if video.
 * @param MD5Hash  MD5 hex string for the item.
 * @return 0 always.
 *
 * @see updateRecords
 */

int createC4AllRecord(LONG nItemID, int picture,wchar_t MD5Hash[33])
{
    FileRecord picFile;
    BOOL done =  FALSE;
    wcscpy(picFile.hashValue,MD5Hash);
    //get file times
    picFile.createdTime = XWF_GetItemInformation(nItemID,32,&done);
    if (picFile.createdTime != 0)
    {
            picFile.createdTime = filetime2Unix(picFile.createdTime);
    }
    done = FALSE;
    picFile.modifiedTime = XWF_GetItemInformation(nItemID,33,&done);
    if (picFile.modifiedTime != 0)
    {
            picFile.modifiedTime = filetime2Unix(picFile.modifiedTime);
    }
    done = FALSE;
    picFile.accessedTime = XWF_GetItemInformation(nItemID,34,&done);
    if (picFile.accessedTime != 0)
    {
            picFile.accessedTime = filetime2Unix(picFile.accessedTime);
    }
    done = FALSE;
    picFile.deletionTime = XWF_GetItemInformation(nItemID,36,&done);
    if (picFile.deletionTime != 0)
    {
            picFile.deletionTime = filetime2Unix(picFile.deletionTime);
    }
    done = FALSE;
    //get description
    INT64 delStatus = XWF_GetItemInformation(nItemID,4,&done);
    if (done == FALSE)
        {
            outputErrorMessage(L"Error getting file deleted status for ID:",nItemID);
            picFile.description[0]=L'\0';
            wcscpy(picFile.description,L"Error");
        }
    else
    {
        picFile.description[0]=L'\0';
        if (delStatus > max_deletion || delStatus < 0)
        {
            wcscpy(picFile.description,L"Unknown Type");
            outputErrorMessage(L"Unknown deletion type returned", delStatus);
        }
        else
        {
            wcscpy(picFile.description,INFO_DELETION[delStatus]);
        }
    }
    //get filename/path
    picFile.fullPath = getFullPath(currSrcID,nItemID,false);
    removeInvalidChars(picFile.fullPath);
    picFile.fullPath = replaceInvalidXMLChars(picFile.fullPath);
    //get physical location and offset
    INT64 ds;
    XWF_GetItemOfs(nItemID,(INT64*)&ds,(INT64*)&picFile.physicalSector);
    INT64 fileSizeResult = XWF_GetItemSize(nItemID);
    if (fileSizeResult < 0)
    {
        wchar_t errMsg[256];
        swprintf(errMsg, 256, L"XWF_GetItemSize returned %lld in createC4AllRecord for itemID: %ld", fileSizeResult, nItemID);
        outputErrorMessage(errMsg);
        fileSizeResult = 0;
    }
    picFile.fileSize = fileSizeResult;
    if (picture == 1)
    {
        writeXML(picFile,picture,currPicFile,picCount);
    }
    else
    {
        writeXML(picFile,picture,currVidFile,vidCount);
    }
    if (picture ==1)
    {
        picCount++;
    }
    else
    {
        vidCount++;
    }
    //clear up records
    delete[] picFile.fullPath;
    return 0;
}

// X-Ways Utility Functions

/**
 * @brief Retrieves the filename for an item, sanitising newlines and backslashes.
 *
 * @param evObject   Evidence object handle (unused, retained for API symmetry).
 * @param nItemID    X-Ways item ID.
 * @param retValue   Output buffer for the filename.
 * @param bufferSize Size of @p retValue in wide characters.
 * @return 0 always.
 *
 * @see createC4AllRecord
 */

int getFileName(LPWSTR evObject,LONG nItemID, wchar_t* retValue,long bufferSize)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Start of getFileName Function Output");}
    LPWSTR fileName = (LPWSTR)XWF_GetItemName(nItemID);
    if (fileName == NULL)
    {
        outputErrorMessage(L"XWF_GetItemName returned NULL for itemID: ", nItemID);
        swprintf(retValue, bufferSize, L"*NoName*");
        return 0;
    }
    int nameLen = wcslen(fileName);
    if (nameLen == 0)
    {
        //no name
        swprintf(retValue,L"*NoName*");
    }
    else
    {
        for (unsigned int i = 0;i<wcslen(fileName);i++)
        {
            //clean up filename string
            if (fileName[i] == L'\n' || fileName[i] == L'\r' )
            {
                fileName[i] = L'\0';
            }
            else if (fileName[i] == L'\\')
            {
                fileName[i] = L'|';
            }
        }
        swprintf(retValue,bufferSize,L"%ls",fileName);
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"End of getFileName Function Output");}
    return 0;
}

/**
 * @brief Returns the full path for an item, formatted for XML or VICS output.
 *
 * XML paths include the filename; VICS paths contain only the parent directory with
 * backslashes replaced by double-backslashes and pipe characters.
 *
 * @param evObject Evidence object wide string (used as path prefix).
 * @param nItemID  X-Ways item ID.
 * @param isVic    TRUE for VICS format, FALSE for XML format.
 * @return Newly allocated wide string containing the path (caller must delete[]).
 */

wchar_t* getFullPath(LPWSTR evObject,LONG nItemID, BOOL isVic)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Start of getFullPath Function Output");}
    wchar_t* retValue = new wchar_t[8192];
    wchar_t* temp = new wchar_t[8192];
    temp[0]=L'\0';
    retValue[0] = L'\0';
    LONG parent;
    //if we are putting data in XML, we include filename
    //in VICS format, we just provide the path (minus filename).
    if (!isVic){
        int error = getFileName(evObject,nItemID,retValue,8192);
    }
    else{
        swprintf(retValue,8192,L"");
    }
    parent = XWF_GetItemParent(nItemID);
    do
    {
        if (parent != -1)
        {
            LPWSTR nameParent = (LPWSTR)XWF_GetItemName(parent);
            if (nameParent == NULL)
            {
                outputErrorMessage(L"XWF_GetItemName returned NULL for itemID: ", parent);
                nameParent = L"(Unknown)";
            }
            if (wcscmp(nameParent,L"(Root directory)")!=0)
            {
                if (isVic)
                {
                    int nameLen = wcslen(nameParent);
                    wchar_t* newName= new wchar_t[nameLen + 2];
                    for (int j = 0;j<nameLen;j++)
                    {
                        if (nameParent[j] != L'\\')
                        {
                            newName[j] = nameParent[j];
                        }
                        else
                        {
                            newName[j] = L'|';
                        }
                    }
                    newName[nameLen]=L'\0';
                    int chkLen = wcslen(retValue);
                    if (chkLen > 0)
                    {
                        swprintf(temp,8192,L"%ls\\%ls",newName,retValue);
                    }
                    else
                    {
                        //if retValue is "" creates an error.
                        swprintf(temp,8192,L"%ls\\",newName);
                    }
                    delete[] newName;
                }
                else
                {
                    swprintf(temp,8192,L"%ls\\%ls",nameParent,retValue);
                }
            }
            else
            {
                if (isVic)
                {
                    int chkLen = wcslen(retValue);
                    if (chkLen > 0)
                    {
                        swprintf(temp,8192,L"\\%ls",retValue);
                    }
                    else
                    {
                        swprintf(temp,8192,L"\\");
                    }
                }
                else
                {
                    swprintf(temp,8192,L"\\%ls",retValue);
                }
            }
            swprintf(retValue,8192,L"%ls",temp);
            parent = XWF_GetItemParent(parent);
        }
    } while (parent != -1);
    //prepend with evidence object name
    HANDLE hEvidence = XWF_GetEvObj(currEvidence);
    if (hEvidence == NULL)
    {
        outputErrorMessage(L"XWF_GetEvObj returned NULL in getFullPath for itemID: ", nItemID);
        delete[] temp;
        return retValue;
    }
    LPWSTR partName = (LPWSTR)XWF_GetEvObjProp(hEvidence,6,NULL);
    if (partName != NULL)
    {
        if (isVic)
        {
            //swprintf(retValue,L"%ls\\\\%ls%ls",currSrcID,partName,temp);
            swprintf(retValue,8192,L"%ls%ls",partName,temp);
        }
        else
        {
            swprintf(retValue,8192,L"%ls\\%ls%ls",currSrcID,partName,temp);
        }
    }
    else
    {
        //error
        XWF_OutputMessage(L"Error obtaining evidence item name while creating path",0);
    }
    delete[] temp;
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"End of getFullPath Function Output");}
    return retValue;
}


// VICS Record Export

/**
 * @brief Populates a VICSRecord with all media file and metadata entries matching a hash value.
 *
 * @param database   SQLite database handle.
 * @param record     Output record to populate.
 * @param hashValue  Null-terminated MD5 hash to match.
 * @param picture    1 for pictures, 0 for videos.
 * @return 0 on success, -1 on database error.
 *
 * @see writeRecords
 */

int extractIntoVicsRecord(sqlite3* database, VICSRecord* record, wchar_t* hashValue ,int picture)
{
    sqlite3_stmt* statement = NULL;
    //we need to extract media files
    int result = returnMediaFileRecords(database, &statement, picture, hashValue);
    if (result < 0){
        //error
        sqlite3_finalize(statement);
        return -1;
    }
    //set up space for records
    record->vMediaFiles = new VICSMediaFile[result];
    record->currentMaxMediaFiles = result;
    record->noMediaFiles = result;
    int sqlResult = 0;
    for (int i = 0;i<result;i++)
    {
        //cycle through the records and add them to main record
        extractVICSMediaFileSQL(record->vMediaFiles[i],statement);
        sqlResult = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);

    //add any media metadata records
    sqlite3_stmt* metaStatement = NULL;
    result = returnMediaMetadataRecords(database, &metaStatement, hashValue);
    if (result < 0){
        //error
        sqlite3_finalize(metaStatement);
        return -1;
    }
    //set up space for records
    record->vMediaMetaData = new VICSMediaMetadata[result];
    record->currentMaxMediaMetadata = result;
    record->noMediaMetadata = result;
    sqlResult = 0;
    for (int i = 0;i<result;i++)
    {
        //cycle through the records and add them to main record
        extractVICSMediaMetadataSQL(&record->vMediaMetaData[i],metaStatement);
        sqlResult = sqlite3_step(metaStatement);
    }
    if (result > 0) sqlite3_finalize(metaStatement);

    return 0;
}



/**
 * @brief Writes all VICS records from the database to the supplied JSON output file.
 *
 * @param database  SQLite database handle.
 * @param vicFile   Open FILE pointer to the VICS JSON output file.
 * @param picture   1 for picture records, 0 for video records.
 * @return 0 on success, -1 on database error.
 *
 * @see outputVICSFile
 */
int writeRecords(sqlite3* database,FILE* vicFile, int picture)
{
    sqlite3_stmt *statement;
    //get number of records returned
    int noRecords = returnMediaRecords(vicsDB,&statement,picture);
    if (noRecords < 0){
        //error
        return -1;
    }
    else if (noRecords == 0)
    {
        //no records to process
        sqlite3_finalize(statement);
        closeVICSFile(vicFile);
        return 0;
    }
    for (int i=0;i<noRecords;i++)
    {
        VICSRecord currentRecord;
        extractVICSMediaSQL(currentRecord.vMedia,statement);
        int result = extractIntoVicsRecord(vicsDB,&currentRecord,(wchar_t*)&currentRecord.vMedia.MD5,picture);
        if (result == 0){
            //success now write record
            writeMediaRecord(vicFile, &currentRecord);
        }
        if (i != noRecords -1){
            //not last record, add seperator
            fprintf(vicFile,",\r\t\t");
        }
        //update screen every 100 records
        if (i % 100==0) {XWF_ShouldStop();}
        //deallocate memory for record and move to next
        deallocateVICSRecord(currentRecord);
        sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    closeVICSFile(vicFile);
    return 0;
}

