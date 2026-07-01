#ifndef VICS_H_INCLUDED
#define VICS_H_INCLUDED

#include <windows.h>
#include "sqlite3.h"
#include "X-Tension.h"
#include "utility.h"


/**
 * @brief Implementation of the Project VICS case-level data structure (v1.3).
 *
 * Seizure date and application version fields are currently omitted.
 */
struct VICSCaseData
{
    /** @brief Unique GUID identifying this case. */
    GUID caseGuid;
    /** @brief Case reference number. */
    wchar_t* CaseNumber=nullptr;
    /** @brief Name of the submitting organisation. */
    wchar_t* ContactOrg=nullptr;
    /** @brief Name of the submitting contact. */
    wchar_t* ContactName=nullptr;
    /** @brief Phone number of the submitting contact. */
    wchar_t* ContactPhone=nullptr;
    /** @brief Email address of the submitting contact. */
    wchar_t* ContactEmail=nullptr;
    /** @brief Job title of the submitting contact. */
    wchar_t* ContactTitle=nullptr;
    /** @brief Total count of media items (currently unused). */
    long long totalMedia = 0;
    /** @brief Count of pre-categorised images (currently unused). */
    long long totalPrecat = 0;
    /** @brief Source application name, always "Clees4All". */
    const char* SourceAppName = "Clees4All";
};

/**
 * @brief Implements version 2.0 of the Project VICS MEDIA entity.
 */
struct VICSMedia
{
    /** @brief Auto-incrementing integer, unique within the output file. */
    INT64 MediaID;
    /** @brief Numeric category for the file; 0 is uncategorised. */
    int Category;
    /** @brief SHA1 hash value as a 40-character wide string. */
    wchar_t SHA1[41];
    /** @brief MD5 hash value as a 32-character wide string. */
    wchar_t MD5[33];
    /** @brief True if a victim has been identified in this media. */
    BOOL VictimID;
    /** @brief True if an offender has been identified in this media. */
    BOOL OffenderID;
    /** @brief True if this media has been distributed. */
    BOOL IsDistributed;
    /** @brief Free-text comments field (currently unused). */
    wchar_t* Comments;
    /** @brief Comma-delimited list of tags (currently unused). */
    wchar_t* Tags;
    /** @brief Comma-delimited list of series names (currently unused). */
    wchar_t* Series;
    /** @brief Size of the media file in bytes. */
    INT64 MediaSize;
    /** @brief Path to the exported file relative to the archive root. */
    wchar_t* RelativeFilePath;
    /** @brief Date the media record was last updated. */
    FILETIME DateUpdated;
    /** @brief Timezone offset in hours (not in the VICS standard; currently unused). */
    int timeZone;
    /** @brief True if the media has been pre-categorised. */
    BOOL IsPreCat;
    /** @brief Source description for pre-categorisation. */
    wchar_t* PrecatSource;
    /** @brief True if the media is suspected but not pre-categorised. */
    BOOL IsSuspected;
    /** @brief MIME type of the file, e.g. "image/jpeg". */
    wchar_t* MimeType;
    /** @brief PhotoDNA hash value(s); space for up to 4 hashes. */
    wchar_t PhotoDNA[256]={0};
};

/**
 * @brief Implements the Project VICS MEDIAMETADATA entity (v2.0 onwards).
 */
struct VICSMediaMetadata
{
    /** @brief MD5 hash linking this metadata to its parent media record. */
    wchar_t MD5[33];
    /** @brief Name of the metadata property. */
    wchar_t* PropertyName = NULL;
    /** @brief Value of the metadata property. */
    wchar_t* PropertyValue = NULL;
};

/**
 * @brief Implements the Project VICS MEDIAFILE entity (v2.0 onwards).
 *
 * Represents a single on-disk instance of a media file.
 */
