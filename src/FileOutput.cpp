//standard libraries
#include <windows.h>
#include <cstdint>
#include <mutex>
#include <shlobj.h>

//local functions
#include "main.h"
#include "FileOutput.h"
#include "debugMessage.h"
#include "utility.h"


//defines
/* Constant: folderDepth
Depth of folders to be created based on hash values. Currently set to 2*/
#define folderDepth 2

//set max_read to 4MB - reduce ram requirements and possible file writing issues.
/* Constant: max_read
Maximum size of file to be written in a single chunk. Currently set at 4MiB*/

#define max_read 4194304LL

//globals
std::mutex lockFile, lockFolder;

//1.41 removed from this section and used one from Utility section
/*BOOL fileExists(char* path)
{
    DWORD chkAttributes = GetFileAttributes(path);
    return (chkAttributes != INVALID_FILE_ATTRIBUTES);
}*/

/*Section: File Path Generation

    This section contains functions to generate path files relating to output files on users local system*/

/*Function: generateRelativeFilePathVICS

    Function that generates relative file path for exported image.

    Filename is MD5 hash value and folders are created base don first 2 characters.

    So MD5 1234567890ABCDEF1234567890ABCDEF would be stored in folder \12\34\ using a depth of 2 (current)

    Differs from <generateRelativeFilePath> in that '\' character must be escaped. These should be merged and a VICS flag added.

    Parameters:

        char* buffer            - character array to store resulting path in

        int sizeBuffer          - integer to give max size of buffer

        wchar_t* fileName       - Wide character string with MD5 hash relating to item

    Returns:
        0   -   Always

    See Also:
        Called by   -   <writeSQLMediaRecord>
        Calls       -   None

*/

int generateRelativeFilePathVICS(char*buffer, int sizeBuffer,wchar_t* fileName)
{
    char tempBuffer[128]={0};
    strncat(buffer,"Files",128);
    for (int i=0;i<folderDepth;i++)
    {
        snprintf(tempBuffer,128,"\\\\%lc%lc",fileName[i*2],fileName[(i*2)+1]);
        strncat(buffer,tempBuffer,sizeBuffer);
        tempBuffer[0]='\0';
    }
    return 0;
}


/*Function: generateRelativeFilePath

    Function that generates relative file path for exported image.

    Filename is MD5 hash value and folders are created base don first 2 characters.

    So MD5 1234567890ABCDEF1234567890ABCDEF would be stored in folder \12\34\ using a depth of 2 (current)

    Parameters:

        char* buffer            - character array to store resulting path in

        int sizeBuffer          - integer to give max size of buffer

        wchar_t* fileName       - Wide character string with MD5 hash relating to item

    Returns:
        0   -   Always

    See Also:
        Called by   -   <generateFilePath>
        Calls       -   None

*/

int generateRelativeFilePath(char*buffer, int sizeBuffer,wchar_t* fileName)
{
    char tempBuffer[128]={0};
    strncat(buffer,"Files",128);
    for (int i=0;i<folderDepth;i++)
    {
        snprintf(tempBuffer,128,"\\%lc%lc",fileName[i*2],fileName[(i*2)+1]);
        strncat(buffer,tempBuffer,sizeBuffer);
        tempBuffer[0]='\0';
    }
    return 0;
}

/*Function: generateFilePath

    Function that generates relative file path for exported image.

    Filename is MD5 hash value and folders are created base don first 2 characters.

    So MD5 1234567890ABCDEF1234567890ABCDEF would be stored in folder \12\34\ using a depth of 2 (current)

    (TODO) Needs to have some kind of error return code for handling.

    Parameters:

        char* buffer            - character array to store resulting path in

        int sizeBuffer          - integer to give max size of buffer

        wchar_t* fileName       - Wide character string with MD5 hash relating to item

        int picFile             - integer flag showing if file is a picture or not

    Returns:
        0   -   Always

    See Also:
        Called by   -   <writeOutputFile>
        Calls       -   <generateRelativeFilePath>

*/

