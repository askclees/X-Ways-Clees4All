#include <cwchar>
#include <cstdio>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <ctime>
#include <string>
#include <windows.h>
#include <mutex>
#include <shlobj.h>
#include <climits>
#include <map>
#include <excpt.h>
#include <synchapi.h>

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
#include "options.h"
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

//1.38 defining check file type code
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
std::mutex lockC4All, lockVics, updateLock, xwfOutputLock;
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
const wchar_t* progVersion = L"1.60";
const wchar_t* INFO_DELETION[] =    {L"Existing",
                                    L"Previously existing, possibly recoverable",
                                    L"Previously existing, first cluster overwritten or unknown",
                                    L"Renamed/moved, possibly recoverable",
                                    L"Renamed/moved, first cluster overwritten or unknown",
                                    L"Carved File"};
const int max_deletion = 5;

//prototyping
INT64 Filetime2Unix(INT64 fTime);
wchar_t* getFullPath(LPWSTR evObject,LONG nItemID, BOOL isVIC);
int createC4POutput();
int writeOutputFile(LONG nItemID,int picFile,wchar_t* fileName,INT64 fileSize, HANDLE hItem);
void removeInvalidChars(wchar_t* strIn);

int createC4AllRecord(LONG nItemID, int picture, wchar_t MD5Hash[33]);
LONG MainItemProcess(LONG nItemID, int picture, INT64 fileSize);
int UpdateRecords(int picture, long nItemID,hashValueStruct currHash);

int createVICSRecord(LONG nItemID, int picture,hashValueStruct hashVals);

void fillItemDetails();
int getCaseOptions();
void createSQLNameList(HANDLE evObj);
int returnHashValue(LONG nItemID, wchar_t* md5Buffer, wchar_t* SHA1Buffer, wchar_t* PDNABuffer);
wchar_t* replaceInvalidXMLChars(wchar_t* strIn);
int getCommandLineOptions();
void addCaseDetail(wchar_t* strArg);
void freeVicsCaseData();

//1.41 added prototyping after reshuffle of code
int determineHashTypes();
int determineColumnNumber(wchar_t* compareStr);
int checkItemType(LONG nItemID, int* picture);
int checkParentType(LONG nItemID);
int caseCleanup();

//1.50 added function from Prepare
int firstRunSetup();
int volumePrepare(HANDLE hEvidence);

//1.50 added dedicated function to check if file is exported
bool checkItemExport(LONG nItemID, int* picture, INT64* fileSize);

//VCIS Stuff

//int writeRecords(FILE* vicFile, int picture);
int outputVICSFile();
int writeRecords(sqlite3* database,FILE* vicFile, int picture);
int writeMediaRecord(FILE* vicFile, VICSRecord &record);

//1.41 added Device Type column number
int DeviceTypeCol = -1;

//1.50 added get physical offset function
INT64 getPhysicalOffset(DWORD nItemID);
INT64 getPhysicalOffset(DWORD nItemID, BOOL* unallocated, BOOL* deleted);

BOOL APIENTRY DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	extractInfo.thisDLL = hInstDLL;
	GetModuleFileNameW(hInstDLL, XT_PATH, MAX_PATH);
    return TRUE;
}


/*Section: X-Ways Functions*/

/*Function: XT_Init
    Init Function of X-tension, called when X-Tension loaded
    Checks version of X-Ways and decides if it can be used
    Also loads options from SQLite Database

    Returns:
        -1 - Prevent use of DLL
        1 - If X-Tension is Thread Safe
        2 - If X-Tension is not Thread Safe

    See Also:
        Calls   -   <loadOrCreateOptions>
*/
LONG DLL_EXPORT XT_Init(CallerInfo info, DWORD nFlags, HANDLE hMainWnd, void* lpReserved)
{
    if (extractInfo.debugSet){debugWriteDetails(0,L"XT_Init Start");}
    //required if testing command line to set debugger
    //MessageBox(NULL,"Test","Test",MB_OK);
    XT_RetrieveFunctionPointers();
    int retVal = SQLInit();
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
        //1.50 updated message //1.41 - Notify that Device type detection unavailable
        XWF_OutputMessage(L"X-Ways version below 20.3, some features are disabled",0);
        XWF_OutputMessage(L"Specifically Device type detection is disabled",0);
    }
    else if (info.version < 2050)
    {
        //1.50 updated message //1.41 - Notify that Device type detection unavailable
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

    //1.50 need to extract last run settings after default set
    loadLastExtractionSettings(&extractInfo);
    if (extractInfo.debugSet){debugWriteDetails(0,L"XT_Init End");}
    return 2;
}

/*Function: XT_About
    Calls the options window creation function

    Returns:
        0 - Always returns

    See Also:
        Calls   -   <createOptionsWindow>
*/
LONG DLL_EXPORT XT_About(HANDLE hParentWnd, PVOID lpReserved)
{
    //setup options
    createOptionsWindow();
    return 0;
}


/*Function: XT_Prepare
    Called at the start of every new volume being processed.
    If its the first time XT_Prepare has been called, it displays the menu.

    Returns:
        -4  -   Stops whole operation if VICS Setup fails
        -3  -   Prevents use of X-Tension if not run from RVS
        3   -   XT_PREPARE_CALLPI | XT_PREPARE_CALLPILATE

    See Also:
        Calls   -   <determineHashTypes>, <setupVics>, <getCaseOptions>, <getFileNumber>

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
        //1.50 moved this section to a separate function.
        int firstRun = firstRunSetup();
        firstTime = 1;
        if (firstRun != 0) { return firstRun;}
    }

    //1.50 run volume prepare function
    int result = volumePrepare(hEvidence);
    if (extractInfo.debugSet){debugWriteDetails(0,L"XT_Prepare End");}
    return 0x03;
}


/*Function: XT_ProcessItem
    Main worker function, first checks that item is of type media

    Then checks if file is excluded as embedded in video file (if option to do so selected)

    Then calls mainItemProcess if its deemed to be a file of interest

    Returns:
        0 - Always
        -1 - would stop operation (RVS) - *NEVER RETURNED*
        -2 - would skip refinement for this item - *NEVER RETURNED*

    See Also:
        Calls   -   <MainItemProcess>
*/
LONG DLL_EXPORT XT_ProcessItem(LONG nItemID, void* lpReserved)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"XT_ProcessItem Start");}
    int picture;
    INT64 fileSize;
    if (checkItemExport(nItemID,&picture,&fileSize)){
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"XT_ProcessItem - Valid Item");}
        return MainItemProcess(nItemID, picture, fileSize);
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"XT_ProcessItem End");}
    return 0;
}


