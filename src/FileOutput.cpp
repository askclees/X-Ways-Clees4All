//standard libraries
#include <windows.h>
#include <cstdint>
#include <shlobj.h>

//local functions
#include "main.h"
#include "FileOutput.h"
#include "debugMessage.h"
#include "utility.h"

/** @brief Depth of folder hierarchy created from the MD5 hash prefix. */
#define folderDepth 2

/** @brief Maximum number of bytes read from an item in a single pass (4 MiB). */
#define max_read 4194304LL

/** @brief Mutex serialising file creation to prevent simultaneous writes of the same file. */
CRITICAL_SECTION lockFile;

/** @brief Mutex serialising directory creation to prevent duplicate mkdir calls. */
CRITICAL_SECTION lockFolder;

void initFileOutputLocks()    { InitializeCriticalSection(&lockFile); InitializeCriticalSection(&lockFolder); }
void destroyFileOutputLocks() { DeleteCriticalSection(&lockFile);     DeleteCriticalSection(&lockFolder); }

/**
 * @brief Generates a relative folder path from an MD5 hash filename.
 *
 * Builds a two-level hierarchy under "Files" using the first four characters
 * of @p fileName: Files\\<c0><c1>\\<c2><c3>
 *
 * @param buffer       Output buffer to write the path into.
 * @param sizeBuffer   Size of @p buffer in bytes.
 * @param fileName     Wide character string of the MD5 hash used as the filename.
 * @param escapedSlash If true, folder separators are written as \\\\ (for VICS JSON output).
 * @return             0 always.
 *
 * @see generateFilePath
 */
int generateRelativeFilePath(char* buffer, int sizeBuffer, wchar_t* fileName, bool escapedSlash)
{
    int pos = snprintf(buffer, sizeBuffer, "Files");
    if (pos < 0) pos = 0;
    const char* sep = escapedSlash ? "\\\\%lc%lc" : "\\%lc%lc";
    for (int i = 0; i < folderDepth && pos < sizeBuffer - 1; i++)
    {
        int written = snprintf(buffer + pos, sizeBuffer - pos, sep, fileName[i*2], fileName[(i*2)+1]);
        if (written > 0) pos += written;
    }
    return 0;
}

/**
 * @brief Generates the full output path for an exported media file and creates the directory.
 *
 * Combines the base output path (picture or video) with the relative folder hierarchy
 * derived from the MD5 hash, creates the directory if it does not exist, then appends
 * the filename.
 *
 * @param buffer   Output buffer to write the full path into.
 * @param maxSize  Size of @p buffer in bytes.
 * @param fileName Wide character string of the MD5 hash used as the filename.
 * @param picFile  True if the file is a picture (uses C4PPath), false for video (uses C4MPath).
 * @return         0 always.
 *
 * @see generateRelativeFilePath, writeOutputFile
 */
static int generateFilePath(char* buffer, int maxSize,wchar_t* fileName, bool picFile)
{
    //generate file path
    char tempBuffer[128]={0};
    int pos;
    if (picFile)
    {
        pos = snprintf(buffer, maxSize, "%ls\\", extractInfo.C4PPath);
    }
    else
    {
        pos = snprintf(buffer, maxSize, "%ls\\", extractInfo.C4MPath);
    }
    if (pos < 0) pos = 0;
    //generate subfolder to split out number of files.
    generateRelativeFilePath(tempBuffer, 128, fileName, false);
    int written = snprintf(buffer + pos, maxSize - pos, "%s", tempBuffer);
    if (written > 0) pos += written;
    //lock folder mutex to stop folder being created twice.
    EnterCriticalSection(&lockFolder);
    if (!ifFileExists(buffer))
    {
        //create directory
        int retVal = SHCreateDirectoryEx(NULL,buffer,NULL);
        if (retVal !=0 && retVal != ERROR_FILE_EXISTS && retVal != ERROR_ALREADY_EXISTS)
        {
            outputErrorMessage(L"Error creating directory, Error Code:",retVal);
        }
    }
    LeaveCriticalSection(&lockFolder);
    written = snprintf(buffer + pos, maxSize - pos, "\\%ls", fileName);
    return 0;
}

/**
 * @brief Writes a file larger than max_read to disk in buffered chunks.
 *
 * Reads the item from X-Ways in max_read sized chunks until the full file has
 * been written. Closes @p hItem on both success and failure.
 *
 * @param fileOut  Open file handle to write to.
 * @param fileSize Size of the file in bytes.
 * @param hItem    X-Ways handle to the item to read.
 * @param nItemID  X-Ways item ID (used for debug output).
 * @param flush    If true, flushes the output file after each chunk.
 * @return         0 on success, 1 if a read returned zero or fewer bytes than requested.
 *
 * @see writeOutputFile
 */
static int writeOutputFileLarge(FILE* fileOut, INT64 fileSize,HANDLE hItem,LONG nItemID, bool flush)
{
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"Start of writeOutputFileLarge Function Output");}
    BYTE* buffer = new BYTE[max_read+1];
    INT64 currentOffset = 0;
    bool done = false;
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
            done = true;
        }
        memset(buffer,0,readSize);
        currentOffset = currentOffset + readSize;
        XWF_ShouldStop();
    }
    while (!done);
    XWF_Close(hItem);
    delete[] buffer;
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"End of writeOutputFileLarge Function Output");}
    return 0;
}

