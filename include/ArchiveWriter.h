#ifndef ARCHIVEWRITER_H_INCLUDED
#define ARCHIVEWRITER_H_INCLUDED

#include "archive.h"
#include "archive_entry.h"

//define error codes
//generic
#define SUCCESS 0

#define LIBARCHIVE_STATIC

//Archive open errors
/* Constants: Open Error Types

    ERROR_CREATE        -   1
    ERROR_COMPRESSION   -   2
    ERROR_FORMAT        -   3
    ERROR_OPEN          -   4

*/
#define ERROR_CREATE        1
#define ERROR_COMPRESSION   2
#define ERROR_FORMAT        3
#define ERROR_OPEN          4

//Archive entry Creation
#define ERROR_WRITE         1

//Archive Close
#define ERROR_CLOSE         1

//Case path flags
/*Constant
    define SET_PIC_PATH    1
    define SET_VID_PATH    2
*/
#define SET_PIC_PATH    1
#define SET_VID_PATH    2

int openZipArchive(const char* filename, struct archive** archFile);
int createZipArchiveEntry(struct archive** archFile, struct archive_entry** entry, const char* filePath,int64_t fileSize);
int closeZipArchiveEntry(struct archive** archFile, struct archive_entry* entry);
int closeZipArchive(struct archive** archFile);
bool findFile(const char* filename);
int setArchivePath(wchar_t* path, int flags);
int writeArchiveFile(LONG nItemID,bool picFile,wchar_t* fileName, INT64 fileSize,HANDLE hdlCurrVol);
int setupZipArchives();
int closeZipArchives();
int writeJSONFile(char* inFilePath, char* filename, bool picFile);

#endif // ARCHIVEWRITER_H_INCLUDED