/*Function: XT_Finalize
    Function that is invoked at the end of processing an x-ways evidence object (partition for example).

    Resets counters and writes numbers of media extracted from devices to text file.

    Returns:
        0 - Always

    See Also:
        Calls   -   None
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


/*Function: XT_Done

    Called by X-Ways before X-Tension is unloaded.

    Call case cleanup and error reporting functions

    Returns:
        0   - Always

    See Also:
        Calls   -   <caseCleanup>, <errorReport>
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

/*Section: Initial Setup Functions
    This section contains functions that set up information or files required for the extraction.

    This includes creation of XML/VICS files and getting case options

*/

/*Function: firstRunSetup
    Setup case when X-Tension is first run. Only called on first XT_Prepare call of run.

    Created in 1.50, split from <XT_Prepare>

    Returns:
        0 - Always

    See Also:
        Called by   -   <XT_Prepare>
        Calls       -   <setupVics>, <getCaseOptions>
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
    if (strLen > 64)
    {
        XWF_OutputMessage(L"Case Title is bigger than 64 chars, truncated",0);
    }
    writeExtractionDetails(extractInfo);
    result = identifyReportTables();
    if (extractInfo.debugSet){debugWriteDetails("firstRunSetup Function End");}
    return 0;
}


/*Function: createC4POutput
    Creates two XML output file locations, one for pictures and the other for videos
    Files only created if option to export that file type is actually ticked.

    Called from <getCaseOptions>

    Maybe should be included in XML file

    Returns:
        0 - Always

    See Also:
        Calls   -   <getCaseOptions>
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
            sprintf(filepath,"%ls%ls C4P Index.xml",extractInfo.C4PPath,buffer);
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
            sprintf(filepath,"%ls%ls C4M Index.xml",extractInfo.C4MPath,buffer);
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



int createNameList(HANDLE evObj)
{
    createSQLNameList(vicsDB, evObj);
    return 0;
}

/*Function: determineHashTypes
    Function to determine where primary or secondary hash is MD5 or SHA1
    Stores values in global variables MD5Hash and SHA1Hash

    1 indicates that primary hash relates to that hash type

    2 indicates secondary hash relates that that hash type

    0 indicates that this hash is not computed

    Returns:
        0 - Always

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

/*Function: setupResultsFiles
    Function for managing creation of XML files and debug log if required.

    Called from <getCaseOptions>

    Returns:
        0 - Function completed successfully
        1 - Failed to create either Picture or Video output file.

    See Also:
        <getCaseOptions>
*/