/** @brief Overload of writeOutputFileLarge with flush defaulting to false. */
static int writeOutputFileLarge(FILE* fileOut, INT64 fileSize,HANDLE hItem,LONG nItemID)
{
    return writeOutputFileLarge(fileOut,fileSize,hItem,nItemID,false);
}

/**
 * @brief Writes a file smaller than or equal to max_read to disk in a single read.
 *
 * Reads the entire item from X-Ways in one call. Tolerates a size difference of
 * one byte between the reported file size and the actual bytes read. Closes
 * @p hItem on both success and failure.
 *
 * @param fileOut  Open file handle to write to.
 * @param fileSize Size of the file in bytes as reported by X-Ways.
 * @param hItem    X-Ways handle to the item to read.
 * @param nItemID  X-Ways item ID (used for debug output).
 * @return         0 on success, 1 if the read returned zero or an unexpected size.
 *
 * @see writeOutputFile
 */
static int writeOutputFileSmall(FILE* fileOut, INT64 fileSize,HANDLE hItem,LONG nItemID)
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
            int64_t* fileSizeVar = new int64_t(fileSize);
            errDetails.entries[errDetails.noVars].varData = fileSizeVar;
            errDetails.entries[errDetails.noVars].varLen = 8;
            errDetails.entries[errDetails.noVars].type ='i';
            errDetails.noVars++;
            int32_t* readVar = new int32_t(read);
            errDetails.entries[errDetails.noVars].varData = readVar;
            errDetails.entries[errDetails.noVars].varLen = 4;
            errDetails.entries[errDetails.noVars].type ='i';
            errDetails.noVars++;
            debugWriteDetails(nItemID, L"writeOutputFileSmall",L"Read value 0 or not matching filesize",errDetails);
            delete fileSizeVar;
            delete readVar;
        }
        return 1;
    }
    fwrite(buffer,read,1,fileOut);
    XWF_Close(hItem);
    delete[] buffer;
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"End of writeOutputFileSmall Function Output");}
    return 0;
}

/**
 * @brief Checks that the size of a file on disk matches the size reported by X-Ways.
 *
 * Polls up to 10 times with a 250ms delay to allow the file system to flush.
 * Allows a tolerance of ±2 bytes.
 *
 * @param nItemID  X-Ways item ID (used for debug output).
 * @param path     Path to the file on disk.
 * @param fileSize Expected file size in bytes as reported by X-Ways.
 * @return         0 if sizes match, 1 if they do not, or -1 if the file attributes
 *                 could not be read.
 *
 * @see writeOutputFile
 */
static int checkFileSize(LONG nItemID,char* path,INT64 fileSize)
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

/**
 * @brief Writes a media file from X-Ways to disk, with size verification and retry on failure.
 *
 * Checks whether the file already exists with the correct size before writing.
 * Files up to max_read are written in a single read; larger files are written in
 * chunks. If the initial write fails due to a size mismatch, one retry is attempted
 * with flushing enabled. Errors are recorded in the appropriate X-Ways report table.
 *
 * @param nItemID    X-Ways item ID of the file to write.
 * @param picFile    True if the file is a picture, false for video.
 * @param fileName   Wide character string of the MD5 hash used as the filename.
 * @param fileSize   Size of the file in bytes as reported by X-Ways.
 * @param hdlCurrVol Handle to the current volume being processed.
 * @return           SUCCESS on success, ERROR_FILE_OPEN if the item or output file
 *                   could not be opened, RETERR_FILE_READ on a read error, or
 *                   RETERR_SIZE_MISMATCH if the written file size does not match.
 *
 * @see generateFilePath, writeOutputFileSmall, writeOutputFileLarge, checkFileSize
 */
int writeOutputFile(LONG nItemID,bool picFile,wchar_t* fileName, INT64 fileSize,HANDLE hdlCurrVol)
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
    EnterCriticalSection(&lockFile);
    HANDLE hItem;

    hItem = XWF_OpenItem(hdlCurrVol,nItemID,0);
    if (hItem == 0)
    {
        errorRaised(nItemID,REPORT_FILEOPEN_ERROR);
        //clear file lock to prevent deadlock
        LeaveCriticalSection(&lockFile);
        return ERROR_FILE_OPEN;
    }
    outputFile = fopen(outputPath,"wb");
    LeaveCriticalSection(&lockFile);
    if (outputFile == NULL)
    {
        XWF_Close(hItem);
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
            if (hItem != 0)
            {
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
                else
                {
                    XWF_Close(hItem);
                }
            }
        }
        if (retVal !=0){
            errorRaised(nItemID,REPORT_UNKNOWN_FILESIZE);
            //delete partially written/corrupt file
            DeleteFileA(outputPath);
            return RETERR_FILE_READ;
        }
    }
    //add check to make sure full file has been written
    retVal = checkFileSize(nItemID,outputPath,fileSize);
    if (retVal != 0){
        errorRaised(nItemID,REPORT_FILESIZE_MISMATCH);
        return RETERR_SIZE_MISMATCH;
    }
    if (extractInfo.debugSet){debugWriteDetails(nItemID, L"End of WriteOutputFile Function Output");}
    return SUCCESS;
}