int generateFilePath(char* buffer, int maxSize,wchar_t* fileName, int picFile)
{
    //generate file path
    char tempBuffer[128]={0};
    if (picFile == 1)
    {
        snprintf(buffer,maxSize,"%ls\\",extractInfo.C4PPath);
    }
    else
    {
        snprintf(buffer,maxSize,"%ls\\",extractInfo.C4MPath);
    }
    //generate subfolder to split out number of files.
    int retVal = generateRelativeFilePath(&tempBuffer[0],128,fileName);
    strncat(buffer,tempBuffer,128);
    //lock folder mutex to stop folder being created twice.
    lockFolder.lock();
    if (!ifFileExists(buffer))
    {
        //create directory
        int retVal = SHCreateDirectoryEx(NULL,buffer,NULL);
        if (retVal !=0 && retVal != ERROR_FILE_EXISTS && retVal != ERROR_ALREADY_EXISTS)
        {
            outputErrorMessage(L"Error creating directory, Error Code:",retVal); //1.38 message changed slightly
        }
    }
    lockFolder.unlock();
    snprintf(tempBuffer,128,"\\%ls",fileName);
    strncat(buffer,tempBuffer,maxSize);
    return 0;
}

/*Section: File Output Generation

    This section contains functions to write the media selected from X-Ways into local files on user's machine
*/


/*Function: writeOutputFileLarge

    Function that writes a file that is over a certain size (defined by <max_read>)

    Writes file in <max_read> sized buffer chunks until all file has been written.

    Also includes a wrapper function where flush flag is not provided. This defaults to flush being false

    Parameters:

        FILE* fileOut   -   Handle to file that is to be used for output data
        INT64 fileSize  -   fileSize of the file to be written
        HANDLE hItem    -   X-Ways Handle to the item to be written
        LONG nItemID    -   X-Ways item ID for file being written
        bool flush      -   flag to flush output after each write.

    Returns:
        0   -   Success
        1   -   Error occurred

    See Also:
        Called by   -   <writeOutputFile>
        Calls       -   None

*/

int writeOutputFileLarge(FILE* fileOut, INT64 fileSize,HANDLE hItem,LONG nItemID, bool flush)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Start of writeOutputFileLarge Function Output");}
    BYTE* buffer = new BYTE[max_read+1];
    INT64 currentOffset = 0;
    BOOL Done = FALSE;
    INT64 bytesLeft = fileSize;
    DWORD readSize;
    do
    {
        if (bytesLeft > max_read){
            readSize = max_read;
        }
        else{
            readSize = bytesLeft;
        }
        DWORD read = XWF_Read(hItem,currentOffset,buffer,readSize);
        XWF_ShouldStop();
        if (read==0 || read != readSize)
        {
            delete[] buffer;
            XWF_Close(hItem);
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Premature return from writeOutputFileLarge Function Output");}
            return 1;
        }
        fwrite(buffer,read,1,fileOut);
        if (flush) {
            if (fflush(fileOut)==EOF)
            {
                outputErrorMessage(L"Error flushing file to disk",nItemID);
            }
        }
        bytesLeft = bytesLeft - readSize;
        if (bytesLeft <= 0)
        {
            Done = TRUE;
        }
        memset(buffer,0,readSize);
        currentOffset = currentOffset + readSize;
        XWF_ShouldStop();
    }
    while (Done ==FALSE);
    XWF_Close(hItem);
    delete[] buffer; //1.37
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"End of writeOutputFileLarge Function Output");}
    return 0;
}

//wrapper function
int writeOutputFileLarge(FILE* fileOut, INT64 fileSize,HANDLE hItem,LONG nItemID)
{
    return writeOutputFileLarge(fileOut,fileSize,hItem,nItemID,false);
}


/*Function: writeOutputFileSmall

    Function that writes a file that is under a certain size (defined by <max_read>)

    File written as a single block.

    If an error occurs and debug is set, creates an array of data for debugWriteDetails function

    Parameters:

        FILE* fileOut   -   Handle to file that is to be used for output data
        INT64 fileSize  -   fileSize of the file to be written
        HANDLE hItem    -   X-Ways Handle to the item to be written
        LONG nItemID    -   X-Ways item ID for file being written

    Returns:
        0   -   Success
        1   -   Error occurred

    See Also:
        Called by   -   <writeOutputFile>
        Calls       -   <debugWriteDetails> if debug set

*/

