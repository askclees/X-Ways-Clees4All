#ifndef ARCHIVEWRITER_H_INCLUDED
#define ARCHIVEWRITER_H_INCLUDED

#define LIBARCHIVE_STATIC
#include "archive.h"
#include "archive_entry.h"

/** @brief Returned by archive functions on success. */
#define SUCCESS 0

/** @brief Returned when a libarchive write object could not be allocated. */
#define ERROR_CREATE        1
/** @brief Returned when a compression format could not be set (reserved). */
#define ERROR_COMPRESSION   2
/** @brief Returned when the zip output format could not be set. */
#define ERROR_FORMAT        3
/** @brief Returned when the archive file could not be opened. */
#define ERROR_OPEN          4

/** @brief Returned when an archive entry header could not be written. */
#define ERROR_WRITE         5

/** @brief Returned when archive_write_free fails during close. */
#define ERROR_CLOSE         6

/** @brief Returned when reading the source file fails partway through (as opposed to a clean EOF). */
#define ERROR_READ           7

/** @brief Flag passed to setArchivePath to set the picture archive path. */
#define SET_PIC_PATH    1
/** @brief Flag passed to setArchivePath to set the video archive path. */
#define SET_VID_PATH    2

/** @brief Creates a new entry header in the archive. */
int createZipArchiveEntry(struct archive** archFile, struct archive_entry** entry, const char* filePath,int64_t fileSize);
/** @brief Frees a zip archive entry. */
int closeZipArchiveEntry(struct archive** archFile, struct archive_entry* entry);
/** @brief Closes and frees a single zip archive. */
int closeZipArchive(struct archive** archFile);
/** @brief Sets the output archive path from a wide character string. */
int setArchivePath(wchar_t* path, int flags);
/** @brief Writes a file from X-Ways into the zip archive, deduplicating by MD5. */
int writeArchiveFile(LONG nItemID,bool picFile,wchar_t* fileName, INT64 fileSize,HANDLE hdlCurrVol);
/** @brief Creates and opens archive object(s) for output. */
int setupZipArchives();
/** @brief Closes all open zip archives. */
int closeZipArchives();
/** @brief Frees the archive path strings allocated by setArchivePath. */
void cleanupArchivePaths();
/** @brief Writes an existing file from disk into the zip archive. */
int writeJSONFile(const char* inFilePath, const char* filename, bool picFile);

void initArchiveLocks();
void destroyArchiveLocks();

#endif // ARCHIVEWRITER_H_INCLUDED
