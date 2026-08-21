
//standard libraries
#include <windows.h>
#include <set>
#include <string>
#include <utility>

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

/** @brief Set of (destination archive, MD5 hash) pairs for files already written, used for deduplication.
 *  Keyed per destination archive object so a hash written to one archive (e.g. ArchVid) doesn't
 *  suppress writing it to a different, separate archive (e.g. ArchPic). */
std::set<std::pair<struct archive*, std::string>> hashList;

/** @brief Critical section preventing simultaneous writes to a zip archive. */
CRITICAL_SECTION archiveLock;

/** @brief Initialises archiveLock. Must be called before any archive writes are attempted. */
void initArchiveLocks()    { InitializeCriticalSection(&archiveLock); }
/** @brief Releases the resources held by archiveLock. */
void destroyArchiveLocks() { DeleteCriticalSection(&archiveLock); }


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
    swprintf(tempStr,pathLen,L"%lsJSON export.zip",path);
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
        {
            closeZipArchive(&ArchAll);
            return ERROR_FORMAT;
        }
        if (archive_write_open_filename(ArchAll, archivePic) != ARCHIVE_OK)
        {
            closeZipArchive(&ArchAll);
            return ERROR_OPEN;
        }
    }
    else
    {
        if (archivePic != nullptr)
        {
            ArchPic = archive_write_new();
            if (ArchPic == NULL)
                return ERROR_CREATE;
            if (archive_write_set_format_zip(ArchPic) != ARCHIVE_OK)
            {
                closeZipArchive(&ArchPic);
                return ERROR_FORMAT;
            }
            if (archive_write_open_filename(ArchPic, archivePic) != ARCHIVE_OK)
            {
                closeZipArchive(&ArchPic);
                return ERROR_OPEN;
            }
        }
        if (archiveVid != nullptr)
        {
            ArchVid = archive_write_new();
            //ArchPic may already be open at this point - on any failure below it must be
            //freed too, otherwise its open file handle leaks for the rest of the X-Ways
            //session (setupZipArchives isn't retried, and caseCleanup is never reached
            //since the caller aborts XT_Prepare on a non-SUCCESS return)
            if (ArchVid == NULL)
            {
                if (ArchPic != nullptr) { closeZipArchive(&ArchPic); }
                return ERROR_CREATE;
            }
            if (archive_write_set_format_zip(ArchVid) != ARCHIVE_OK)
            {
                closeZipArchive(&ArchVid);
                if (ArchPic != nullptr) { closeZipArchive(&ArchPic); }
                return ERROR_FORMAT;
            }
            if (archive_write_open_filename(ArchVid, archiveVid) != ARCHIVE_OK)
            {
                closeZipArchive(&ArchVid);
                if (ArchPic != nullptr) { closeZipArchive(&ArchPic); }
                return ERROR_OPEN;
            }
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
 * Also clears the dedup hashList, since its keys are paired with the archive pointers
 * being freed here - a stale entry could otherwise collide with an unrelated archive
 * object a later run happens to allocate at the same address.
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
    hashList.clear();
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
    //archive_write_free releases all resources regardless of its return code, so the pointer
    //must always be nulled here - otherwise a stale pointer from this run can be mistaken for
    //a live archive by setupZipArchives()/selectArchiveObject() on a later run in the same
    //X-Ways session (e.g. switching between a combined and a separate picture/video output
    //config leaves whichever archive pointer the new config doesn't use dangling)
    int result = (archive_write_free(*archFile)!=ARCHIVE_OK) ? ERROR_CLOSE : SUCCESS;
    *archFile = NULL;
    return result;
}

/**
 * @brief Frees and nulls the shared archive object that a short/failed write left in an
 *        indeterminate state.
 *
 * libarchive requires archive_write_free() after a write error - the entry's declared
 * size (set up front via archive_entry_set_size) no longer matches the bytes actually
 * written, so the zip stream can't be trusted for any further entries. Nulling the
 * relevant global here makes selectArchiveObject() return NULL afterwards, so later
 * writeArchiveFile/writeJSONFile calls fail fast with ERROR_WRITE instead of silently
 * corrupting more entries into the same archive.
 *
 * @param picFile True if the failed write was for a picture, false for video.
 */
void invalidateArchiveObject(bool picFile)
{
    if (ArchAll != nullptr)
    {
        closeZipArchive(&ArchAll);
    }
    else if (picFile)
    {
        if (ArchPic != nullptr) { closeZipArchive(&ArchPic); }
    }
    else
    {
        if (ArchVid != nullptr) { closeZipArchive(&ArchVid); }
    }
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
 * @return           SUCCESS on success, ERROR_OPEN if the file size could not
 *                   be determined or the file could not be opened, ERROR_WRITE
 *                   if the archive entry header could not be written, or
 *                   ERROR_READ if reading the source file fails partway through.
 *
 * @see getFileSize, createZipArchiveEntry
 */
int writeJSONFile(const char* inFilePath, const char* filename, bool picFile)
{
    struct archive *outa = selectArchiveObject(picFile);
    if (outa == nullptr)
        return ERROR_WRITE;
    struct archive_entry *entry;
    size_t bytesRead=0;

    INT64 fSize = getFileSize(inFilePath);
    if (fSize < 0)
        return ERROR_OPEN;
    FILE* inputFile = fopen(inFilePath,"rb");
    if (inputFile == NULL)
        return ERROR_OPEN;
    int result = createZipArchiveEntry(&outa,&entry,filename,fSize);
    if (result != SUCCESS)
    {
        fclose(inputFile);
        return result;
    }
    unsigned char* buffer = new unsigned char[max_read+1];
    while ((bytesRead = fread(buffer,1,max_read, inputFile)) > 0)
    {
        la_ssize_t written = archive_write_data(outa,buffer,bytesRead);
        if (written < 0 || (size_t)written != bytesRead)
        {
            fclose(inputFile);
            closeZipArchiveEntry(&outa, entry);
            delete[] buffer;
            //the entry's declared size no longer matches what was actually written -
            //the archive object can't be trusted for any further entries
            invalidateArchiveObject(picFile);
            return ERROR_WRITE;
        }
    }
    if (ferror(inputFile))
    {
        fclose(inputFile);
        closeZipArchiveEntry(&outa, entry);
        delete[] buffer;
        invalidateArchiveObject(picFile);
        return ERROR_READ;
    }
    fclose(inputFile);
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
    EnterCriticalSection(&archiveLock);
    struct archive *outa = selectArchiveObject(picFile);
    if (outa == nullptr)
    {
        //a previous write on this archive failed and invalidated it - fail fast rather than
        //dereferencing a freed archive object
        LeaveCriticalSection(&archiveLock);
        return ERROR_WRITE;
    }
    char* filePath = generateCompressedPath(fileName);
    bool fileFound = false;
    std::wstring wFileName(fileName);
    std::string hashValue(wFileName.begin(), wFileName.end());
    auto hashKey = std::make_pair(outa, hashValue);
    fileFound = hashList.count(hashKey);

    if (fileFound)
    {
        LeaveCriticalSection(&archiveLock);
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
        LeaveCriticalSection(&archiveLock);
        delete[] filePath;
        return 1;
    }
    struct archive_entry *entry;
    if (createZipArchiveEntry(&outa,&entry,filePath,fileSize) != SUCCESS)
    {
        XWF_Close(hItem);
        LeaveCriticalSection(&archiveLock);
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
        if (read != readSize)
        {
            XWF_Close(hItem);
            delete[] buffer;
            closeZipArchiveEntry(&outa, entry);
            //entry's declared size (fileSize) no longer matches what was actually written -
            //the archive object can't be trusted for any further entries
            invalidateArchiveObject(picFile);
            LeaveCriticalSection(&archiveLock);
            delete[] filePath;
            return ERROR_WRITE;
        }
        la_ssize_t written = archive_write_data(outa,buffer,read);
        if (written < 0 || (DWORD)written != read)
        {
            XWF_Close(hItem);
            delete[] buffer;
            closeZipArchiveEntry(&outa, entry);
            invalidateArchiveObject(picFile);
            LeaveCriticalSection(&archiveLock);
            delete[] filePath;
            return ERROR_WRITE;
        }
        currOffset += readSize;
    }
    //close handle!
    XWF_Close(hItem);
    delete[] buffer;
    closeZipArchiveEntry(&outa, entry);
    hashList.insert(hashKey);
    //end of function, unlock file
    LeaveCriticalSection(&archiveLock);
    delete[] filePath;
    return SUCCESS;
}



