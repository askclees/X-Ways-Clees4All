#ifndef VICS_H_INCLUDED
#define VICS_H_INCLUDED

#include <windows.h>
#include "sqlite3.h"
#include "X-Tension.h"


/*Struct: VICSCaseData

Implementation of version 1.3 of case VICS structure
Seizure date and Application version ommitted currently

Fields:
    CaseNumber - Wide-character pointer for case number
    ContactOrg - Wide-character pointer for case number
    ContactName - Wide-character pointer for case number
    ContactPhone - Wide-character pointer for case number
    ContactEmail - Wide-character pointer for case number
    ContactTitle - Wide-character pointer for case number
    totalMedia - long long value to store count of media (currently unused)
    totalPrecat - long long value to store number of pre-cat images (currently unused)
    SourceAppName - const char* with text "Clees4All"
*/

struct VICSCaseData
{
    GUID caseGuid;
    wchar_t* CaseNumber=nullptr;
    wchar_t* ContactOrg=nullptr;
    wchar_t* ContactName=nullptr;
    wchar_t* ContactPhone=nullptr;
    wchar_t* ContactEmail=nullptr;
    wchar_t* ContactTitle=nullptr;
    long long totalMedia = 0;
    long long totalPrecat = 0;
    const char* SourceAppName = "Clees4All";
};

/*Struct: VICSMedia

Struct implementing version 2.0 of the VICS MEDIA entity

Fields:
	MediaID - Auto-incrementing integer, unique in file (INT64)
	Category - Numeric category for file - NULL is uncategorised (int)
	SHA1 - Wide character buffer for SHA1 hash + NULL terminator (wchar_t[41])
	MD5 - Wide character buffer for MD5 hash + NULL terminator (wchar_t[33])
	VictimID - Flag for Victim being identified (BOOL)
	OffenderID - Flag for Offender being identified (BOOL)
	IsDistributed - Flag for media being distributed (BOOL)
	Comments - Field for comments (wchar_t*) - unused
	Tags - List of Tags, delimited by commas (wchar_t*) -unused
	Series - List of series, delimited by commas (wchar_t*) - unused
	MediaSize - Size of media file in bytes (INT64)
	RelativeFilePath - Path to the media file exported, relative to root (wchar_t*)
	DateUpdated - Date media record was last updated (FILETIME)
	timeZone - Timezone value that does not appear in VICS standard. Unused. (int)
	IsPreCat - Flag to state image has been pre-categorised
	PrecatSource - String that provides data on precategorisation source (wchar_t*)
	IsSuspected - Flag for image that is not pre-cat but flagged for review
	MimeType - string with mime type of file e.g. image/jpeg. (wchar_t*)
	PhotoDNA -  Wide character array with space for 4 PhotoDNA hashes (wchar_t[256])
*/

struct VICSMedia
{
	INT64 MediaID;
	int Category;
	wchar_t SHA1[41];
	wchar_t MD5[33];
	BOOL VictimID;
	BOOL OffenderID;
	BOOL IsDistributed;
	wchar_t* Comments;
	wchar_t* Tags;
	wchar_t* Series;
	INT64 MediaSize;
	wchar_t* RelativeFilePath;
	FILETIME DateUpdated;
	int timeZone;
	BOOL IsPreCat;
	wchar_t* PrecatSource;
	BOOL IsSuspected;
	wchar_t* MimeType;
	wchar_t PhotoDNA[256]={0};
};

/*Struct: VICSMediaMetadata

Struct implementing the VICS MEDIAMETADATA entity (v2.0 onwards)

Can be used for any Name Key value and may be used multiple times

Currently used to add screenshot information from X-Ways

Fields:
	MD5 - Wide character buffer for MD5 hash + NULL terminator (wchar_t[33])
	Property Name - String with the name of the property (wchar_t*)
	Property Value - String with the value of the property (wchar_t*)

*/

//1.41 needed for OCR and Device type
struct VICSMediaMetadata
{
    wchar_t MD5[33];
    wchar_t* PropertyName = NULL;
    wchar_t* PropertyValue = NULL;
};