struct VICSMediaFile
{
    /** @brief MD5 hash of the file, linking it to its VICSMedia record. */
    wchar_t MD5[33];
    /** @brief File name. */
    wchar_t* fileName=NULL;
    /** @brief Full file path as it appeared on the evidence. */
    wchar_t* filePath=NULL;
    /** @brief File creation timestamp. */
    FILETIME created;
    /** @brief File last-written timestamp. */
    FILETIME written;
    /** @brief File last-accessed timestamp. */
    FILETIME accessed;
    /** @brief True if the file was recovered from unallocated space. */
    BOOL unallocated;
    /** @brief Source ID of the evidence object containing this file. */
    wchar_t* sourceID=NULL;
    /** @brief Byte offset from the start of the disk where the file is located. */
    INT64 physicalLocation;
    /** @brief True if the file was deleted and recovered. */
    BOOL deleted;
    /** @brief MD5 hash of the parent file (e.g. an archive or disk image). */
    wchar_t parentMD5[33];
    /** @brief Name of the parent file. */
    wchar_t* parentName=NULL;
    /** @brief Path of the parent file. */
    wchar_t* parentFilePath=NULL;
    /** @brief Byte offset of the parent file from the start of the disk. */
    INT64 parentPhysLoc;
    /** @brief X-Ways item ID used for duplicate detection. */
    long XWFitemID=0;
};

/**
 * @brief Implements the Project VICS EXIF entity. Currently unused.
 */
struct VICSExif
{
    /** @brief MD5 hash linking this EXIF record to its parent media record. */
    wchar_t* MD5;
    /** @brief EXIF property name. */
    wchar_t* propertyName;
    /** @brief EXIF property value. */
    wchar_t* propertyValue;
};

/**
 * @brief Stores an alternative hash value for a media file. Currently unused.
 *
 * Under VICS 1.3 this stored PhotoDNA; from v2.0 PhotoDNA moved to the VICSMedia record.
 */
struct VICSAltHash
{
    /** @brief MD5 hash linking this record to its parent media record. */
    wchar_t MD5[33];
    /** @brief Type of hash stored, e.g. "SHA256" or "EDK". */
    wchar_t* hashName;
    /** @brief The hash value. */
    wchar_t* hashValue;
};

/**
 * @brief Implements the Project VICS SEGMENT entity. Currently unused.
 */
struct VICSSegment
{
    /** @brief MD5 hash linking this segment to its parent media record. */
    wchar_t MD5[33];
    /** @brief Index of this segment within the media item. */
    int segmentIndex;
    /** @brief Start timecode or position of the segment. */
    wchar_t* Start;
    /** @brief End timecode or position of the segment. */
    wchar_t* End;
    /** @brief Category of the segment. */
    int category;
};

/**
 * @brief Implements the Project VICS REPOSITORY entity. Currently unused.
 */
struct VICSRepository
{
    /** @brief MD5 hash linking this record to its parent media record. */
    wchar_t MD5[33];
    /** @brief Name of the repository. */
    wchar_t* repositoryName;
};

/**
 * @brief Top-level container linking all Project VICS entities for a single media item.
 */
struct VICSRecord
{
    /** @brief The core VICS Media record. */
    VICSMedia vMedia;
    /** @brief Array of associated MediaFile records. */
    VICSMediaFile* vMediaFiles=nullptr;
    /** @brief Number of valid entries in vMediaFiles. */
    int noMediaFiles=0;
    /** @brief Allocated capacity of vMediaFiles. */
    int currentMaxMediaFiles=0;
    /** @brief Array of alternative hash records (currently unused). */
    VICSAltHash* vAltHashes=nullptr;
    /** @brief Number of valid entries in vAltHashes. */
    int noAltHash=0;
    /** @brief Allocated capacity of vAltHashes. */
    int currentMaxAltHash=0;
    /** @brief Array of EXIF records (currently unused). */
    VICSExif* vExif=nullptr;
    /** @brief Number of valid entries in vExif. */
    int noExif=0;
    /** @brief Allocated capacity of vExif. */
    int currentMaxExif=0;
    /** @brief Array of segment records (currently unused). */
    VICSSegment* vSegment=nullptr;
    /** @brief Number of valid entries in vSegment. */
    int noSegments=0;
    /** @brief Allocated capacity of vSegment. */
    int currentMaxSegments=0;
    /** @brief Array of repository records (currently unused). */
    VICSRepository* vRepository=nullptr;
    /** @brief Number of valid entries in vRepository. */
    int noRepository=0;
    /** @brief Allocated capacity of vRepository. */
    int currentMaxRepository=0;
    /** @brief Array of media metadata records. */
    VICSMediaMetadata* vMediaMetaData=nullptr;
    /** @brief Number of valid entries in vMediaMetaData. */
    int noMediaMetadata=0;
    /** @brief Allocated capacity of vMediaMetaData. */
    int currentMaxMediaMetadata=0;
};