int writeOutputFileSmall(FILE* fileOut, INT64 fileSize,HANDLE hItem,LONG nItemID)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Start of writeOutputFileSmall Function Output");}
    BYTE* buffer = new BYTE[fileSize+1];
    DWORD read = XWF_Read(hItem,0,buffer,fileSize);
    //sometime file is 1 byte different, account for this.
    if (read==0 || read < fileSize-1)
    {
        XWF_Close(hItem);
        delete[] buffer;
        if (extractInfo.debugSet){
            varList errDetails;
            errDetails.entries[errDetails.noVars].varData = new int64_t;
            *(int64_t*)errDetails.entries[errDetails.noVars].varData = fileSize;
            errDetails.entries[errDetails.noVars].varLen = 8;
            errDetails.entries[errDetails.noVars].type ='i';
            errDetails.noVars++;
            errDetails.entries[errDetails.noVars].varData = new int32_t;
            *(int32_t*)errDetails.entries[errDetails.noVars].varData = read;
            errDetails.entries[errDetails.noVars].varLen = 4;
            errDetails.entries[errDetails.noVars].type ='i';
            errDetails.noVars++;
            debugWriteDetails(nItemID, L"writeOutputFileSmall",L"Read value 0 or not matching filesize",errDetails);

        }
        return 1;
    }
    fwrite(buffer,read,1,fileOut);
    XWF_Close(hItem);
    delete[] buffer;
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"End of writeOutputFileSmall Function Output");}
    return 0;
}

/*Function: checkFileSize

    Function to check that file size matches that provided by x-ways +- 2 bytes.

    This allows X-Tension to see if network issues have caused file to not write correctly.

    Parameters:

        LONG nItemID    -   X-Ways ItemID of file being checked
        char* path      -   Path of data that has been output (local machine)
        INT64 fileSize  -   Filesize as provided by X-Ways

    Returns:
        -1  -   Error getting filesize from file on disk
        0   -   File size matches expected size
        1   -   File size does not match expected size

    See Also:
        Called by   -   <writeOutputFile>
        Calls       -   None

*/

int checkFileSize(LONG nItemID,char* path,INT64 fileSize)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Start of checkFileSize Function Output");}
    WIN32_FILE_ATTRIBUTE_DATA fad;
    //sometimes takes a second for file system to update
    int counter = 0;
    LARGE_INTEGER actualSize;
    actualSize.QuadPart = 0;
    do{
        if (!GetFileAttributesEx(path, GetFileExInfoStandard, &fad)){
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"End of checkFileSize Function Return -1");}
            return -1;
        }
        actualSize.HighPart = fad.nFileSizeHigh;
        actualSize.LowPart = fad.nFileSizeLow;
        counter++;
        if (actualSize.QuadPart == 0) {
                Sleep(250);
        }
    } while (actualSize.QuadPart ==0 && counter < 10);
    if (actualSize.QuadPart == 0 && extractInfo.debugSet)
    {
        debugWriteDetails(nItemID, L"Filesize still showing as zeros");
    }
    //give a 2 byte leway
    if (!(actualSize.QuadPart >= fileSize -2 && actualSize.QuadPart <= fileSize +2))
    {
        if (extractInfo.debugSet){debugWriteDetails(nItemID, L"End of checkFileSize Function Return 1");}
            return 1;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"End of checkFileSize Function Output");}
    return 0;
}