int setupResultsFiles()
{
    if (extractInfo.debugSet){debugWriteDetails("Start of setupResultsFiles Function");}
    char filePath[2048]={0};
    if (extractInfo.extractPictures)
    {
        sprintf(filePath,"%ls Pictures Results.txt",extractInfo.C4PPath);
        picResults=fopen(filePath,"w");
        filePath[0]='\0';
        if (extractInfo.debugSet)
        {
            //open debug file
            sprintf(filePath,"%lsdebug.log",extractInfo.C4PPath);
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
        sprintf(filePath,"%ls Video Results.txt",extractInfo.C4MPath);
        vidResults=fopen(filePath,"w");
        filePath[0]='\0';
        if (extractInfo.debugSet && (!extractInfo.extractPictures))
        {
            //open debug file
            sprintf(filePath,"%lsdebug.log",extractInfo.C4MPath);
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

/*Function: setupVicsExport
    Function for managing creation of VICS output files

    Called from <getCaseOptions>, should possibly be delayed until generating VICS data

    Returns:
        0 - Function completed successfully
        -1 - Failed to create VICS Picture file output
        -2 - Failed to create VICS Video file output

    See Also:
        <getCaseOptions>
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

/*Function: getCaseOptions
    Function for setting up a number of things, only executed in the first run of XT_Prepare (i.e. when program starts).

    Called from <XT_Prepare>

    Returns:
        0  - Function completed successfully
        -1 - Failed (generic)
        -4 - Failed to start process

    See Also:
        <XT_Prepare>
        <setupVicsExport>
        <setupResultsFiles>

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
    //1.50 only create if not just ZIP
    else
    {
        char buffer[1024];
        sprintf(buffer,"%ls",extractInfo.C4PPath);
        if (extractInfo.C4ALLExport || extractInfo.VICExport)
        {
            strcat(buffer,"Files");
            CreateDirectory(buffer,NULL);
        }
        buffer[0] = '\0';
        sprintf(buffer,"%ls",extractInfo.C4MPath);
        if (extractInfo.C4ALLExport || extractInfo.VICExport)
        {
            strcat(buffer,"Files");
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
    //1.50 moved this to when records are output.
    /*
    if (extractInfo.VICExport || extractInfo.VICSCompressed)
    {
        int retVal = setupVicsExport();
        if (retVal !=0)
        {
            outputErrorMessage(L"Error setting up VICS export files");
            return -1;
        }
    }*/
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
    //1.50 setup zip file
    if (extractInfo.VICSCompressed)
    {
        setArchivePath(extractInfo.C4PPath, SET_PIC_PATH);
        setArchivePath(extractInfo.C4MPath, SET_VID_PATH);
        setupZipArchives();
    }
    if (extractInfo.debugSet){debugWriteDetails("End of GetCaseOptions function");}
    return 0;
}

/*Function: getCommandLineOptions
    Function for parsing out command line options. If there are none, or not enough to run,
    options window will be displayed in parent function

    Called from <getCaseOptions>

    Returns:
        0 - No Arguments or insufficent to process
        1 - Command line arguments extracted successfully.

    See Also:
        <getCaseOptions>
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
        XWF_OutputMessage(L"CommandLinxToArgw Failed\n",0);
    }
    if (numArgv == 1)
    {
        if (extractInfo.debugSet){debugWriteDetails(0, L"getCommandLineOptions End - No Args");}
        return 0;
    }
    //check for XTparam
    for (int i = 0;i<numArgv;i++)
    {
        if (wcsncmp(L"xtparam:",argv[i],8)==0)
        {
            int argLen = wcslen(argv[i]);
            wchar_t* tempArg = new wchar_t[argLen - 7];
            memcpy(tempArg,(wchar_t*)(argv[i] + 8) ,sizeof(wchar_t) * (argLen - 8));
            tempArg[argLen - 8] = L'\0';
            addCaseDetail(tempArg);
            delete[] tempArg;
        }
    }
    if ((extractInfo.C4MPath[0] == L'\0') && (extractInfo.C4PPath[0] == L'\0'))
    {
        if (extractInfo.debugSet){debugWriteDetails(0, L"getCommandLineOptions End - No paths provided");}
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
    //1.50 added check if vicscompressed is set.
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
    //1.39 - need to create a GUID and case reference
    HRESULT error = CoCreateGuid(&vCaseData.caseGuid);
    if (extractInfo.GriffeyeCaseName == NULL)
    {
        vCaseData.CaseNumber = new wchar_t[128];
        INT64 result = XWF_GetCaseProp(NULL,1,vCaseData.CaseNumber,128);
    }
    extractInfo.processStart = TRUE;
    if (extractInfo.debugSet){debugWriteDetails(0, L"getCommandLineOptions End - return 1");}
    return 1;
}

/*Function: addCaseDetail
    Parses individual command line parameters

    Ensures all paths end with a '\' character.

    Should look at having a function that parses arguments into the 2 sections first (name, value)

    Called from <getCommandLineOptions>

    See Also:
        <getCommandLineOptions>
*/

void addCaseDetail(wchar_t* strArg)
{
    if (extractInfo.debugSet){debugWriteDetails(0, L"addCaseDetail Start");}
    if (wcsncmp(L"picpth:",strArg,7)==0)
    {
        memcpy(extractInfo.C4PPath,(wchar_t*)strArg+7,(wcslen(strArg)-7) * sizeof(wchar_t));
        if (extractInfo.C4PPath[wcslen(strArg)-8] != L'\\')
        {
            extractInfo.C4PPath[wcslen(strArg)-7] = L'\\';
            extractInfo.C4PPath[wcslen(strArg)-6] = L'\0';
        }
        else
        {
            extractInfo.C4PPath[wcslen(strArg)-7] = L'\0';
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
            memcpy(extractInfo.C4MPath,(wchar_t*)strArg+7,(wcslen(strArg)-7) * sizeof(wchar_t));
            if (extractInfo.C4MPath[wcslen(strArg)-8] != L'\\')
            {
                extractInfo.C4MPath[wcslen(strArg)-7] = L'\\';
                extractInfo.C4MPath[wcslen(strArg)-6] = L'\0';
            }
            else
            {
                extractInfo.C4MPath[wcslen(strArg)-7] = L'\0';
            }
            SHCreateDirectoryExW(NULL,extractInfo.C4MPath,NULL);
            XWF_OutputMessage(extractInfo.C4MPath,0);
        }
    }
    else if (wcsncmp(L"grfpth:",strArg,7)==0)
    {
        extractInfo.GriffeyeCaseLocation = new wchar_t[wcslen(strArg)];
        memcpy(extractInfo.GriffeyeCaseLocation,(wchar_t*)strArg+7,(wcslen(strArg)-7) * sizeof(wchar_t));
        extractInfo.GriffeyeCaseLocation[wcslen(strArg)-7] = L'\0';
        SHCreateDirectoryExW(NULL,extractInfo.GriffeyeCaseLocation,NULL);
        XWF_OutputMessage(extractInfo.GriffeyeCaseLocation,0);
    }
    else if (wcsncmp(L"grfcse:",strArg,7)==0)
    {
        extractInfo.GriffeyeCaseName = new wchar_t[wcslen(strArg)];
        memcpy(extractInfo.GriffeyeCaseName,(wchar_t*)strArg+7,(wcslen(strArg)-7) * sizeof(wchar_t));
        extractInfo.GriffeyeCaseName[wcslen(strArg)-7] = L'\0';
        XWF_OutputMessage(extractInfo.GriffeyeCaseName,0);
    }
    //1.41 add option for excluding media from within live videos
    else if (wcsncmp(L"excludeFromVid",strArg,14)==0)
    {
        //set option
        extractInfo.checkParent = true;
    }
    //1.50 added compressed vics
    else if (wcsncmp(L"compressVICS",strArg,12)==0){
        extractInfo.C4ALLExport = false;
        extractInfo.VICExport = false;
        extractInfo.VICSCompressed = true;
    }
    //1.51 added custom griffeye settings file
    else if (wcsncmp(L"grfset",strArg,7)==0){
        extractInfo.GriffeyeSettingsName = new wchar_t[wcslen(strArg)];
        memcpy(extractInfo.GriffeyeSettingsName,(wchar_t*)strArg+7,(wcslen(strArg)-7) * sizeof(wchar_t));
        extractInfo.GriffeyeSettingsName[wcslen(strArg)-7] = L'\0';
        XWF_OutputMessage(extractInfo.GriffeyeCaseName,0);
    }
    //1.51 added excluding thumbnail options
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

/* Section: Volume Setup Functions
    Functions each time the X-Tension moves to a new volume
*/

/*Function: volumePrepare

    Function to prepare for run over next volume.

    Added in version 1.50, split from XT_Prepare

    Returns:
        0 - Always

    See Also:
        <XT_Prepare>

*/

int volumePrepare(HANDLE hEvidence)
{
    if (extractInfo.debugSet){debugWriteDetails(0, L"volumePrepare Start");}
    //determine column name each time
    if (versionNo >= 2030){
        DeviceTypeCol = determineColumnNumber(L"Device type");
    }
    XWF_OutputMessage((LPWSTR)XWF_GetEvObjProp(hEvidence,6,NULL),0);
    //need to fill target value
    DWORD target = getRootObj(vicsDB,currEvidence);

    //locate fileNo for ID for C4P/M XML Export
    int fileNumber = getFileNumber(vicsDB,target);
    if (fileNumber >= 0)
    {
        //we have match
        currentFileObject=fileNumber;
        wcscpy(currentEvObject,extractInfo.outputFiles[fileNumber].evidenceObj);
        currPicFile = extractInfo.outputFiles[fileNumber].picOutput;
        currVidFile = extractInfo.outputFiles[fileNumber].vidOutput;
        picCount = extractInfo.outputFiles[fileNumber].picCounter;
        vidCount = extractInfo.outputFiles[fileNumber].vidCounter;
    }
    //1.50 added check if C4All XML export selected
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

/*Function: determineColumnNumber
    Function to locate the ID of a column, given the name as a wchar_t* parameter

    Wide character string should be NULL terminated

    Added in version 1.41

    Returns:
        >0 - ID of the column that matches provided text
        -1 - column not located

*/

//1.41 to be used to get device type column
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

/* Section: Main Processing
    Functions used by the processing of items, generally the functions that will be called once when processing a media item
*/

/*Function: checkParentType
    Function to check if parent type is a video file.

    This function is used to exclude pictures and videos that are embedded in live videos
    This includes extracted frames or sub videos, to prevent issues with numbers in grading tools.

    Added in version 1.40

    Returns:
        0 - parent is not a video file or not confirmed as such
        1 - parent is a Video file with file signature confirmation

*/

//1.40 new function for checking if file is in live video
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

/*Function: checkItemType
    Function to check the type of an item. nItemID and an int* named picture passed as parameters

    nItemID is the internal item ID of the object being checked

    picture parameter indicates where media is picture or not. Picture is set to 1 if item is a picture

    Generally speaking it checks that the category of the file is either a picture or a video

    In version 1.38, a check was added to include Flash videos, which are included in the Internet category

    Returns:
        TYPE_MEDIA - Item is a media type
        TYPE_OTHER - Item is not a media item

*/

int checkItemType(LONG nItemID, int* picture)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkItemType Start");}
    LPWSTR type = new wchar_t[128];
    type[0] = L'\0';
    DWORD length = 0x40000080;

    LONG retVal = XWF_GetItemType(nItemID,type,length);
    //return error val if function fails.
    if (retVal == -1)    {
            delete[] type;
            return ERROR_GETITEMTYPE;
    }
    if (!(((wcscmp(type,(LPWSTR)L"Pictures")==0) && extractInfo.extractPictures)||((wcscmp(type,(LPWSTR)L"Video")==0) && extractInfo.extractVideos)))
    {
        //not something we are interested in.
        //1.38 add check here for flash videos Type Internet Type Description "Macromedia Flash [Certain potentially relevant types]"
        if (wcscmp(type,(LPWSTR)L"Internet")==0)
        {
            //1.38 get description of file type, not category
            long descLen = 0x20000080;
            wchar_t descr[128]={0};
            LONG retVal = XWF_GetItemType(nItemID,(wchar_t*)&descr,descLen);
            if (retVal == -1){
                    delete[] type;
                    return ERROR_GETITEMTYPEDESC;
            }
            if (wcsncmp(descr,L"Macromedia Flash",16)!=0)
            {
                delete[] type;
                if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkItemType End Return TYPE_OTHER");}
                return TYPE_OTHER;
            }
        }
        else{
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkItemType End Return TYPE_OTHER");}
            delete[] type;
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
    delete[] type;
    return TYPE_MEDIA;
}

/*Function: validFileSize
    Function to determine if size is within required range

    Added in version 1.50 as part of request to exclude small files (set by user).

    Returns:
        true    - File within valid size range
        false   - File outside valid size range

    See Also:
        Called from <MainItemProcess>
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

bool isThumbnailObject(LONG nItemID)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isThumbnailObject - Start of Function");}
    if (wcscmp(XWF_GetItemName(nItemID),L"Thumbnail.jpg")!=0){
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isThumbnailObject - End Return False");}
        return false;
    }
    LONG parentID = XWF_GetItemParent(nItemID);
    DWORD buffLen = 1024 | 0x40000000;
    wchar_t buffer[1024];
    LONG itemStatus = XWF_GetItemType(parentID,buffer,buffLen);
    if (wcscmp(buffer, L"Pictures")!=0){return false;}
    if (itemStatus == 3 || itemStatus == 5 || itemStatus == 6 ){
        if (extractInfo.exceptMismatch){
            //need to add code to check for report table association here
            buffer[0] = L'\0';
            DWORD numTables = XWF_GetReportTableAssocs(parentID,buffer,1024);
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
        else{return true;}
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isThumbnailObject - End Return False");}
    return false;
}

//1.51 added new function to check file type status is one user selected.
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

//1.51 added new function to check file type status is one user selected.
bool isSelectedFileFormatStatus(LONG nItemID)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isSelectedFileFormatStatus - Start of Function");}
    LONG fileFormat = XWF_GetItemType(nItemID,NULL,0x80000000);
    fileFormat = (fileFormat & 0xff00) >> 8;
    if (fileFormat != -1){
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
    }
    else{
        debugWriteDetails(nItemID,L"Error determining File Format Status");
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"isSelectedFileFormatStatus - End of Function");}
    return true;
}

/*Function: checkItemExport
    Function for determining whether item is exported

    Added in 1.50 to put all checks in same function

    Called from <XT_ProcessItem>

    Returns:
        True - item to be exported
        False - item to be ignored

    See Also:
        <XT_ProcessItem>
*/

bool checkItemExport(LONG nItemID, int* picture, INT64* fileSize)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"checkItemExport - Start of Function");}
    if (!validType(nItemID,picture)) {return false;}

    //1.40 need to add a check to see if parent file is a video
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
    if (*fileSize == -1){
        errorRaised(nItemID,REPORT_UNKNOWN_FILESIZE);
        return false;
    }
    //1.50 check if valid size
    if (!validFileSize(extractOpt,*fileSize,*picture)){
        //1.50 add excluded on filesize
        errorRaised(nItemID,REPORT_EXCLUDED_FILESIZE);
        return false;
    }
    //1.50 new feature to exclude thumbnails
    if (extractInfo.ignoreThumbs)
    {
        if (isThumbnailObject(nItemID)) {
            errorRaised(nItemID,REPORT_EXCLUDED_THUMBNAIL);
            return false;
        }
    }
    //1.51 new ability to filter out based on file consistency and status
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



/*Function: MainItemProcess
    Main function for processing items

    Gets hash values for files using <returnHashValue>

    Write the output file using <writeOutputFile>

    Then writes the XML/VICS details (as appropriate) using <UpdateRecords> function

    Returns:
        0 - Always

    See Also:
        <returnHashValue>
        <writeOutputFile>
        <UpdateRecords>
*/

LONG MainItemProcess(LONG nItemID, int picture, INT64 fileSize)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"MainItemProcess Start");}
    hashValueStruct currHash;
    //start processing item
    int result = returnHashValue(nItemID,(wchar_t*)&currHash.MD5,(wchar_t*)&currHash.SHA1,(wchar_t*)&currHash.photoDNA);
    if (result == 0)
    {
        //1.50 changed to switch whether compressed output or not.
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
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"MainItemProcess End");}
        return 0;
    }
    UpdateRecords(picture,nItemID, currHash);
    //reset completed flag to 0
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"MainItemProcess End");}
    return 0;
}