/*Struct: VICSMediaFile

Struct implementing the VICS MEDIAFILE entity (v2.0 onwards)
Can be used for any Name Key value and may be used multiple times
To be used to add screenshot information from X-Ways
Where the file is recovered from unallocated space, Clees4All sets both Unallocated and Deleted flags

Fields:
	MD5 - Wide character buffer for MD5 hash + NULL terminator of file (wchar_t[33])
	fileName - String with the name of the file entry (wchar_t*)
	filePath - String with the path of the file as it is on evidence (wchar_t*)
	created - timestamp for created datetime of file (FILETIME)
	written - timestamp for last written datetime of file (FILETIME)
	accessed - timestamp for last access datetime of file (FILETIME)
    unallocated - Flag to state file was recovered from unallocated space (BOOL)
    sourceID - string containing the source id that the file was recovered from (wchar_t*)
    physicalLocation - offset in bytes from beginning of disk where file was located (INT64)
    deleted - Flag to state that file was deleted and recovered. Usually means via deleted MFT record or similar  as opposed to carved(BOOL)
    Parent MD5 - Wide character buffer for MD5 hash + NULL terminator of parent file(wchar_t[33])
    parentName - String with parent files name (wchar_t*)
    parentFilePath - String with path of parent file (wchar_t*)
    parentPhysLoc - offset in bytes from beginning of disk where parent file exists
*/

struct VICSMediaFile
{
	wchar_t MD5[33];
	wchar_t* fileName=NULL;
	wchar_t* filePath=NULL;
	FILETIME created;
	FILETIME written;
	FILETIME accessed;
	BOOL unallocated;
	wchar_t* sourceID=NULL;
	INT64 physicalLocation;
	BOOL deleted;
	wchar_t parentMD5[33];
	wchar_t* parentName=NULL;
	wchar_t* parentFilePath=NULL;
	INT64 parentPhysLoc;
	long XWFitemID=0;
};

/*Struct: VICSExif

Struct implementing the VICS EXIF entity
Currently unused

*/

struct VICSExif
{
	wchar_t* MD5;
	wchar_t* propertyName;
	wchar_t* propertyValue;
};

/*Struct: VICSAltHash

Struct implementing the VICS ALTERNATIVEHASH entity
Currently unused

*/

struct VICSAltHash
{
	wchar_t MD5[33];
	wchar_t* hashName;
	wchar_t* hashValue;
};

/*Struct: VICSSegment

Struct implementing the VICS SEGMENT entity
Currently unused

*/

struct VICSSegment
{
	wchar_t MD5[33];
	int segmentIndex;
	wchar_t* Start;
	wchar_t* End;
	int category;
};

/*Struct: VICSRepository

Struct implementing the VICS REPOSITORY entity
Currently unused

*/

struct VICSRepository
{
	wchar_t MD5[33];
	wchar_t* repositoryName;
};

/*Struct: VICSRecord

Struct linking all VICS entities
Contains arrays of each type and corresponding counter


Fields:
	VICSMedia vMedia                    -   <VICSMedia> record
	VICSMediaFile* vMediaFiles          -   Pointer for <VICSMediaFile> to be set up as an array
	int noMediaFiles                    -   Current number of valid VICSMediaFile records referenced by pointer
	int currentMaxMediaFiles            -   Max number of media files in record based on current memory allocation. Linked to vMediaFiles
	VICSAltHash* vAltHashes             -   Pointer for <VICSAltHash> to be set up as an array
	int noAltHash                       -   Current number of valid VICSAltHash records referenced by pointer
	int currentMaxAltHash               -   Max number of media files in record based on current memory allocation. Linked to vAltHashes
	VICSExif* vExif                     -   Pointer for <VICSExif> to be set up as an array
	int noExif                          -   Current number of valid VICSExif records referenced by pointer
	int currentMaxExif                  -   Max number of media files in record based on current memory allocation. Linked to vExif
	VICSSegment* vSegment               -   Pointer for <VICSSegment> to be set up as an array
	int noSegments                      -   Current number of valid VICSSegment records referenced by pointer
	int currentMaxSegments              -   Max number of media files in record based on current memory allocation. Linked to vSegment
	VICSRepository* vRepository         -   Pointer for <VICSRepository> to be set up as an array
	int noRepository                    -   Current number of valid VICSRepository records referenced by pointer
	int currentMaxRepository            -   Max number of media files in record based on current memory allocation. Linked to vRepository
    VICSMediaMetadata* vMediaMetaData   -   Pointer for <VICSMediaMetadata> to be set up as an array
	int noMediaMetadata                 -   Current number of valid VICSMediaMetadata records referenced by pointer
	int currentMaxMediaMetadata         -   Max number of media files in record based on current memory allocation. Linked to vMediaMetaData

*/

