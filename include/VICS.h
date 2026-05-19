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

Fields:
	MD5 - Wide character buffer for MD5 hash + NULL terminator (wchar_t[33])
	Property Name - String with the name of the property (wchar_t*)
	Property Value - String with the value of the property (wchar_t*)

*/

struct VICSMediaMetadata
{
    wchar_t MD5[33];
    wchar_t* PropertyName = NULL;
    wchar_t* PropertyValue = NULL;
};

/*Struct: VICSMediaFile

Struct implementing the VICS MEDIAFILE entity (v2.0 onwards)

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
    deleted - Flag to state that file was deleted and recovered (BOOL)
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

/*Struct: VICSExif - Currently unused */
struct VICSExif
{
	wchar_t* MD5;
	wchar_t* propertyName;
	wchar_t* propertyValue;
};

/*Struct: VICSAltHash - Currently unused */
struct VICSAltHash
{
	wchar_t MD5[33];
	wchar_t* hashName;
	wchar_t* hashValue;
};

/*Struct: VICSSegment - Currently unused */
struct VICSSegment
{
	wchar_t MD5[33];
	int segmentIndex;
	wchar_t* Start;
	wchar_t* End;
	int category;
};

/*Struct: VICSRepository - Currently unused */
struct VICSRepository
{
	wchar_t MD5[33];
	wchar_t* repositoryName;
};

/*Struct: VICSRecord

Struct linking all VICS entities

Fields:
	VICSMedia vMedia                    -   <VICSMedia> record
	VICSMediaFile* vMediaFiles          -   Pointer for <VICSMediaFile> array
	int noMediaFiles                    -   Number of valid VICSMediaFile records
	int currentMaxMediaFiles            -   Allocated capacity for vMediaFiles
	VICSAltHash* vAltHashes             -   Pointer for <VICSAltHash> array (unused)
	int noAltHash                       -   Number of valid VICSAltHash records
	VICSExif* vExif                     -   Pointer for <VICSExif> array (unused)
	int noExif                          -   Number of valid VICSExif records
	VICSSegment* vSegment               -   Pointer for <VICSSegment> array (unused)
	int noSegments                      -   Number of valid VICSSegment records
	VICSRepository* vRepository         -   Pointer for <VICSRepository> array (unused)
	int noRepository                    -   Number of valid VICSRepository records
    VICSMediaMetadata* vMediaMetaData   -   Pointer for <VICSMediaMetadata> array (unused)
	int noMediaMetadata                 -   Number of valid VICSMediaMetadata records
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
    VICSMediaMetadata* vMediaMetaData=nullptr;
	int noMediaMetadata=0;
	int currentMaxMediaMetadata=0;
};

//open & close file
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

//deallocation routines
void deallocateVICSRecord(VICSRecord record);
void deallocateMediaRecord(VICSMedia &record);
void deallocateMediaFileRecord(VICSMediaFile &record);
void deallocateMediaMetadataRecord(VICSMediaMetadata &record);
void freeVicsCaseData();

//writing
int writeMediaRecord(FILE* vicFile, VICSRecord* record);

//record size helpers (used by SQL insert functions)
INT64 getMediaFileRecordSize(VICSMediaFile &record);
INT64 getMediaRecordSize(VICSMedia &record);

//extraction from SQL
void extractVICSMediaSQL(VICSMedia &recMedia, sqlite3_stmt* statement);
void extractVICSMediaFileSQL(VICSMediaFile &recMediaFile, sqlite3_stmt* statement);
void extractVICSMediaMetadataSQL(VICSMediaMetadata* record, sqlite3_stmt* statement);

//validation
bool validFiletime(FILETIME timestamp);

//utility functions
char* convertWideToChar(const wchar_t* wString);

//extern variables
extern VICSCaseData vCaseData;


#endif // VICS_H_INCLUDED
