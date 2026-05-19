
//standard libraries
#include <mutex>
#include <set>
#include <string>

//project includes
#include "ArchiveWriter.h"
#include "X-Tension.h"
#include "utility.h"
#include "debugMessage.h"

//defines
/* Constant: folderDepth
Depth of folders to be created based on hash values. Currently set to 2*/
#define folderDepth 2

//define max read size.
/* Constant: max_read
Max size to be read in a single pass. Currently set to 4194304LL*/
#define max_read 4194304LL

//variables for holding paths for different archives
char* archivePic=nullptr;
char* archiveVid=nullptr;

//define a global writer
struct archive* ArchPic=nullptr;
struct archive* ArchVid=nullptr;
struct archive* ArchAll=nullptr;

/* Variable: hashList
std::set of std::string that's used to keep a list of files that have been written to an archive.
*/
std::set<std::string> hashList;

/* Variable: archiveLock
std::mutex used to stop simultaneous writes to a zip archive.*/

std::mutex archiveLock;


/*Section: Archive Creation Functions
    This section contains all the functions required to write to a zip archive.

    Added in version 1.50

*/


/*Function: setArchivePath
    Function that sets the output location in UTF-8 from wide character string.

    Parameters:
        wchar_t* path   -   Wide string containing Null terminated output path
        int flags       -   SET_PIC_PATH or SET_VID_PATH are valid flags. Can be both if single archive

    Returns:
        SUCCESS     -   Always

    See Also:
        Calls   -   <loadOrCreateOptions>
*/

int setArchivePath(wchar_t* path, int flags)
{
    wchar_t* tempStr;
    int pathLen = wcslen(path) + 64;
    tempStr = new wchar_t[pathLen];
    swprintf(tempStr,L"%lsJSON export.zip",path);
    if (flags & SET_PIC_PATH)
    {
        archivePic = convertWideToChar(tempStr);
    }
    if (flags & SET_VID_PATH)
    {
        archiveVid = convertWideToChar(tempStr);
    }
    delete[] tempStr;
    return SUCCESS;

}

/*Function: setupZipArchives
    Function that sets up object(s) for output archive(s).

    Where the output path for pictures and videos is the same, use a single archive.

    Otherwise create an archive and corresponding object in each location.

    Returns:
        SUCCESS         -   Function successfully completed
        ERROR_CREATE    -   Error creating Archive Object
        ERROR_FORMAT    -   Error setting as zip format
        ERROR_OPEN      -   Error opening file

*/

int setupZipArchives()
{
    if (strcmp(archivePic,archiveVid)==0)
    {
        //same location for both
        ArchAll = archive_write_new();
        if (ArchAll == NULL)
            return ERROR_CREATE;
        if (archive_write_set_format_zip(ArchAll) != ARCHIVE_OK)
            return ERROR_FORMAT;
        if (archive_write_open_filename(ArchAll, archivePic) != ARCHIVE_OK)
            return ERROR_OPEN;
    }
    else
    {
        if (archivePic != nullptr)
        {
            ArchPic = archive_write_new();
            if (ArchPic == NULL)
                return ERROR_CREATE;
            if (archive_write_set_format_zip(ArchPic) != ARCHIVE_OK)
                return ERROR_FORMAT;
            if (archive_write_open_filename(ArchPic, archivePic) != ARCHIVE_OK)
                return ERROR_OPEN;
        }
        if (archiveVid != nullptr)
        {
            ArchVid = archive_write_new();
            if (ArchVid == NULL)
                return ERROR_CREATE;
            if (archive_write_set_format_zip(ArchVid) != ARCHIVE_OK)
                return ERROR_FORMAT;
            if (archive_write_open_filename(ArchVid, archiveVid) != ARCHIVE_OK)
                return ERROR_OPEN;
        }
    }
    return SUCCESS;
}

/*Function: createZipArchiveEntry
    Function that creates a new archive entry header and sets the size,
    pathname and filemode

    Required prior to writing data for a file.

    Returns:
        SUCCESS         -   Function successfully completed
        ERROR_WRITE     -   Error writing archive entry

    See Also:
        Called from     -   <writeArchiveFile>
*/

int createZipArchiveEntry(struct archive** archFile, struct archive_entry** entry, const char* filePath,int64_t fileSize)
{
    *entry = archive_entry_new();
    archive_entry_set_size(*entry,fileSize);
    archive_entry_set_pathname(*entry, filePath);
    archive_entry_set_mode(*entry, AE_IFREG);
    if (archive_write_header(*archFile,*entry)!=ARCHIVE_OK)
        return ERROR_WRITE;
    return SUCCESS;
}

/*Section: Archive Closing Functions*/

/*Function: closeZipArchives
    Function that closes the zip archives.

    Currently does not check for errors or if the archive is in use.

    Returns:
        SUCCESS         -   Always

    See Also:
        Calls   -   <closeZipArchiveEntry>

*/

int closeZipArchives()
{
    closeZipArchive(&ArchPic);
    closeZipArchive(&ArchVid);
    closeZipArchive(&ArchAll);
    return SUCCESS;
}

/*Function: closeZipArchiveEntry
    Function that closes a zip file entry

    Currently does not check for errors

    Returns:
        SUCCESS         -   Always

    See Also:
        Called by       -   <writeJSONFile>

*/

int closeZipArchiveEntry(struct archive** archFile, struct archive_entry* entry)
{
    archive_entry_free(entry);
    return SUCCESS;
}

/*Function: closeZipArchive
    Function that closes a single zip archive.

    Currently does not check for errors or if the archive is in use.

    Returns:
        SUCCESS         -   Always

    See Also:
        Called by       -   <closeZipArchives>

*/