/*Function: UpdateRecords
    Function for managing creation of XML/VICS records. Called from <MainItemProcess>

    Calls <createC4AllRecord> is XML output is selected and <createVICSRecord> if VICS output selected

    Increments counters for pictures and videos after record updates

    Returns:
        0 - Always

    See Also:
        <MainItemProcess>
*/

int UpdateRecords(int picture, long nItemID, hashValueStruct currHash)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"UpdateRecords Start");}
	//we now have hash values!!!!
	if (extractInfo.C4ALLExport)
	{
		lockC4All.lock();
		createC4AllRecord(nItemID,picture,currHash.MD5);
		lockC4All.unlock();
	}
	// code for finding VICS record ID
	if (extractInfo.VICExport || extractInfo.VICSCompressed)
	{
		lockVics.lock();
		createVICSRecord(nItemID,picture,currHash);
		lockVics.unlock();
	}
	updateLock.lock();
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
	updateLock.unlock();
	if (extractInfo.debugSet){debugWriteDetails(nItemID, L"UpdateRecords End");}
	return 0;
}

/*Function: createGriffeyeCase
    Function for creating Griffeye case. Adds pictures then Videos

    Needs amending to allow addition to existing case, this requires --add source to be included in argument

    Given that --add-source is a case level argument, assume it only needs including once.
    Should have a function that checks if the case already exists in that path and therefore requires the --add-source flag.

    Possibly need to look at issues relating to Unicode characters in path name

    Called from <caseCleanup>

    See Also:
        Called from -   <caseCleanup>
        Calls       -   <DirExistsW>
*/

