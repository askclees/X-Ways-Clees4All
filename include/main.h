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
#define ERROR_HASH_NOT_COMPUTED 3

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


/*Struct: FileRecord

Struct used for XML output in <createC4AllRecord>.

Fields:
    INT64 fileID                - Auto Incrementing ID
    wchar_t* fullPath           - Path of file (including filename)
    INT64 createdTime           - Recorded Created Time
    INT64 modifiedTime          - Recorded Modified Time
    INT64 accessedTime          - Recorded Accessed Time
    INT64 deletionTime          - Recorded Deleted Time
    wchar_t hashValue[64]       - MD5 hash value
    wchar_t description[128]    - Text based description, includes things like deleted etc
    INT64 fileSize              - Recorded size of file
    INT64 fileOffset            - Recorded offset of file
    INT64 physicalSector        - Recorded physical sector of file

*/

struct FileRecord{
    INT64 fileID;
    wchar_t* fullPath;
    INT64 createdTime;
    INT64 modifiedTime;
    INT64 accessedTime;
    INT64 deletionTime;
    wchar_t hashValue[64];
    wchar_t description[128];
    INT64 fileSize;
    INT64 fileOffset;
    INT64 physicalSector;
};

/*Struct: EvidenceObjects

Struct used to contain details of a single evidence object

Fields:
    wchar_t longName[2048];     - Long name of Evidence object
    DWORD objID                 - X-Ways ID of evidence object
    DWORD parentID              - X-Ways ID of parent evidence object

*/

struct EvidenceObjects{
    wchar_t longName[2048];
    DWORD objID;
    DWORD parentID;
};


/*Struct: outputDetails

Struct used to contain details of output files for XML output

Fields:
    wchar_t longName[2048]      - Long name of Evidence object
    DWORD objID                 - X-Ways ID of evidence object
    FILE* picOutput             - Handle to File for picture output
    INT64 picCounter            - Integer for number of records currently stored
    FILE* vidOutput             - Handle to File for Video output
    INT64 vidCounter            - Integer for number of records currently stored

*/

struct outputDetails{
    wchar_t evidenceObj[2048];
    DWORD objID;
    FILE* picOutput;
    INT64 picCounter;
    FILE* vidOutput;
    INT64 vidCounter;
};

/*Struct: ObjectNames

Struct used to contain detauils of the Evidence objects and the preferred output names.

The preferred names are used as the source ID in the JSON VICS export.

Fields:
    DWORD objectID              - DWORD of X-Ways reference to Object
    wchar_t actualName[1024]    - Actual name of evidence object (as shown in X-Ways forensics)
    wchar_t prefName[1024]      - Name that is to be displayed as source ID in JSON VICS output

*/

struct ObjectNames{
    DWORD objectID;
    wchar_t actualName[1024];
    wchar_t prefName[1024];
};

/*Struct: ExtractionDetails

Struct that contains details of extraction options

Fields:
    wchar_t* C4PPath                - Pointer to path of output for C4P XML file
    wchar_t* C4MPath                - Pointer to path of output for C4M XML file
    BOOL extractPictures            - Flag for exporting Pictures
    BOOL extractVideos              - Flag for exporting Videos
    outputDetails outputFiles[32]   - Array of object details
    int outputFileCounter           - Integer that contains number of Output Files in arrya
    ObjectNames* nameList           - Array of Object Names, should be of size noNames
    int noNames                     - Integer that stores number of name objects in Array
    BOOL VICExport                  - Flag for project VICS export
    BOOL C4ALLExport                - Flag for C4All (XML) setting
    BOOL debugSet                   - Debug Flag
    BOOL processStart               - Has process started
    BOOL createGriffeye             - Option to create Griffeye case
    BOOL checkParent                - Added in 1.40
    BOOL VICSCompressed             - Added in 1.50
    BOOL ocrScreenshot              - Added in 1.50
    BOOL extractText                - Added in 1.50
    wchar_t* GriffeyeCaseLocation   - Pointer to Griffeye Case Creation Path
    wchar_t* GriffeyeCaseName       - Pointer to Griffeye Case Name
    wchar_t* GriffeyeSettingsName   - Pointer to Griffeye Settings Name Added in 1.51
    HINSTANCE thisDLL
*/

struct ExtractionDetails{
    wchar_t* C4PPath;
    wchar_t* C4MPath;
    BOOL extractPictures;
    BOOL extractVideos;
    outputDetails outputFiles[32];
    int outputFileCounter;
    ObjectNames* nameList;
    int noNames;
    BOOL VICExport;
    BOOL C4ALLExport;
    BOOL debugSet;
    BOOL processStart;
    BOOL createGriffeye;
//1.40 add variable for ensuring frames are not extracted from videos
//1.50 set default value to true
    BOOL checkParent=true;
//1.50 added option for compressed VICS export
    BOOL VICSCompressed=false;
//1.50 add variables for OCR on screenshots and extracting any OCR text
    BOOL ocrScreenshot=false;
    BOOL extractText=false;
//1.50 added extract report tables and ignore thumbnails
    BOOL ignoreThumbs=false;
    BOOL exceptMismatch=false;
    BOOL exportReportTables=false;
    wchar_t* GriffeyeCaseLocation;
    wchar_t* GriffeyeCaseName;
    wchar_t* GriffeyeSettingsName = nullptr;
    HINSTANCE thisDLL;
};

struct EvidenceProps
{
    DWORD ID;
    wchar_t* Name;
    wchar_t* SourceID;
    DWORD parentID;
    int fileID;
    int selected;
};

struct hashValueStruct
{
    wchar_t MD5[33]={0};
    wchar_t SHA1[41]={0};
    int noPhotoDNAHash = 0;
    wchar_t photoDNA[256]={0};
};

struct ExtractOptions
{
    INT64 maxPictureSize;
    INT64 maxMovieSize;
    BOOL overwriteFiles;
    //1.50 allow for minimum file size as well
    INT64 minPictureSize;
    INT64 minMovieSize;
    //1.51 added file type fields
    int TypeStatusFlags;
    int FileTypeFlag;
    wchar_t errorReportPath[2048];
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