int closeZipArchive(struct archive** archFile)
{
    if (archive_write_free(*archFile)!=ARCHIVE_OK)
        return ERROR_CLOSE;
    return SUCCESS;
}

/*Section: Archive Utility Functions*/

/*Function: generateCompressedPath
    Function that generates the file path for a file in zip based on filename

    Returned buffer needs to be freed by calling function

    Parameters:
        wchar_t* filename   -   wide character string of the MD5 has filename

    Returns:
        char*           -   Buffer containing the filename

    See Also:
        Called by       -   <closeZipArchives>

*/

char* generateCompressedPath(wchar_t* fileName)
{
    char* retBuffer= new char[128];
    char tempBuffer[128] ={0};
    retBuffer[0] = '\0';
    strncat(retBuffer,"Files",128);
    for (int i=0;i<folderDepth;i++)
    {
        snprintf(tempBuffer,128,"\\%lc%lc",fileName[i*2],fileName[(i*2)+1]);
        strncat(retBuffer,tempBuffer,128);
        tempBuffer[0]='\0';
    }
    snprintf(tempBuffer,128,"\\%ls",fileName);
    strncat(retBuffer,tempBuffer,128);
    return retBuffer;
}

/*Function: selectArchiveObject
    Function that selects the correct Archive object.

    Decision based on whether item is picture or not.

    Parameters:
        bool picFile   -   Flag for whether

    Returns:
        struct archive*     -   Buffer containing the filename

    See Also:
        Called by       -   <closeZipArchives>

*/

struct archive* selectArchiveObject(bool picFile)
{
    if (ArchAll != nullptr)
    {
        return ArchAll;
    }
    else if (picFile)
    {
        return ArchPic;
    }
    else
    {
        return ArchVid;
    }
};

/*Section: Archive Writing Functions*/

/*Function: writeJSONFile
    Function that writes an existing JSON file to the zip archive

    In theory, can be used for any live file.

    Parameters:
        char* inFilePath    -   File path to file on disk
        char* filename      -   Name of file to be written
        bool picFile        -   Whether item being written is a picture

    Returns:
        SUCCESS -   Always

    See Also:
        Calls - <FileSize>
*/

int writeJSONFile(char* inFilePath, char* filename, bool picFile)
{
    struct archive *outa = selectArchiveObject(picFile);
    struct archive_entry *entry;
    size_t bytesRead=0;

    int result = createZipArchiveEntry(&outa,&entry,filename,FileSize(inFilePath));
    INT64 currOffset = 0;
    unsigned char* buffer = new unsigned char[max_read+1];
    FILE* inputFile = fopen(inFilePath,"rb");
    if (inputFile != NULL)
    {
        while ((bytesRead = fread(buffer,1,max_read, inputFile)) > 0)
        {
            archive_write_data(outa,buffer,bytesRead);
        }
    }
    fclose(inputFile);
    closeZipArchiveEntry(&outa, entry);
    delete[] buffer;
    return SUCCESS;
}

/*Function: writeArchiveFile
    Function that writes a file from X-Ways to ZIP file.

    The hashList set contains the MD5 hashes of all files that have been
    written to the zip file already

    Function checks if MD5 already exists and, if so, returns success

    If not already in, attempts to write. Will error if a valid handle is
    not returned from XWF_OpenItem function

    Parameters:
        LONG nItemID        -   X-Ways Item ID of file to be written to zip
        bool picFile        -   Flag to state whether file is a picture or not
        wchar_t* fileName   -   wide character string of MD5 hash as filename
        INT64 fileSize      -   Size of file in bytes
        HANDLE hdlCurrVol   -   Handle to the Current Volume being processed

    Returns:
        SUCCESS -   If file already exists or is written successfully
        1       -   On error opening handle to file.

    See Also:
        Calls - <generateCompressedPath>
*/

int writeArchiveFile(LONG nItemID,bool picFile,wchar_t* fileName, INT64 fileSize,HANDLE hdlCurrVol)
{
    archiveLock.lock();
    char* filePath = generateCompressedPath(fileName);
    bool fileFound = false;
    std::wstring wFileName(fileName);
    std::string hashValue(wFileName.begin(), wFileName.end());
    fileFound = hashList.count(hashValue);

    if (fileFound)
    {
        archiveLock.unlock();
        delete[] filePath;
        return SUCCESS;
    }
    //file does not exist, create it
    HANDLE hItem;
    hItem = XWF_OpenItem(hdlCurrVol,nItemID,0);
    if (hItem == 0)
    {
        errorRaised(nItemID,REPORT_FILEOPEN_ERROR);
        //clear file lock to prevent deadlock
        archiveLock.unlock();
        delete[] filePath;
        return 1;
    }
    struct archive *outa = selectArchiveObject(picFile);
    struct archive_entry *entry;
    int result = createZipArchiveEntry(&outa,&entry,filePath,fileSize);

    //file writing section
    INT64 currOffset = 0;
    unsigned char* buffer = new unsigned char[max_read+1];
    while (currOffset < fileSize)
    {
        DWORD readSize;
        if (fileSize - currOffset > max_read)
        {
            readSize = max_read;
        }
        else
        {
            readSize = fileSize - currOffset;
        }
        DWORD read = XWF_Read(hItem,currOffset,buffer,readSize);
        if (read > 0)
        {
            archive_write_data(outa,buffer,read);
        }
        currOffset += readSize;
    }
    //close handle!
    XWF_Close(hItem);
    delete[] buffer;
    closeZipArchiveEntry(&outa, entry);
    hashList.insert(hashValue);
    //end of function, unlock file
    archiveLock.unlock();
    delete[] filePath;
    return SUCCESS;
}