static const char* detectGriffeyeExe(const wchar_t* folder)
{
    char narrowFolder[MAX_PATH];
    snprintf(narrowFolder, MAX_PATH, "%ls", folder);
    bool hasSlash = (narrowFolder[strlen(narrowFolder)-1] == '\\');
    char check[MAX_PATH];
    sprintf(check, hasSlash ? "%sanalyze-cli.exe" : "%s\\analyze-cli.exe", narrowFolder);
    if (FILE* f = fopen(check, "r")) { fclose(f); return "analyze-cli.exe"; }
    sprintf(check, hasSlash ? "%smagnet-griffeye-cli.exe" : "%s\\magnet-griffeye-cli.exe", narrowFolder);
    if (FILE* f = fopen(check, "r")) { fclose(f); return "magnet-griffeye-cli.exe"; }
    return NULL;
}

int createGriffeyeCase()
{
    if (extractInfo.debugSet){debugWriteDetails(0, L"createGriffeyeCase Start");}
    const char* griffeyeExe = detectGriffeyeExe(extractOpt.GriffeyePath);
    if (griffeyeExe == NULL)
    {
        XWF_OutputMessage(L"Griffeye CLI executable not found, cannot create case",0);
        return 1;
    }
    char cmdOutput[8192];
    char tempString[1024];
    sprintf(cmdOutput,"\"%ls%s\" import --case-folder \"%ls\" --name \"%ls\"",extractOpt.GriffeyePath,griffeyeExe,extractInfo.GriffeyeCaseLocation, extractInfo.GriffeyeCaseName);
    int sourceNo = 1;

    //1.41 add option to add to existing case
    wchar_t path[32768] ={0};
    snwprintf(path,32768,L"%ls\\%ls\\%ls.ANCF",extractInfo.GriffeyeCaseLocation, extractInfo.GriffeyeCaseName,extractInfo.GriffeyeCaseName);
    if (ifFileExistsW((wchar_t*)&path))
    {
        strncat(cmdOutput, " --add-source",8191);
    }

    if (extractInfo.extractPictures)
    {
        //1.41 use temp string an concatenate
        tempString[0] = '\0';
        sprintf(tempString," --source-id source%d --source-path \"%lsVICS_Pictures_Results.json\" --source-type vics --include-vics-data all",sourceNo++,extractInfo.C4PPath);
        strncat(cmdOutput,tempString,8191);
    }
    if (extractInfo.extractVideos)
    {
        //1.41 use temp string an concatenate
        tempString[0] = '\0';
        sprintf(tempString," --source-id source%d --source-path \"%lsVICS_Movies_Results.json\" --source-type vics --include-vics-data all", sourceNo++,extractInfo.C4MPath);
        strncat(cmdOutput,tempString,8191);
    }
    //1.51 added use of settings file
    if (extractInfo.GriffeyeSettingsName != nullptr)
    {
        tempString[0] = '\0';
        sprintf(tempString," --import-settings-file %ls", extractInfo.GriffeyeSettingsName);
        strncat(cmdOutput,tempString,8191);
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

/*Function: caseCleanup

    Called when XT_Done called (at end of run)

    Used to clean up files and perform end of run jobs. This includes output of VICS data and Griffeye case creation

    Appears database is never output, need to check why.

    Called from <XT_Done>

    Returns:
        0   - Always

    See Also:
        <XT_Done>
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
    //1.50 moved here for when C4All not selected.
    fclose(picResults);
    fclose(vidResults);
    if (extractInfo.debugSet)
    {
        endDebugLog();
    }
    if (extractInfo.VICExport || extractInfo.VICSCompressed)
    {
        int errChk = outputVICSFile();
    }
    //used for debug
    if (extractInfo.debugSet)
    {
        char sqlOutputPath[4096]= {0};
        sprintf(sqlOutputPath,"%ls%ls",extractOpt.errorReportPath, caseTitle);
        //1.41 changed to function in utility module rather than seperate function.
        if (!DirExists(sqlOutputPath))
        {
            CreateDirectory(sqlOutputPath,NULL);
        }
        //sprintf(sqlOutputPath,"%s\\errorOutput.sqlite",sqlOutputPath);
        //this is not called?
        strncat(sqlOutputPath,"\\errorOutput.sqlite",4095); //1.37 change
        loadOrSaveDb(vicsDB,sqlOutputPath,1);
    }
    if (extractInfo.VICExport || extractInfo.VICSCompressed)
    {
        sqlite3_close(vicsDB);
        fclose(vicPicFile);
        fclose(vicMovieFile);
        //1.50 add to zip file
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
    //1.50 close zip archives
    if (extractInfo.VICSCompressed)
    {
        closeZipArchives();
    }
    cleanupArchivePaths();
    extractInfo.noNames = 0;
    freeVicsCaseData();
    delete [] extractInfo.C4PPath;
    delete [] extractInfo.C4MPath;
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
    if (extractInfo.debugSet){debugWriteDetails(0, L"caseCleanup End");}
    return 0;
}

/*Section: Hash Value Functions*/

/*Function: getHashValue

    Function that gets hash value of a file of nItemID.

    If the forced flag is set, it will tell X-Ways to generate the hash value if it has not been computed.

    Parameters:

        LONG nItemID            - X-Ways item ID that the function will get the hash value for

        wchar_t* hashValue      - buffer to contain the extracted hash value

        int hashSize            - size in bytes of the hash type being extracted

        int hashNumber          - Type of hash to be extracted (1 = primary, 2 = secondary, 3 = PhotoDNA)

        BOOL forced             - Flag to state whether function should force X-Ways to compute a hashvalue if it hasn't already

    Returns:
        0       -   Success
        1       -   Error getting the hash value for this item


    See Also:
        Called by   -   <returnHashValue>, <getSingleHash>, <forceHashes>

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

/*Function: getSingleHash

    Function that gets a single hash value of a file of nItemID, either MD5 or SHA1.

    Calls getHashValue without forced flag

    Parameters:

        LONG nItemID            - X-Ways item ID that the function will get the hash value for

        wchar_t* md5buffer      - buffer to contain the MD5 hash value

        wchar_t* SHA1buffer     - buffer to contain the SHA1 hash value

        int hashType            - Type of hash to be extracted (MD5Hash or SHA1Hash)

    Returns:
        0       -   Success
        1       -   Error getting the hash value for this item


    See Also:
        Called by   -   <returnHashValue>
        Calls       -   <getHashValue>

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

/*Function: forceHashes

    Function that gets a single hash value of a file of nItemID, either MD5 or SHA1.

    Calls getHashValue with forced flag

    Parameters:

        LONG nItemID            - X-Ways item ID that the function will get the hash value for

        wchar_t* md5buffer      - buffer to contain the MD5 hash value

        wchar_t* SHA1buffer     - buffer to contain the SHA1 hash value

        int md5Type             - 0 if MD5 hash not to be extracted, any other value otherwise

        int sha1Type            - 0 if SHA1 hash not to be extracted, any other value otherwise

    Returns:
        0       -   Success
        1       -   Error getting the hash value for this item


    See Also:
        Called by   -   <returnHashValue>
        Calls       -   <getHashValue>

*/

int forceHashes(LONG nItemID,wchar_t* md5Buffer, wchar_t* SHA1Buffer, int md5Type,int sha1Type)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"forceHashes Start");}
    //check if
    int checkVal = 0;
    //1.41 changed from MD5Hash to md5Type
    if (md5Type !=0)
    {
        //this is primary hash
        checkVal = getHashValue(nItemID,md5Buffer,32,MD5Hash,true);
    }
    //1.41 changed from SHA1Hash to sha1Type
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

