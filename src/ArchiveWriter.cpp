
//standard libraries
#include <mutex>
#include <set>
#include <string>

//project includes
#include "ArchiveWriter.h"
#include "X-Tension.h"
#include "utility.h"
#include "debugMessage.h"

/** @brief Depth of folders created within the archive based on hash values. */
#define folderDepth 2

/** @brief Maximum number of bytes read from a file in a single pass. */
#define max_read 4194304LL

/** @brief UTF-8 path to the picture output archive. */
char* archivePic=nullptr;

/** @brief UTF-8 path to the video output archive. */
char* archiveVid=nullptr;

/** @brief libarchive write object for picture files, or NULL if unused. */
struct archive* ArchPic=nullptr;

/** @brief libarchive write object for video files, or NULL if unused. */
struct archive* ArchVid=nullptr;

/** @brief libarchive write object used when picture and video paths are the same. */
struct archive* ArchAll=nullptr;

/** @brief Set of MD5 hash strings for files already written to the archive, used for deduplication. */
std::set<std::string> hashList;

/** @brief Mutex preventing simultaneous writes to a zip archive. */
std::mutex archiveLock;


/**
 * @brief Sets the output archive path from a wide character string.
 *
 * Appends "JSON export.zip" to the supplied path and stores the result as a
 * UTF-8 string in @p archivePic, @p archiveVid, or both depending on @p flags.
 *
 * @param path  Wide string containing the null-terminated output directory path.
 * @param flags Combination of SET_PIC_PATH and/or SET_VID_PATH.
 * @return      SUCCESS always.
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

/**
 * @brief Creates and opens archive object(s) for output.
 *
 * Where the picture and video paths are the same, a single archive object is
 * created. Otherwise a separate object is created for each non-null path.
 *
 * @return SUCCESS on success, ERROR_CREATE if an archive object could not be
 *         allocated, ERROR_FORMAT if the zip format could not be set, or
 *         ERROR_OPEN if the archive file could not be opened.
 */
