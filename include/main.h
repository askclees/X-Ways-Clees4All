#ifndef __MAIN_H__
#define __MAIN_H__

#include <windows.h>
#include <stdio.h>
#include <objbase.h>
#include "X-Tension.h"


/*  To use this exported function of dll, include this header
 *  in your project.
 */

//#ifdef BUILD_DLL
    #define DLL_EXPORT __declspec(dllexport)
/*#else
    #define DLL_EXPORT __declspec(dllimport)
#endif*/

#define C4A_NAME      "Clees4All"
#define C4A_VERSION   "1.61"
#define C4A_TITLE     C4A_NAME " " C4A_VERSION

#define _C4A_WIDE(s)  L##s
#define C4A_WIDE(s)   _C4A_WIDE(s)
#define C4A_VERSION_W C4A_WIDE(C4A_VERSION)

#define ERROR_NO_MD5_HASH 0
#define ERROR_NO_SHA1_HASH 1
#define ERROR_CANNOT_READ 2
#define ERROR_FILESIZE_ZERO 3
#define ERROR_HASH_NOT_COMPUTED 4

//1.51 added unique values for type status
#define NOT_VERIFIED        1
#define IRRELEVANT          2
#define NOT_IN_LIST         4
#define CONFIRMED           8
#define NOT_CONFIRMED       16
#define NEWLY_IDENTIFIED    32
#define MISMATCH_DETECTED   64

//1.51 added unique values for file format status
#define UNKNOWN     1
#define OK          2
#define IRREGULAR   4
#define CORRUPT     8


extern const wchar_t* const errorValues[];
#define NoErrorTypes 5


/**
 * @brief Record used for XML output in createC4AllRecord.
 *
 * Holds per-file metadata written to the C4All XML report.
 */
struct FileRecord{
    /** @brief Auto-incrementing record ID. */
    INT64 fileID;
    /** @brief Full path of the file including filename. */
    wchar_t* fullPath;
    /** @brief Recorded creation timestamp. */
    INT64 createdTime;
    /** @brief Recorded last-modified timestamp. */
    INT64 modifiedTime;
    /** @brief Recorded last-accessed timestamp. */
    INT64 accessedTime;
    /** @brief Recorded deletion timestamp. */
    INT64 deletionTime;
    /** @brief MD5 hash value of the file. */
    wchar_t hashValue[64];
    /** @brief Text description including status flags such as "deleted". */
    wchar_t description[128];
    /** @brief Recorded size of the file in bytes. */
    INT64 fileSize;
    /** @brief Recorded byte offset of the file. */
    INT64 fileOffset;
    /** @brief Recorded physical sector of the file. */
    INT64 physicalSector;
};

/**
 * @brief Contains details of a single evidence object.
 */
struct EvidenceObjects{
    /** @brief Long display name of the evidence object. */
    wchar_t longName[2048];
    /** @brief X-Ways ID of the evidence object. */
    DWORD objID;
    /** @brief X-Ways ID of the parent evidence object. */
    DWORD parentID;
};


/**
 * @brief Contains details of XML output files associated with an evidence object.
 */
struct outputDetails{
    /** @brief Name of the evidence object. */
    wchar_t evidenceObj[2048];
    /** @brief X-Ways ID of the evidence object. */
    DWORD objID;
    /** @brief FILE handle for picture XML output. */
    FILE* picOutput;
    /** @brief Number of picture records written. */
    INT64 picCounter;
    /** @brief FILE handle for video XML output. */
    FILE* vidOutput;
    /** @brief Number of video records written. */
    INT64 vidCounter;
};

/**
 * @brief Contains the actual and preferred output names for an evidence object.
 *
 * The preferred name is used as the SourceID in the Project VICS JSON export.
 */
struct ObjectNames{
    /** @brief X-Ways reference ID for the evidence object. */
    DWORD objectID;
    /** @brief Actual name of the evidence object as shown in X-Ways Forensics. */
    wchar_t actualName[1024];
    /** @brief Name to display as SourceID in the VICS JSON output. */
    wchar_t prefName[1024];
};

/**
 * @brief Contains the full set of extraction options and runtime state for a processing run.
 */