/*Function: returnHashValue

    Function that returns MD5, SHA1 and PhotoDNA hashes (if computed)

    Parameters:

        LONG nItemID            - X-Ways item ID that the function will get the hash value for

        wchar_t* md5buffer      - buffer to contain the MD5 hash value

        wchar_t* SHA1buffer     - buffer to contain the SHA1 hash value

        int md5Type             - 0 if MD5 hash not to be extracted, any other value otherwise

        int sha1Type            - 0 if SHA1 hash not to be extracted, any other value otherwise

    Returns:
        0       -   Success
        1       -   Error getting the hash value for this item


    See Also:
        Called by   -   <MainItemProcess>
        Calls       -   <getSingleHash>, <forceHashes>

*/

int returnHashValue(LONG nItemID, wchar_t* md5Buffer, wchar_t* SHA1Buffer, wchar_t* PDNABuffer)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"returnHashValue Start");}
    INT64 flags;
    BOOL complete=FALSE;
    flags = XWF_GetItemInformation(nItemID,3,&complete);
    if (flags & 0x00040000)
    {
        //extract primary hash value
        int retVal = getSingleHash(nItemID,md5Buffer,SHA1Buffer,1);
        if (retVal !=0)
        {
            outputErrorMessage(L"Unable to retrieve primary hash for itemID: ",nItemID);
            xwfOutputLock.lock();
            recordError(vicsDB,ERROR_NO_MD5_HASH, nItemID, currSrcID);
            xwfOutputLock.unlock();
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
            xwfOutputLock.lock();
            recordError(vicsDB,ERROR_NO_MD5_HASH, nItemID, currSrcID);
            xwfOutputLock.unlock();
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
            //1.38 changed to add to report table
            errorRaised(nItemID,REPORT_NOHASH);
            xwfOutputLock.lock();
            recordError(vicsDB, ERROR_HASH_NOT_COMPUTED,nItemID,L"No hash computed for item");
            xwfOutputLock.unlock();
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
    if (flags & 0x80000000)
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

/* Section: Writing VICS records*/

/*Function: outputVICSFile

    Function for writing VICS records to file.

    1.41 fix - Made a check to ensure that only attempt to write files that re selected (i.e. pictures or videos)

    Called from <caseCleanup>

    Returns:
        0   - Success
        1   - Error writing picture files
        2   - Error writing Video files
        3   - Error writing both Picture and Video files

    See Also:
        <caseCleanup>
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
        //1.41 add check for each media type
        if (extractInfo.extractPictures)
        {
            int check = writeRecords(vicsDB,vicPicFile, 1);
            if (check != 0 )
            {
                XWF_OutputMessage(L"Error writing Picture VICS data",0);
                retVal = retVal | 0x01;
            }
        }
        //1.41 add check for each media type
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

/*Function: writeSQLMediaRecord

    Function that creates a VICS media record and calls functions to insert record into SQLite database

    Parameters:

        LONG nItemID                - X-Ways item ID that the media file relates to

        hashValueStruct* hashVals   - Structure containing hash values for item

        int picture                  - Flag indicating whether file is a picture

    See Also:
        Called by   -   <createVICSRecord>
        Calls       -   <generateRelativeFilePathVICS>, <insertMediaRecord>, <deallocateMediaRecord>

*/

void writeSQLMediaRecord(LONG nItemID, hashValueStruct hashVals, int picture)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"writeSQLMediaRecord Start");}
    VICSMedia currentRecord;
    InitializeMediaRecord(currentRecord);
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
    int retVal = generateRelativeFilePath(&relativeBuffer[0],128,currentRecord.MD5,true);
    //merge paths
    swprintf(currentRecord.RelativeFilePath,L"%s\\\\%ls",relativeBuffer,currentRecord.MD5);
    currentRecord.MediaSize = XWF_GetItemSize(nItemID);
    insertMediaRecord(vicsDB,currentRecord, picture);
    deallocateMediaRecord(currentRecord);
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"writeSQLMediaRecord End");}
}