int setupZipArchives()
{
    if (archivePic != nullptr && archiveVid != nullptr && strcmp(archivePic,archiveVid)==0)
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

/**
 * @brief Creates a new entry header in the archive.
 *
 * Sets the entry size, pathname and file mode. The entry is freed and set to
 * nullptr on failure.
 *
 * @param archFile  Pointer to the archive write object.
 * @param entry     Output parameter populated with the new archive entry.
 * @param filePath  Path for the file within the archive.
 * @param fileSize  Size of the file in bytes.
 * @return          SUCCESS on success, or ERROR_WRITE if the header could not
 *                  be written.
 *
 * @see writeArchiveFile, writeJSONFile
 */
int createZipArchiveEntry(struct archive** archFile, struct archive_entry** entry, const char* filePath,int64_t fileSize)
{
    *entry = archive_entry_new();
    archive_entry_set_size(*entry,fileSize);
    archive_entry_set_pathname(*entry, filePath);
    archive_entry_set_mode(*entry, AE_IFREG);
    if (archive_write_header(*archFile,*entry)!=ARCHIVE_OK)
    {
        archive_entry_free(*entry);
        *entry = nullptr;
        return ERROR_WRITE;
    }
    return SUCCESS;
}

/**
 * @brief Closes all open zip archives.
 *
 * @return SUCCESS always.
 *
 * @see closeZipArchive
 */
int closeZipArchives()
{
    closeZipArchive(&ArchPic);
    closeZipArchive(&ArchVid);
    closeZipArchive(&ArchAll);
    return SUCCESS;
}

/**
 * @brief Frees the archive path strings allocated by setArchivePath.
 *
 * Should be called at shutdown to avoid leaking archivePic and archiveVid.
 */
void cleanupArchivePaths()
{
    delete[] archivePic;
    archivePic = nullptr;
    delete[] archiveVid;
    archiveVid = nullptr;
}

/**
 * @brief Frees a zip archive entry.
 *
 * @param archFile Pointer to the archive write object (unused, retained for API symmetry).
 * @param entry    The entry to free.
 * @return         SUCCESS always.
 *
 * @see writeJSONFile
 */
int closeZipArchiveEntry(struct archive** archFile, struct archive_entry* entry)
{
    archive_entry_free(entry);
    return SUCCESS;
}

/**
 * @brief Closes and frees a single zip archive.
 *
 * @param archFile Pointer to the archive write object to close.
 * @return         SUCCESS on success, or ERROR_CLOSE if archive_write_free fails.
 *
 * @see closeZipArchives
 */
int closeZipArchive(struct archive** archFile)
{
    if (archive_write_free(*archFile)!=ARCHIVE_OK)
        return ERROR_CLOSE;
    return SUCCESS;
}

/**
 * @brief Generates the in-archive file path for a file based on its MD5 filename.
 *
 * Builds a two-level folder hierarchy using the first four characters of the
 * filename: Files\\<c0><c1>\\<c2><c3>\\<filename>
 *
 * The returned buffer is allocated with new[] and must be freed by the caller
 * using delete[].
 *
 * @param fileName Wide character string of the MD5 hash used as the filename.
 * @return         Newly allocated char buffer containing the in-archive path.
 *
 * @see writeArchiveFile
 */
char* generateCompressedPath(wchar_t* fileName)
{
    char* retBuffer = new char[128];
    // Build: Files\<c0><c1>\<c2><c3>\<fileName>  (folderDepth = 2)
    snprintf(retBuffer, 128, "Files\\%lc%lc\\%lc%lc\\%ls",
             fileName[0], fileName[1],
             fileName[2], fileName[3],
             fileName);
    return retBuffer;
}

/**
 * @brief Selects the appropriate archive write object based on file type.
 *
 * Returns ArchAll if a combined archive is in use, otherwise returns ArchPic
 * for pictures or ArchVid for all other files.
 *
 * @param picFile True if the file is a picture, false for video.
 * @return        Pointer to the selected archive write object.
 *
 * @see writeArchiveFile, writeJSONFile
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
}

/**
 * @brief Writes an existing file from disk into the zip archive.
 *
 * @param inFilePath Path to the source file on disk.
 * @param filename   Path for the file within the archive.
 * @param picFile    True if the file is a picture.
 * @return           SUCCESS on success, or ERROR_OPEN if the file size could
 *                   not be determined or the file could not be opened.
 *
 * @see fileSize, createZipArchiveEntry
 */
int writeJSONFile(const char* inFilePath, const char* filename, bool picFile)
{
    struct archive *outa = selectArchiveObject(picFile);
    struct archive_entry *entry;
    size_t bytesRead=0;

    INT64 fSize = fileSize(inFilePath);
    if (fSize < 0)
        return ERROR_OPEN;
    int result = createZipArchiveEntry(&outa,&entry,filename,fSize);
    unsigned char* buffer = new unsigned char[max_read+1];
    FILE* inputFile = fopen(inFilePath,"rb");
    if (inputFile != NULL)
    {
        while ((bytesRead = fread(buffer,1,max_read, inputFile)) > 0)
        {
            archive_write_data(outa,buffer,bytesRead);
        }
        fclose(inputFile);
    }
    closeZipArchiveEntry(&outa, entry);
    delete[] buffer;
    return SUCCESS;
}

/**
 * @brief Writes a file from X-Ways into the zip archive, deduplicating by MD5.
 *
 * Checks whether the file's MD5 hash has already been written. If so, returns
 * SUCCESS immediately. Otherwise opens the item via XWF_OpenItem and writes its
 * contents to the archive.
 *
 * @param nItemID    X-Ways item ID of the file to write.
 * @param picFile    True if the file is a picture, false for video.
 * @param fileName   Wide character string of the MD5 hash used as the filename.
 * @param fileSize   Size of the file in bytes.
 * @param hdlCurrVol Handle to the current volume being processed.
 * @return           SUCCESS if the file was written or already existed,
 *                   ERROR_WRITE if the archive entry could not be created, or
 *                   1 if the item handle could not be opened.
 *
 * @see generateCompressedPath, createZipArchiveEntry
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
    if (createZipArchiveEntry(&outa,&entry,filePath,fileSize) != SUCCESS)
    {
        XWF_Close(hItem);
        archiveLock.unlock();
        delete[] filePath;
        return ERROR_WRITE;
    }

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