struct ExtractionDetails{
    /** @brief Path for the C4P (picture) XML output file. */
    wchar_t* C4PPath;
    /** @brief Path for the C4M (movie/video) XML output file. */
    wchar_t* C4MPath;
    /** @brief Flag to extract pictures. */
    BOOL extractPictures;
    /** @brief Flag to extract videos. */
    BOOL extractVideos;
    /** @brief Array of per-evidence-object output file details (up to 32 objects). */
    outputDetails outputFiles[32];
    /** @brief Number of active entries in outputFiles. */
    int outputFileCounter;
    /** @brief Array of evidence object name mappings. */
    ObjectNames* nameList;
    /** @brief Number of entries in nameList. */
    int noNames;
    /** @brief Flag to enable Project VICS JSON export. */
    BOOL VICExport;
    /** @brief Flag to enable C4All (XML) export. */
    BOOL C4ALLExport;
    /** @brief Debug logging flag. */
    BOOL debugSet;
    /** @brief Indicates whether processing has started. */
    BOOL processStart;
    /** @brief Flag to create a Griffeye case. */
    BOOL createGriffeye;
    /** @brief Ignore files whose parent has already been processed (added in v1.40, default true). */
    BOOL checkParent=true;
    /** @brief Enable compressed (zip) VICS export (added in v1.50). */
    BOOL VICSCompressed=false;
    /** @brief Enable OCR on screenshots (added in v1.50, currently unused). */
    BOOL ocrScreenshot=false;
    /** @brief Enable extraction of OCR text (added in v1.50, currently unused). */
    BOOL extractText=false;
    /** @brief Ignore thumbnail files. */
    BOOL ignoreThumbs=false;
    /** @brief Except thumbnail-mismatch items from the thumbnail filter. */
    BOOL exceptMismatch=false;
    /** @brief Export report table associations. */
    BOOL exportReportTables=false;
    /** @brief Path to the Griffeye case creation directory. */
    wchar_t* GriffeyeCaseLocation;
    /** @brief Name for the Griffeye case. */
    wchar_t* GriffeyeCaseName;
    /** @brief Name for the Griffeye settings profile (added in v1.51). */
    wchar_t* GriffeyeSettingsName = nullptr;
    /** @brief Handle to this DLL module. */
    HINSTANCE thisDLL;
};

/**
 * @brief Properties of a single evidence object used for SQLite record insertion.
 */
struct EvidenceProps
{
    /** @brief X-Ways evidence object ID. */
    DWORD ID;
    /** @brief Display name of the evidence object. */
    wchar_t* Name;
    /** @brief Source identifier string. */
    wchar_t* SourceID;
    /** @brief X-Ways ID of the parent evidence object. */
    DWORD parentID;
    /** @brief Index of the associated XML output file. */
    int fileID;
    /** @brief 1 if the evidence object is selected for processing, 0 otherwise. */
    int selected;
};

/**
 * @brief Stores MD5, SHA1, and PhotoDNA hash values for a file.
 */
struct hashValueStruct
{
    /** @brief MD5 hash as a 32-character wide string. */
    wchar_t MD5[33]={0};
    /** @brief SHA1 hash as a 40-character wide string. */
    wchar_t SHA1[41]={0};
    /** @brief Number of PhotoDNA hash values present. */
    int noPhotoDNAHash = 0;
    /** @brief PhotoDNA hash value. */
    wchar_t photoDNA[256]={0};
};

/**
 * @brief Persistent extraction options loaded from and saved to the options SQLite database.
 */
struct ExtractOptions
{
    /** @brief Maximum picture file size in bytes to process (0 = no limit). */
    INT64 maxPictureSize;
    /** @brief Maximum video file size in bytes to process (0 = no limit). */
    INT64 maxMovieSize;
    /** @brief Overwrite existing output files if true. */
    BOOL overwriteFiles;
    /** @brief Minimum picture file size in bytes to process (added in v1.50). */
    INT64 minPictureSize;
    /** @brief Minimum video file size in bytes to process (added in v1.50). */
    INT64 minMovieSize;
    /** @brief Bitmask of X-Ways type status flags to include (added in v1.51). */
    int TypeStatusFlags;
    /** @brief Bitmask of X-Ways file format status flags to include (added in v1.51). */
    int FileTypeFlag;
    /** @brief Path for the error report output directory. */
    wchar_t errorReportPath[2048];
    /** @brief Path to the Griffeye Analyze installation directory. */
    wchar_t GriffeyePath[2048];

};


extern ExtractionDetails extractInfo;
extern ExtractOptions extractOpt;
extern void outputErrorMessage(const wchar_t* errMsg, LONG nItemID);
extern wchar_t* getFullPath(LPWSTR evObject,LONG nItemID, BOOL isVIC);


#ifdef __cplusplus
extern "C"
{
#endif

LONG DLL_EXPORT XT_Init(CallerInfo info, DWORD nFlags, HANDLE hMainWnd,
   void* lpReserved);

#ifdef __cplusplus
}
#endif

#endif // __MAIN_H__