/*Function: getPhysicalOffset

    Function that gets a Physical Offset to a file, given an ItemID.

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
    //1.53 0 represents an error, 0xFFFFFFFF indicates Not available.
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
    //1.53 0 represents an error, 0xFFFFFFFF indicates Not available.
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


//1.51 created a seperate function for getting dates

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

bool getDeletedStatus(long nItemID, BOOL* unallocated)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getDeletedStatus Start");}
    BOOL boolCheck=0;
    INT64 delStatus = XWF_GetItemInformation(nItemID,4,&boolCheck);
    if (delStatus == 0)
    {
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getDeletedStatus return false");}
        return false;
    }
    else
    {
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"getDeletedStatus return true");}
        //1.54 added unallocated flag to check
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

/*Function: extractMediaFileRecordDetails

    Function that creates a VICS media record and calls functions to insert record into SQLite database

    Parameters:

        LONG nItemID                - X-Ways item ID that the media file relates to

        wchar_t* MD5Hash            - Pointer to Wide character String that contains MD5 hash value

        int picture                 - Flag indicating whether file is a picture

        VICSMediaFile* record       - Pointer to a VICSMediaFile struct that details will be stored in

    See Also:
        Called by   -   <createVICSRecord>
        Calls       -   <insertMediaFileRecord>, <deallocateMediaFileRecord>, <extractMediaFileRecordDetails>

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
    //1.51 getting filename is own function
    getItemFileName(nItemID,record);
    //MAC times
    //1.51 used specific function instead
    getFileTimestamp(&record->created,nItemID,32);
    getFileTimestamp(&record->written,nItemID,33);
    getFileTimestamp(&record->accessed,nItemID,34);
    //get deleted status
    //1.51 moved to own function
    record->deleted = getDeletedStatus(nItemID, &record->unallocated);
    //getPhysical Sector
    //1.50 moved into function
    record->physicalLocation = getPhysicalOffset(nItemID,&record->unallocated,&record->deleted);
    //add source
    record->sourceID = new wchar_t[128];
    record->sourceID[0] = L'\0';
    swprintf(record->sourceID,L"%ls",currSrcID);
    //1.50 add itemid to record
    record->XWFitemID = nItemID;
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"extractMediaFileRecordDetails End");}
    return 0;
}

/*Function: writeSQLMediaFileRecord

    Function that creates a VICS media record and calls functions to insert record into SQLite database

    Parameters:

        LONG nItemID                - X-Ways item ID that the media file relates to

        wchar_t* MD5Hash            - Pointer to Wide character String that contains MD5 hash value

        in picture                  - Flag indicating whether file is a picture

    See Also:
        Called by   -   <createVICSRecord>
        Calls       -   <insertMediaFileRecord>, <deallocateMediaFileRecord>, <extractMediaFileRecordDetails>

*/

void writeSQLMediaFileRecord(LONG nItemID,wchar_t MD5Hash[33], int picture)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"writeSQLMediaFileRecord Start");}
    VICSMediaFile currentMedia;
    InitializeMediaFileRecord(currentMedia);
    extractMediaFileRecordDetails(nItemID, MD5Hash, picture, &currentMedia);
    insertMediaFileRecord(vicsDB,currentMedia, picture);
    //clear up memory
    deallocateMediaFileRecord(currentMedia);
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"writeSQLMediaFileRecord End");}
}

/*Function: writeSQLMediaMetadataRecord

    Function that creates a VICS media metadata record and calls functions to insert record into SQLite database

    Deallocates memory associated with record afterwards

    Parameters:

        LONG nItemID            - X-Ways item ID that the media file relates to

        wchar_t* MD5Hash        - Pointer to Wide character String that contains MD5 hash value

        wchar_t* PropertyName   - Pointer to wide character string containing NULL terminated property name

        wchar_t* PropertyValue  - Pointer to wide character string containing NULL terminated property value

    See Also:
        Called by   -   <createVICSRecord>
        Calls       -   <insertMediaMetadataRecord>, <deallocateMediaMetadataRecord>

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

bool replaceOriginalItem(LONG origID, LONG newID)
{
    if (extractInfo.debugSet){debugWriteDetails(0, L"replaceOriginalItem Start");}
    INT64 originalDeletedFlags = XWF_GetItemInformation(origID,XWF_ITEM_INFO_DELETION, NULL);
    INT64 newDeletedFlags = XWF_GetItemInformation(newID,XWF_ITEM_INFO_DELETION, NULL);
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

/*Function: createVICSRecord

    Function that creates a VICS media metadata record and calls functions to insert record into SQLite database

    <writeSQLMediaMetadataRecord> only called if Device Type is not blank or "unknown".

    Parameters:

        LONG nItemID            - X-Ways item ID that the media file relates to

        int picture             - flag to state if item is a picture

        hashValueStruct hasVals - structure containing hash values relating to X-Ways itemID

    See Also:
        Called by   -   <UpdateRecords>
        Calls       -   <writeSQLMediaRecord>, <writeSQLMediaFileRecord>, <writeSQLMediaMetadataRecord>

*/

int createVICSRecord(LONG nItemID, int picture, hashValueStruct hashVals)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"createVICSRecord Start");}
    INT64 recordID = getVicsRecord(vicsDB, hashVals.MD5, picture);
    if (recordID == 0){
        //needs adding
        writeSQLMediaRecord(nItemID, hashVals, picture);
    }
    //1.50 - look at checking if record for same sector and hash value exists
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
            InitializeMediaFileRecord(recUpdate);
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

    //1.41 add media metadata
    if (DeviceTypeCol != -1)
    {
        //check if device type column is blank
        wchar_t buffer[128] ={0};
        LONG result = XWF_GetCellText(nItemID,NULL,0,DeviceTypeCol,(wchar_t*)&buffer,127);
        if (result < 0)
        {
            //error
            outputErrorMessage(L"Unable to get Device Type Column Data for Item: ",nItemID);
        }
        else{
            if (wcsncmp(L"",buffer,127)!=0 && wcsncmp(L"unknown",buffer,127)!=0)
            {
                writeSQLMediaMetadataRecord(nItemID, hashVals.MD5, L"Device Type",(wchar_t*)buffer);
            }
            //1.50 needed for testing
            else if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Device Type either blank or unknown");}
        }
    }
    else if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Device Type column unidentified");}

    //1.50 added Report Table Association checks
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