/** @brief Opens a VICS JSON output file and writes the case header. @see closeVICSFile */
FILE* openVICSFile(char* filePath, const wchar_t* progVersion);
/** @brief Writes the closing JSON brackets and closes the VICS output file. @see openVICSFile */
int closeVICSFile(FILE* vFile);

/** @brief Initialises all fields of a VICSMedia record to zero/null defaults. */
void initializeMediaRecord(VICSMedia& record);
/** @brief Initialises all fields of a VICSAltHash record to zero/null defaults. */
void initializeAltHashRecord(VICSAltHash& record);
/** @brief Initialises all fields of a VICSMediaFile record to zero/null defaults. */
void initializeMediaFileRecord(VICSMediaFile& record);
/** @brief Initialises a VICSRecord and its embedded VICSMedia sub-record to zero/null defaults. */
void initializeVICSRecord(VICSRecord& record);
/** @brief Initialises all fields of a VICSRepository record to zero/null defaults. */
void initializeRepositoryRecord(VICSRepository& record);
/** @brief Initialises all fields of a VICSExif record to zero/null defaults. */
void initializeExifRecord(VICSExif& record);
/** @brief Initialises all fields of a VICSSegment record to zero/null defaults. */
void initializeSegmentRecord(VICSSegment& record);

/** @brief Frees all dynamically allocated memory within a VICSRecord. */
void deallocateVICSRecord(VICSRecord record);
/** @brief Frees all dynamically allocated pointer fields within a VICSMedia record. */
void deallocateMediaRecord(VICSMedia &record);
/** @brief Frees all dynamically allocated pointer fields within a VICSMediaFile record. */
void deallocateMediaFileRecord(VICSMediaFile &record);
/** @brief Frees the PropertyName and PropertyValue fields within a VICSMediaMetadata record. */
void deallocateMediaMetadataRecord(VICSMediaMetadata &record);
/** @brief Frees all dynamically allocated string fields in the global VICSCaseData struct. */
void freeVicsCaseData();

/** @brief Writes a single VICS Media record with its MediaFiles array to an open VICS file. */
int writeMediaRecord(FILE* vicFile, VICSRecord* record);

/** @brief Returns the total character count of variable-length fields in a VICSMediaFile record. */
INT64 getMediaFileRecordSize(VICSMediaFile &record);
/** @brief Returns the total character count of variable-length fields in a VICSMedia record. */
INT64 getMediaRecordSize(VICSMedia &record);

/** @brief Populates a VICSMedia record from the current row of an SQLite statement. */
void extractVICSMediaSQL(VICSMedia &recMedia, sqlite3_stmt* statement);
/** @brief Populates a VICSMediaFile record from the current row of an SQLite statement. */
void extractVICSMediaFileSQL(VICSMediaFile &recMediaFile, sqlite3_stmt* statement);
/** @brief Populates a VICSMediaMetadata record from the current row of an SQLite statement. */
void extractVICSMediaMetadataSQL(VICSMediaMetadata* record, sqlite3_stmt* statement);

/** @brief Validates a FILETIME, rejecting zero, implausibly old, and future timestamps. */
bool validFiletime(FILETIME timestamp);

extern VICSCaseData vCaseData;


#endif // VICS_H_INCLUDED