/*Function: writeOutputFile

    Function to write the media file to disk.

    Checks if the file already exists and returns if it has. Checks filesize to see if file was completely written.

    Attempts to write the file. If this fails re-attempts with flush option set.

    Large files are handed seperatly to small files, defined by  being larger or smaller that <max_read>

    Could do with a tidy up to avoid repeated code. Possible error where 2 files with the same has value are trying to be written simultaneously.

    Where errors occur, the items are tagged with the corresponding report table for ease of finding later.

    Parameters:

        LONG nItemID        -   X-Ways ItemID of file being outputted
        int picFile         -   Flag to indicate if file is a picture
        wchar_t* fileName   -   Wide character string of filename, must be NULL terminated
        INT64 fileSize      -   Filesize as reported by X-Ways
        HANDLE hdlCurrVol   -   Handle to Current Volume being processed by X-Ways

    Returns:
        <SUCCESS>                 -   No Errors
        <ERROR_FILE_OPEN>         -   Error occurred when getting a HANDLE from X-Ways for nItemID
        <RETERR_FILE_READ>        -   Corresponding writeoutput returned an error other than filesize error
        <RETERR_SIZE_MISMATCH>    -   Size of output file does not match reported size by X-Ways

    See Also:
        Called by   -   <MainItemProcess>
        Calls       -   <ifFileExists>, <writeOutputFileSmall>, <writeOutputFileLarge>
*/

int writeOutputFile(LONG nItemID,int picFile,wchar_t* fileName, INT64 fileSize,HANDLE hdlCurrVol)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Start of WriteOutputFile Function Output");}
    char outputPath[2048];
    FILE* outputFile;
    generateFilePath(&outputPath[0],2048, fileName, picFile);
    if (ifFileExists(outputPath))
    {
        //no need to re-write file
        //1.41 check filesize first
        int sizeCheck = checkFileSize(nItemID,outputPath,fileSize);
        if (sizeCheck == 0)
        {
            if (extractInfo.debugSet){debugWriteDetails(nItemID, L"WriteOutputFile - Output file exists");}
            return SUCCESS;
        }
    }
    lockFile.lock();
    HANDLE hItem;

    //1.41 try catch does not work, code removed
    hItem = XWF_OpenItem(hdlCurrVol,nItemID,0);
    if (hItem == 0)
    {
        //1.38 move to report table
        //outputErrorMessage(L"Unable to open fileID: ",nItemID);
        errorRaised(nItemID,REPORT_FILEOPEN_ERROR);
        //clear file lock to prevent deadlock
        lockFile.unlock();
        return ERROR_FILE_OPEN;
    }
    outputFile = fopen(outputPath,"wb");
    lockFile.unlock();
    if (outputFile == NULL)
    {
        errorRaised(nItemID,REPORT_FILEOPEN_ERROR);
        return ERROR_FILE_OPEN;
    }
    int retVal=0;
    if (fileSize <= max_read){
        retVal = writeOutputFileSmall(outputFile,fileSize,hItem,nItemID);
    }
    else{
        retVal = writeOutputFileLarge(outputFile,fileSize,hItem,nItemID);
    }
    int closeValue = fclose(outputFile);
    outputFile = NULL;
    if (retVal !=0 || closeValue !=0)
    {
        if (retVal == FILE_ERROR_SIZE)
        {
            //retry with flush option
            hItem = XWF_OpenItem(hdlCurrVol,nItemID,0);
            outputFile = fopen(outputPath,"wb");
            if (outputFile != NULL)
            {
                if (fileSize <= max_read){
                    retVal = writeOutputFileSmall(outputFile,fileSize,hItem,nItemID);
                }
                else {
                    retVal = writeOutputFileLarge(outputFile,fileSize,hItem,nItemID,true);
                }
                fclose(outputFile);
                outputFile = NULL;
            }
        }
        if (retVal !=0){
            //1.38 change to report table rather than output message
            errorRaised(nItemID,REPORT_UNKNOWN_FILESIZE);
            //outputErrorMessage(L"Error reading file data from ItemID: ",nItemID);

            //delete partially written/corrupt file
            DeleteFile((char*)&outputPath);
            return RETERR_FILE_READ;
        }
    }
    //add check to make sure full file has been written
    retVal = checkFileSize(nItemID,outputPath,fileSize);
    if (retVal != 0){
        //1.38 move to report table
        //outputErrorMessage(L"Filesize written to disk does not match expected size for ItemID: ",nItemID);
        errorRaised(nItemID,REPORT_FILESIZE_MISMATCH);
        return RETERR_SIZE_MISMATCH;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"End of WriteOutputFile Function Output");}
    return SUCCESS;
}