/*Section: C4All XML Records*/

/*Function: createC4AllRecord

    Function that creates an XML record and writes it to the relevant XML file

    Parameters:

        LONG nItemID            - X-Ways item ID that the media file relates to

        int picture             - flag to state if item is a picture

        wchar_t[33] MD5Hash     - Wide character string with MD5 hash relating to item

    See Also:
        Called by   -   <UpdateRecords>
        Calls       -   <removeInvalidChars>

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
            picFile.createdTime = Filetime2Unix(picFile.createdTime);
    }
    done = FALSE;
    picFile.modifiedTime = XWF_GetItemInformation(nItemID,33,&done);
    if (picFile.modifiedTime != 0)
    {
            picFile.modifiedTime = Filetime2Unix(picFile.modifiedTime);
    }
    done = FALSE;
    picFile.accessedTime = XWF_GetItemInformation(nItemID,34,&done);
    if (picFile.accessedTime != 0)
    {
            picFile.accessedTime = Filetime2Unix(picFile.accessedTime);
    }
    done = FALSE;
    picFile.deletionTime = XWF_GetItemInformation(nItemID,36,&done);
    if (picFile.deletionTime != 0)
    {
            picFile.deletionTime = Filetime2Unix(picFile.deletionTime);
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
    picFile.fileSize=XWF_GetItemSize(nItemID);
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

/*Section: X-Ways Utility Functions  */

/*Function: getFileName

    Function that gets a filename for a given ItemID in an Evidence object.

    Takes a pointer to a Wide Character buffer and the size of the buffer.

    Replaces new lines and '\' characters in name

    Parameters:

        LPWSTR evObject     -
        LONG nItemID        -
        wchar_t* retValue   - Pointer to Wide Character String where file name will be returned
        long bufferSize     - Size of buffer for filename

    Returns:
            0       -   Always

    See Also:
        Called by   -   <createC4AllRecord>

*/

int getFileName(LPWSTR evObject,LONG nItemID, wchar_t* retValue,long bufferSize)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Start of getFileName Function Output");}
    LPWSTR fileName = (LPWSTR)XWF_GetItemName(nItemID);
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
        snwprintf(retValue,bufferSize,L"%ls",fileName);
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"End of getFileName Function Output");}
    return 0;
}

/*Function: getFullPath
    Returns a full path for a give item provided as parameter nItemID.
    Returns slight difference between XML version and VICS
    XML version requires full path, including filename
    VICS version requires on path of parent of item
    In VICS versions, it replaces '\' characters with '|'

    This function needs a re-write in order to make it more readable and modular


    See Also:
        <extractVICSMediaFileSQL>
        <extractVICSMediaSQL>
        <createVICSstring>
        <outputVICSFile>

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
        swprintf(retValue,L"");
    }
    parent = XWF_GetItemParent(nItemID);
    do
    {
        if (parent != -1)
        {
            LPWSTR nameParent = (LPWSTR)XWF_GetItemName(parent);
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
                        swprintf(temp,L"%ls\\\\%ls",newName,retValue);
                    }
                    else
                    {
                        //if retValue is "" creates an error.
                        swprintf(temp,L"%ls\\\\",newName);
                    }
                    delete[] newName;
                }
                else
                {
                    swprintf(temp,L"%ls\\%ls",nameParent,retValue);
                }
            }
            else
            {
                if (isVic)
                {
                    int chkLen = wcslen(retValue);
                    if (chkLen > 0)
                    {
                        swprintf(temp,L"\\\\%ls",retValue);
                    }
                    else
                    {
                        swprintf(temp,L"\\\\");
                    }
                }
                else
                {
                    swprintf(temp,L"\\%ls",retValue);
                }
            }
            swprintf(retValue,L"%ls",temp);
            parent = XWF_GetItemParent(parent);
        }
    } while (parent != -1);
    //prepend with evidence object name
    HANDLE hEvidence = XWF_GetEvObj(currEvidence);
    LPWSTR partName = (LPWSTR)XWF_GetEvObjProp(hEvidence,6,NULL);
    if (partName != NULL)
    {
        if (isVic)
        {
            //1.51 remove evidence source id from beginning of path
            //swprintf(retValue,L"%ls\\\\%ls%ls",currSrcID,partName,temp);
            swprintf(retValue,L"%ls%ls",partName,temp);
        }
        else
        {
            swprintf(retValue,L"%ls\\%ls%ls",currSrcID,partName,temp);
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


/*1.41 removed this function, appears to be unused

int openMediaEntry(FILE* vFile)
{
    //fwprintf(vFile,L"\t\t\"odata.id\":\"Media(\\\"0\\\")\",");
    //1.38 changed to single byte strings
    fprintf(vFile,"\t\t\"");
    return 0;
}
*/

/*Section: VICS Record Export*/

/*Function: extractIntoVicsRecord

    Function that locates all the MediaFile Entries, stored in supplied database, with matching hash value to parameter.

    Allocated memory for number of media file records required.

    Parameters:

        sqlite3* database           - Handle to an SQLite3 database. This should have created with setupVICS function

        VICSRecord*                 - Pointer to a VICSRecord that media file records are to be added to

        wchar_t* hashValue          - Pointer to wide character string containing hash value to match against. Must be NULL terminated.

        int picture                 - Integer to state if getting picture of video records. 1 indicates pictures.

    Returns:
        -1      -   Error getting media file records from database
        0       -   No records in table


    See Also:
        Called by   -   <writeRecords>

        <returnMediaFileRecords>
*/

int extractIntoVicsRecord(sqlite3* database, VICSRecord* record, wchar_t* hashValue ,int picture)
{
    sqlite3_stmt* statement;
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
    result = returnMediaMetadataRecords(database, &statement, hashValue);
    if (result < 0){
        //error
        sqlite3_finalize(statement);
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
        extractVICSMediaMetadataSQL(&record->vMediaMetaData[i],statement);
        sqlResult = sqlite3_step(statement);
    }

    return 0;
}



/*Function: writeRecords
    Takes a FILE* to the JSON file that the records are to be written to.

    This functions probably needs a re-write, along with the associated functions
    Called from outputVICSFile

    See Also:
        <extractVICSMediaFileSQL>
        <extractVICSMediaSQL>
        <createVICSstring>
        <outputVICSFile>

*/

//start of VICS code
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
        //1.383 added if there are no records
        //1.50 removed fprintf(vicFile,"}");
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