struct VICSRecord
{
	VICSMedia vMedia;
	VICSMediaFile* vMediaFiles=nullptr;
	int noMediaFiles=0;
	int currentMaxMediaFiles=0;
	VICSAltHash* vAltHashes=nullptr;
	int noAltHash=0;
	int currentMaxAltHash=0;
	VICSExif* vExif=nullptr;
	int noExif=0;
	int currentMaxExif=0;
	VICSSegment* vSegment=nullptr;
	int noSegments=0;
	int currentMaxSegments=0;
	VICSRepository* vRepository=nullptr;
	int noRepository=0;
	int currentMaxRepository=0;
    //1.41 added
    VICSMediaMetadata* vMediaMetaData=nullptr;
	int noMediaMetadata=0;
	int currentMaxMediaMetadata=0;
};

//extern functions
//open & close functions
FILE* openVICSFile(char* filePath, const wchar_t* progVersion);
int closeVICSFile(FILE* vFile);

//initialize records
void InitializeMediaRecord(VICSMedia& record);
void InitializeAltHashRecord(VICSAltHash& record);
void InitializeMediaFileRecord(VICSMediaFile& record);
void InitializeVICSRecord(VICSRecord& record);
void InitializeRepositoryRecord(VICSRepository& record);
void InitializeExifRecord(VICSExif& record);
void InitializeSegmentRecord(VICSSegment& record);

//string generation
wchar_t* generateVicsMediaString(VICSMedia record);
wchar_t* generateVicsMediaFileString(VICSMediaFile* record);
wchar_t* generateVicsAltHashString(VICSAltHash record);
wchar_t* generateVicsRepositoryString(VICSRepository record);
wchar_t* generateVicsSegmentString(VICSSegment record);
wchar_t* generateVicsEXIFString(VICSExif record);
wchar_t* getAltHashRecordsString(VICSAltHash* records,int number);
wchar_t* getExifRecordsString(VICSExif* records,int number);
wchar_t* getMediaRecordString(VICSMediaFile* records, int number);
wchar_t* getRepositoryRecordString(VICSRepository* records, int number);
wchar_t* getSegmentRecordString(VICSSegment* records, int number);
//1.41 added media metadata
wchar_t* generateVicsMediaMetadataString(VICSMediaMetadata* record);

//deallocation routines
void deallocateVICSRecord(VICSRecord record);
void deallocateAltHashRecord(VICSAltHash record);
void deallocateExifRecord(VICSExif record);
void deallocateMediaRecord(VICSMedia &record);
void deallocateMediaFileRecord(VICSMediaFile &record);
void deallocateRepositoryRecord(VICSRepository record);
void deallocateSegmentRecord(VICSSegment record);
//1.41 added media metadata
void deallocateMediaMetadataRecord(VICSMediaMetadata &record);

//writing functions
int writeMediaRecord(FILE* vicFile, VICSRecord* record);

//record size functions
INT64 getMediaFileRecordSize(VICSMediaFile &record);
INT64 getMediaRecordSize(VICSMedia &record);

//return string functions
wchar_t* getAltHashRecordsString(VICSAltHash* records,int number);
wchar_t* getExifRecordsString(VICSExif* records,int number);
wchar_t* getMediaRecordString(VICSMediaFile* records, int number);
wchar_t* getSegmentRecordString(VICSSegment* records, int number);
wchar_t* getRepositoryRecordString(VICSRepository* records, int number);
wchar_t* createVICSstring(sqlite3* vicsDB,VICSMedia &record, int picture, int first);

//extraction from SQL
void extractVICSMediaSQL(VICSMedia &recMedia,sqlite3_stmt* statement);
//1.41 added media files
void extractVICSMediaFileSQL(VICSMediaFile &recMediaFile,sqlite3_stmt* statement);
//1.41 added media metadata records
void extractVICSMediaMetadataSQL(VICSMediaMetadata* record,sqlite3_stmt* statement);

//validation
wchar_t* checkJsonText(wchar_t* textIn);

//utility functions
char* convertWideToChar(wchar_t* wString);

//extern variables
extern VICSCaseData vCaseData;


#endif // VICS_H_INCLUDED
