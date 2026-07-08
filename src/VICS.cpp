//std headers
#include <cwchar>
#include <cstdio>
#include <windows.h>
#include <stdint.h>

//project headers
#include "VICS.h"
#include "utility.h"
#include "cJSON.h"

//globals
VICSCaseData vCaseData;
static unsigned char BOM[]={0xFF,0xFE};
//1.50 minimum value for FILETIME timestamp
const DWORD minTime = 0x015fffff;



/**
 * @brief Creates a new VICS JSON output file and writes the case header information.
 *
 * The output is not a complete VICS JSON entry until closeVICSFile is called to write
 * the closing brackets.
 *
 * @param filePath    NULL-terminated path for the file to be created.
 * @param progVersion Wide string containing the program version to embed in the header.
 * @return FILE pointer to the opened VICS file, or NULL on failure.
 *
 * @see closeVICSFile
 * @see setupVicsExport
 */
FILE* openVICSFile(char* filePath, const wchar_t* progVersion)
{
	FILE* newFile = fopen(filePath,"wb");
	if (newFile == NULL) 	{ return newFile;}
	//1.38 - changed text file to UTF8, no BOM added
	fprintf(newFile,"{\r\n\t\"@odata.context\":\"http://github.com/VICSDATAMODEL/ProjectVic/DataModels/2.0.xml/UK/$metadata#Cases\",\r\n\t");
    //new items for case data
	fprintf(newFile,"\"value\":[{\"CaseID\":\"{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\",\r\n\t",
          vCaseData.caseGuid.Data1, vCaseData.caseGuid.Data2, vCaseData.caseGuid.Data3, vCaseData.caseGuid.Data4[0], vCaseData.caseGuid.Data4[1], vCaseData.caseGuid.Data4[2],
          vCaseData.caseGuid.Data4[3],  vCaseData.caseGuid.Data4[4], vCaseData.caseGuid.Data4[5], vCaseData.caseGuid.Data4[6], vCaseData.caseGuid.Data4[7]);
	//1.51 fixed so case number also translates wide characters
    if (vCaseData.CaseNumber != nullptr) {
        char* caseNumStr = convertWideToChar(vCaseData.CaseNumber);
        fprintf(newFile,"\"CaseNumber\":\"%s\",\r\n\t",caseNumStr);
        delete[] caseNumStr;
    }
	if (vCaseData.ContactPhone != nullptr) {fprintf(newFile,"\"ContactPhone\":\"%ls\",\r\n\t",vCaseData.ContactPhone);}
	//1.41 - convert details to UTF8
	if (vCaseData.ContactEmail != nullptr) {
        char* emailStr = convertWideToChar(vCaseData.ContactEmail);
        fprintf(newFile,"\"ContactEmail\":\"%s\",\r\n\t",emailStr);
        delete[] emailStr;
    }
	if (vCaseData.ContactTitle != nullptr) {
        char* titleStr = convertWideToChar(vCaseData.ContactTitle);
        fprintf(newFile,"\"ContactTitle\":\"%s\",\r\n\t",titleStr);
        delete[] titleStr;
    }
	if (vCaseData.ContactOrg != nullptr) {
        char* contactStr = convertWideToChar(vCaseData.ContactOrg);
        fprintf(newFile,"\"ContactOrganization\":\"%s\",\r\n\t",contactStr);
        delete[] contactStr;
    }
    //1.41 changed to use data from struct
	fprintf(newFile,"\"SourceApplicationName\":\"Clees4All\",\r\n\t");
	fprintf(newFile,"\"SourceApplicationVersion\":\"%ls\",\r\n\t",progVersion);
	fprintf(newFile,"\"Media\":[");
	fflush(newFile);
	return newFile;
}

/**
 * @brief Writes the closing JSON brackets to a VICS file and then closes it.
 *
 * @param vFile Valid FILE pointer previously opened by openVICSFile.
 * @return 0 on success, 1 if vFile is NULL, 2 if writing the closing brackets fails,
 *         or the result of fclose on error.
 *
 * @see openVICSFile
 * @see writeRecords
 */
int closeVICSFile(FILE* vFile)
{
	if (vFile == NULL) { return 1;}
	int check = fprintf(vFile,"\t]\r\n\t}]\r\n\t}");
	if (check<0)
	{
		return 2;
	}
	return fclose(vFile);
}

/**
 * @brief Initialises all fields of a VICSMedia record to their zero/null defaults.
 *
 * @param record Reference to the VICSMedia struct to initialise.
 */
void initializeMediaRecord(VICSMedia& record)
{
    record.Category = 0;
    record.MediaID = 0;
    record.MediaSize = 0;
    record.timeZone = 0;

    record.IsDistributed = FALSE;
    record.IsPreCat = FALSE;
    record.IsSuspected = FALSE;
    record.OffenderID = FALSE;
    record.VictimID = FALSE;

    record.Comments = NULL;
    record.MimeType = NULL;
    record.PrecatSource = NULL;
    record.RelativeFilePath = NULL;
    record.Series = NULL;
    record.Tags = NULL;

    record.MD5[0] = L'\0';
    record.SHA1[0] = L'\0';

    record.DateUpdated.dwHighDateTime = 0;
    record.DateUpdated.dwLowDateTime = 0;
}

/**
 * @brief Initialises all fields of a VICSAltHash record to their zero/null defaults.
 *
 * @param record Reference to the VICSAltHash struct to initialise.
 */
void initializeAltHashRecord(VICSAltHash& record)
{
    record.hashName = NULL;
    record.hashValue = NULL;
    record.MD5[0] = L'\0';
}

/**
 * @brief Initialises all fields of a VICSMediaFile record to their zero/null defaults.
 *
 * @param record Reference to the VICSMediaFile struct to initialise.
 */
void initializeMediaFileRecord(VICSMediaFile& record)
{
    record.deleted = FALSE;
    record.unallocated = FALSE;

    record.fileName = NULL;
    record.filePath = NULL;
    record.parentFilePath = NULL;
    record.MD5[0] = L'\0';
    record.parentMD5[0]=L'\0';
    record.parentName = NULL;
    record.sourceID = NULL;

    record.accessed.dwHighDateTime=0;
    record.accessed.dwLowDateTime=0;
    record.created.dwHighDateTime=0;
    record.created.dwLowDateTime=0;
    record.written.dwHighDateTime=0;
    record.written.dwLowDateTime=0;

    record.parentPhysLoc = 0;
    record.physicalLocation = 0;
}

/**
 * @brief Initialises a VICSRecord and its embedded VICSMedia sub-record to their zero/null defaults.
 *
 * @param record Reference to the VICSRecord struct to initialise.
 *
 * @see initializeMediaRecord
 */
void initializeVICSRecord(VICSRecord& record)
{
    record.noMediaFiles = 0;
    record.noAltHash = 0;
    record.noExif = 0;
    record.noSegments = 0;
    record.noRepository = 0;

    initializeMediaRecord(record.vMedia);
}

/**
 * @brief Initialises all fields of a VICSRepository record to their zero/null defaults.
 *
 * @param record Reference to the VICSRepository struct to initialise.
 */
void initializeRepositoryRecord(VICSRepository& record)
{
    record.repositoryName = NULL;
    record.MD5[0] = L'\0';
}

/**
 * @brief Initialises all fields of a VICSExif record to their zero/null defaults.
 *
 * @param record Reference to the VICSExif struct to initialise.
 */
void initializeExifRecord(VICSExif& record)
{
    record.propertyName = NULL;
    record.propertyValue = NULL;
    record.MD5 =  NULL;
}

/**
 * @brief Initialises all fields of a VICSSegment record to their zero/null defaults.
 *
 * @param record Reference to the VICSSegment struct to initialise.
 */
void initializeSegmentRecord(VICSSegment& record)
{
    record.Start = NULL;
    record.End = NULL;
    record.MD5[0] = L'\0';

    record.segmentIndex = 0;
    record.category = 0;
}

/**
 * @brief Frees all dynamically allocated memory within a VICSRecord, including its MediaFiles and MediaMetadata.
 *
 * @param record The VICSRecord to deallocate (passed by value; sub-records are freed in place).
 *
 * @see deallocateMediaRecord
 * @see deallocateMediaFileRecord
 * @see deallocateMediaMetadataRecord
 */
void deallocateVICSRecord(VICSRecord record)
{
    deallocateMediaRecord(record.vMedia);
    if (record.noMediaFiles !=0)
    {
        for (int i=0;i<record.noMediaFiles;i++)
        {
            deallocateMediaFileRecord(record.vMediaFiles[i]);
        }
        delete[] record.vMediaFiles;
        record.noMediaFiles= 0;
    }
    //1.41 add cleaning of media metadata records
    if (record.noMediaMetadata !=0)
    {
        for (int i=0;i<record.noMediaMetadata;i++)
        {
            deallocateMediaMetadataRecord(record.vMediaMetaData[i]);
        }
        delete[] record.vMediaMetaData;
        record.noMediaMetadata= 0;
    }
}

/**
 * @brief Frees all dynamically allocated pointer fields within a VICSMedia record.
 *
 * @param record Reference to the VICSMedia struct whose fields are to be freed.
 */
void deallocateMediaRecord(VICSMedia &record)
{
    if (record.Comments != NULL) {delete[] record.Comments;}
    if (record.MimeType != NULL) {delete[] record.MimeType;}
    if (record.PrecatSource != NULL) {delete[] record.PrecatSource;}
    if (record.RelativeFilePath != NULL) {delete[] record.RelativeFilePath;}
    if (record.Series != NULL) {delete[] record.Series;}
    if (record.Tags != NULL) {delete[] record.Tags;}
}

/**
 * @brief Frees the PropertyName and PropertyValue fields within a VICSMediaMetadata record.
 *
 * @param record Reference to the VICSMediaMetadata struct whose fields are to be freed.
 */
void deallocateMediaMetadataRecord(VICSMediaMetadata &record)
{
    if (record.PropertyName != NULL) {delete[] record.PropertyName;}
    if (record.PropertyValue != NULL) {delete[] record.PropertyValue;}
}

/**
 * @brief Frees all dynamically allocated pointer fields within a VICSMediaFile record.
 *
 * @param record Reference to the VICSMediaFile struct whose fields are to be freed.
 */
void deallocateMediaFileRecord(VICSMediaFile &record)
{
    if (record.fileName != NULL) {delete[] record.fileName;}
    if (record.filePath != NULL) {delete[] record.filePath;}
    if (record.parentFilePath != NULL) {delete[] record.parentFilePath;}
    if (record.parentName != NULL) {delete[] record.parentName;}
    if (record.sourceID != NULL) {delete[] record.sourceID;}
}

/**
 * @brief Frees all dynamically allocated string fields in the global VICSCaseData struct.
 */
void freeVicsCaseData()
{
    if (vCaseData.CaseNumber != nullptr)    { delete[] vCaseData.CaseNumber; }
    if (vCaseData.ContactEmail != nullptr)  { delete[] vCaseData.ContactEmail; }
    if (vCaseData.ContactName != nullptr)   { delete[] vCaseData.ContactName; }
    if (vCaseData.ContactOrg != nullptr)    { delete[] vCaseData.ContactOrg; }
    if (vCaseData.ContactPhone != nullptr)  { delete[] vCaseData.ContactPhone; }
    if (vCaseData.ContactTitle != nullptr)  { delete[] vCaseData.ContactTitle; }
}

/**
 * @brief Returns the total character count of all variable-length wide string fields in a VICSMediaFile record.
 *
 * Used by insertMediaFileRecord to determine the required SQL query buffer size.
 *
 * @param record Reference to the VICSMediaFile struct to measure.
 * @return Total character count of all non-null wchar_t* fields.
 *
 * @see insertMediaFileRecord
 */
INT64 getMediaFileRecordSize(VICSMediaFile &record)
{
    INT64 retSize=0;
    if (record.fileName!= NULL) {retSize = retSize + wcslen(record.fileName);}
    if (record.filePath!= NULL) {retSize = retSize + wcslen(record.filePath);}
    if (record.sourceID!= NULL) {retSize = retSize + wcslen(record.sourceID);}
    if (record.parentName!= NULL) {retSize = retSize + wcslen(record.parentName);}
    if (record.parentFilePath!= NULL) {retSize = retSize + wcslen(record.parentFilePath);}
    return retSize;
}

/**
 * @brief Returns the total character count of all variable-length wide string fields in a VICSMedia record.
 *
 * Used by insertMediaRecord to determine the required SQL query buffer size.
 *
 * @param record Reference to the VICSMedia struct to measure.
 * @return Total character count of all non-null wchar_t* fields.
 *
 * @see insertMediaRecord
 */
INT64 getMediaRecordSize(VICSMedia &record)
{
    INT64 retSize=0;
    if (record.Comments!= NULL) {retSize = retSize + wcslen(record.Comments);}
    if (record.Tags!= NULL) {retSize = retSize + wcslen(record.Tags);}
    if (record.Series!= NULL) {retSize = retSize + wcslen(record.Series);}
    if (record.RelativeFilePath!= NULL) {retSize = retSize + wcslen(record.RelativeFilePath);}
    if (record.PrecatSource!= NULL) {retSize = retSize + wcslen(record.PrecatSource);}
    if (record.MimeType!= NULL) {retSize = retSize + wcslen(record.MimeType);}
    return retSize;
}

/* Add an INT64 field to a cJSON object as a JSON integer (no double conversion). */
static void cjsonAddInt64(cJSON* obj, const char* key, INT64 value)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%lld", (long long)value);
    cJSON_AddItemToObject(obj, key, cJSON_CreateRaw(buf));
}

/* Add a wide string field to a cJSON object.
   Converts wchar_t* to UTF-8 via convertWideToChar. Skips NULL or empty strings. */
static void cjsonAddWide(cJSON* obj, const char* key, const wchar_t* wstr)
{
    if (wstr == NULL || wstr[0] == L'\0') return;
    char* utf8 = convertWideToChar(wstr);
    cJSON_AddStringToObject(obj, key, utf8);
    delete[] utf8;
}

/* Add a FILETIME as an ISO 8601 string to a cJSON object.
   tz is a signed hours offset applied to the DateUpdated field (MediaRecord only).
   Skips invalid timestamps. */
static void cjsonAddFiletime(cJSON* obj, const char* key, FILETIME ft, int tz)
{
    if (!validFiletime(ft)) return;
    SYSTEMTIME st;
    if (!FileTimeToSystemTime(&ft, &st)) return;
    char buf[64];
    if (tz != 0)
    {
        snprintf(buf, sizeof(buf), "%d-%02d-%02dT%02d:%02d:%02d.%03d%+03d:00Z",
                 st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, tz);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                 st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    }
    cJSON_AddStringToObject(obj, key, buf);
}

/* Build a cJSON object for a VICSMediaFile record.
   Returns NULL if mandatory fields (MD5, FileName, FilePath) are absent. */
static cJSON* buildMediaFileCJSON(VICSMediaFile* f)
{
    if (f->MD5[0] == L'\0' || f->fileName == NULL || f->filePath == NULL)
        return NULL;

    cJSON* obj = cJSON_CreateObject();

    char md5[33];
    wcstombs(md5, f->MD5, sizeof(md5));
    md5[32] = '\0';
    cJSON_AddStringToObject(obj, "MD5", md5);

    cjsonAddWide(obj, "FileName", f->fileName);
    cjsonAddWide(obj, "FilePath", f->filePath);
    cjsonAddFiletime(obj, "Created",  f->created,  0);
    cjsonAddFiletime(obj, "Written",  f->written,  0);
    cjsonAddFiletime(obj, "Accessed", f->accessed, 0);

    if (f->unallocated) cJSON_AddStringToObject(obj, "Unallocated", "true");
    if (f->deleted)     cJSON_AddStringToObject(obj, "Deleted",     "true");

    if (f->parentMD5[0] != L'\0')
    {
        char pmd5[33];
        wcstombs(pmd5, f->parentMD5, sizeof(pmd5));
        pmd5[32] = '\0';
        cJSON_AddStringToObject(obj, "ParentMD5", pmd5);
    }
    cjsonAddWide(obj, "ParentFileName", f->parentName);
    cjsonAddWide(obj, "ParentFilePath", f->parentFilePath);

    if (f->parentPhysLoc != 0)
        cjsonAddInt64(obj, "ParentPhysicalLocation", f->parentPhysLoc);
    if (f->physicalLocation != 0)
        cjsonAddInt64(obj, "PhysicalLocation", f->physicalLocation);

    cjsonAddWide(obj, "SourceID", f->sourceID);

    return obj;
}

/* Build a cJSON object for a VICSMedia record.
   Returns NULL if the mandatory MD5 field is absent. */
static cJSON* buildMediaCJSON(VICSMedia& m)
{
    if (m.MD5[0] == L'\0') return NULL;

    cJSON* obj = cJSON_CreateObject();

    cjsonAddInt64(obj, "MediaID", m.MediaID);

    if (m.Category != 0)
        cJSON_AddNumberToObject(obj, "Category", m.Category);

    char md5[33];
    wcstombs(md5, m.MD5, sizeof(md5));
    md5[32] = '\0';
    cJSON_AddStringToObject(obj, "MD5", md5);

    if (m.SHA1[0] != L'\0')
    {
        char sha1[41];
        wcstombs(sha1, m.SHA1, sizeof(sha1));
        sha1[40] = '\0';
        cJSON_AddStringToObject(obj, "SHA1", sha1);
    }

    if (m.VictimID)     cJSON_AddStringToObject(obj, "VictimIdentified",   "true");
    if (m.OffenderID)   cJSON_AddStringToObject(obj, "OffenderIdentified",  "true");
    if (m.IsDistributed)cJSON_AddStringToObject(obj, "IsDistributed",       "true");

    cjsonAddWide(obj, "Comments", m.Comments);
    cjsonAddWide(obj, "Series",   m.Series);
    cjsonAddWide(obj, "Tags",     m.Tags);

    cjsonAddFiletime(obj, "DateUpdated", m.DateUpdated, m.timeZone);

    if (m.MediaSize != 0)
        cjsonAddInt64(obj, "MediaSize", m.MediaSize);

    cjsonAddWide(obj, "RelativeFilePath", m.RelativeFilePath);

    if (m.IsPreCat)
    {
        cJSON_AddStringToObject(obj, "IsPrecategorized", "true");
        cjsonAddWide(obj, "PrecategorizationSource", m.PrecatSource);
    }
    if (m.IsSuspected)
        cJSON_AddStringToObject(obj, "IsSuspected", "true");

    cjsonAddWide(obj, "MimeType", m.MimeType);

    if (m.PhotoDNA[0] != L'\0')
    {
        char photodna[256];
        wcstombs(photodna, m.PhotoDNA, sizeof(photodna));
        cJSON_AddStringToObject(obj, "PhotoDNA", photodna);
    }

    return obj;
}

/**
 * @brief Writes a single VICS Media record (with its MediaFiles array) to an open VICS file.
 *
 * Uses cJSON to build and serialise the record; all string escaping is handled automatically.
 *
 * @param vicFile FILE pointer previously opened by openVICSFile.
 * @param record  Pointer to the VICSRecord to write.
 * @return 0 on success, -1 if vicFile or record is NULL or the mandatory MD5 field is absent,
 *         -2 if cJSON serialisation fails.
 *
 * @see openVICSFile
 */
int writeMediaRecord(FILE* vicFile, VICSRecord* record)
{
    if (vicFile == NULL || record == NULL) return -1;

    cJSON* mediaObj = buildMediaCJSON(record->vMedia);
    if (mediaObj == NULL) return -1;

    if (record->noMediaFiles > 0)
    {
        cJSON* filesArray = cJSON_CreateArray();
        for (int i = 0; i < record->noMediaFiles; i++)
        {
            cJSON* fileObj = buildMediaFileCJSON(&record->vMediaFiles[i]);
            if (fileObj != NULL)
                cJSON_AddItemToArray(filesArray, fileObj);
        }
        cJSON_AddItemToObject(mediaObj, "MediaFiles", filesArray);
    }

    if (record->noMediaMetadata > 0)
    {
        cJSON* metaArray = cJSON_CreateArray();
        for (int i = 0; i < record->noMediaMetadata; i++)
        {
            VICSMediaMetadata* m = &record->vMediaMetaData[i];
            if (m->PropertyName == NULL || m->PropertyValue == NULL) continue;
            cJSON* metaObj = cJSON_CreateObject();
            cjsonAddWide(metaObj, "PropertyName",  m->PropertyName);
            cjsonAddWide(metaObj, "PropertyValue", m->PropertyValue);
            cJSON_AddItemToArray(metaArray, metaObj);
        }
        cJSON_AddItemToObject(mediaObj, "MediaMetadata", metaArray);
    }

    char* jsonStr = cJSON_Print(mediaObj);
    cJSON_Delete(mediaObj);

    if (jsonStr == NULL) return -2;

    fprintf(vicFile, "%s", jsonStr);
    cJSON_free(jsonStr);
    fflush(vicFile);
    return 0;
}

/**
 * @brief Populates a VICSMedia record from the current row of an SQLite statement.
 *
 * @param recMedia  Reference to the VICSMedia struct to populate.
 * @param statement Prepared and stepped SQLite statement positioned on a valid row.
 *
 * @see VICSMedia
 */
void extractVICSMediaSQL(VICSMedia &recMedia,sqlite3_stmt* statement)
{
    recMedia.MediaID = sqlite3_column_int64(statement,0);
    recMedia.Category = sqlite3_column_int(statement,1);
    int CheckSize = sqlite3_column_bytes16(statement,2);
    if (CheckSize < 10)
    {
        recMedia.SHA1[0] = L'\0';
    }
    else
    {
        wcsncpy(recMedia.SHA1, (wchar_t*)sqlite3_column_text16(statement,2), 40);
        recMedia.SHA1[40] = L'\0';
    }
    wcsncpy(recMedia.MD5, (wchar_t*)sqlite3_column_text16(statement,3), 32);
    recMedia.MD5[32] = L'\0';
    recMedia.VictimID = sqlite3_column_int(statement,4);
    recMedia.OffenderID = sqlite3_column_int(statement,5);
    recMedia.IsDistributed = sqlite3_column_int(statement,6);
    CheckSize = sqlite3_column_bytes16(statement,7);
    if (CheckSize == 0) { recMedia.Comments = NULL; }
    else {
            recMedia.Comments = new wchar_t[CheckSize + 2];
            wcscpy(recMedia.Comments, (wchar_t*)sqlite3_column_text16(statement,7));
        }
    CheckSize = sqlite3_column_bytes16(statement,8);
    if (CheckSize == 0) { recMedia.Tags = NULL; }
    else {
            recMedia.Tags = new wchar_t[CheckSize + 2];
            wcscpy(recMedia.Tags, (wchar_t*)sqlite3_column_text16(statement,8));
        }
    CheckSize = sqlite3_column_bytes16(statement,9);
    if (CheckSize == 0) { recMedia.Series = NULL; }
    else {
            recMedia.Series = new wchar_t[CheckSize + 2];
            wcscpy(recMedia.Series, (wchar_t*)sqlite3_column_text16(statement,9));
        }
    recMedia.MediaSize = sqlite3_column_int64(statement,10);
    CheckSize = sqlite3_column_bytes16(statement,11);
    recMedia.RelativeFilePath =  new wchar_t[CheckSize + 1];
    wcscpy(recMedia.RelativeFilePath,(wchar_t*)sqlite3_column_text16(statement,11));
    INT64 timeTmp = sqlite3_column_int64(statement,12);
    FILETIME tmpFileTime;
    memcpy(&tmpFileTime,&timeTmp,sizeof(tmpFileTime));
    recMedia.DateUpdated = tmpFileTime;
    recMedia.timeZone = sqlite3_column_int64(statement,13);
    CheckSize = sqlite3_column_bytes16(statement,14);
    if (CheckSize == 0) { recMedia.PrecatSource = NULL; }
    else {
            recMedia.PrecatSource = new wchar_t[CheckSize + 2];
            wcscpy(recMedia.PrecatSource, (wchar_t*)sqlite3_column_text16(statement,14));
        }
    recMedia.IsSuspected = sqlite3_column_int64(statement,15);
    CheckSize = sqlite3_column_bytes16(statement,16);
    if (CheckSize == 0) { recMedia.MimeType = NULL; }
    else {
            recMedia.MimeType = new wchar_t[CheckSize + 2];
            wcscpy(recMedia.MimeType, (wchar_t*)sqlite3_column_text16(statement,16));
        }
    CheckSize = sqlite3_column_bytes16(statement,17);
    if (CheckSize == 0) { recMedia.PhotoDNA[0] = '\0'; }
        else {
            wcsncpy((wchar_t*)recMedia.PhotoDNA, (wchar_t*)sqlite3_column_text16(statement,17), 255);
            recMedia.PhotoDNA[255] = L'\0';
        }
}

/**
 * @brief Populates a VICSMediaFile record from the current row of an SQLite statement.
 *
 * @param recMediaFile Reference to the VICSMediaFile struct to populate.
 * @param statement    Prepared and stepped SQLite statement positioned on a valid row.
 *
 * @see VICSMediaFile
 */
void extractVICSMediaFileSQL(VICSMediaFile &recMediaFile,sqlite3_stmt* statement)
{
    wcsncpy(recMediaFile.MD5, (wchar_t*)sqlite3_column_text16(statement,0), 32);
    recMediaFile.MD5[32] = L'\0';
    //Filename
    int CheckSize = sqlite3_column_bytes16(statement,1);
    recMediaFile.fileName =  new wchar_t[CheckSize + 2];
    wcscpy(recMediaFile.fileName, (wchar_t*)sqlite3_column_text16(statement,1));
    //file path
    CheckSize = sqlite3_column_bytes16(statement,2);
    recMediaFile.filePath =  new wchar_t[CheckSize + 2];
    wcscpy(recMediaFile.filePath, (wchar_t*)sqlite3_column_text16(statement,2));
    //Created
    INT64 timeTmp = sqlite3_column_int64(statement,3);
    FILETIME tmpFileTime;
    if (timeTmp < 145452016110000000)
    {
        memcpy(&tmpFileTime,&timeTmp,sizeof(tmpFileTime));
        recMediaFile.created = tmpFileTime;
    }
    //modified
    timeTmp = sqlite3_column_int64(statement,4);
    if (timeTmp < 145452016110000000)
    {
        memcpy(&tmpFileTime,&timeTmp,sizeof(tmpFileTime));
        recMediaFile.written = tmpFileTime;
    }
    //accessed
    timeTmp = sqlite3_column_int64(statement,5);
    if (timeTmp < 145452016110000000)
    {
        memcpy(&tmpFileTime,&timeTmp,sizeof(tmpFileTime));
        recMediaFile.accessed = tmpFileTime;
    }
    recMediaFile.unallocated = sqlite3_column_int(statement,6);
    //sourceID
    CheckSize = sqlite3_column_bytes16(statement,7);
    if (CheckSize == 0) { recMediaFile.sourceID = NULL; }
    else {
            recMediaFile.sourceID = new wchar_t[CheckSize + 2];
            wcscpy(recMediaFile.sourceID, (wchar_t*)sqlite3_column_text16(statement,7));
        }
    recMediaFile.physicalLocation = sqlite3_column_int64(statement,8);
    recMediaFile.deleted = sqlite3_column_int(statement,9);
    //parentMD5
    CheckSize = sqlite3_column_bytes16(statement,10);
    if (CheckSize == 0) { recMediaFile.parentMD5[0] = L'\0'; }
    else {
            wcsncpy(recMediaFile.parentMD5, (wchar_t*)sqlite3_column_text16(statement,10), 32);
            recMediaFile.parentMD5[32] = L'\0';
        }
    //parentName
    CheckSize = sqlite3_column_bytes16(statement,11);
    if (CheckSize == 0) { recMediaFile.parentName = NULL; }
    else {
            recMediaFile.parentName = new wchar_t[CheckSize + 2];
            wcscpy(recMediaFile.parentName, (wchar_t*)sqlite3_column_text16(statement,11));
        }
    //parentPath
    CheckSize = sqlite3_column_bytes16(statement,12);
    if (CheckSize == 0) { recMediaFile.parentFilePath = NULL; }
    else {
            recMediaFile.parentFilePath = new wchar_t[CheckSize + 2];
            wcscpy(recMediaFile.parentFilePath, (wchar_t*)sqlite3_column_text16(statement,12));
        }
    recMediaFile.parentPhysLoc = sqlite3_column_int64(statement,13);
}

/**
 * @brief Populates a VICSMediaMetadata record from the current row of an SQLite statement.
 *
 * @param record    Pointer to the VICSMediaMetadata struct to populate.
 * @param statement Prepared and stepped SQLite statement positioned on a valid row.
 *
 * @see VICSMediaMetadata
 */
void extractVICSMediaMetadataSQL(VICSMediaMetadata* record,sqlite3_stmt* statement)
{
    wcscpy(record->MD5, (wchar_t*)sqlite3_column_text16(statement,0));
    int CheckSize = sqlite3_column_bytes16(statement,1);
    record->PropertyName =  new wchar_t[CheckSize + 2];
    wcscpy(record->PropertyName, (wchar_t*)sqlite3_column_text16(statement,1));
    CheckSize = sqlite3_column_bytes16(statement,2);
    record->PropertyValue =  new wchar_t[CheckSize + 2];
    wcscpy(record->PropertyValue, (wchar_t*)sqlite3_column_text16(statement,2));
}

/**
 * @brief Validates a FILETIME value, rejecting zero, implausibly old, and future timestamps.
 *
 * Rejects timestamps that are zero, below the minTime constant, or more than two years
 * in the future relative to the current system time.
 *
 * @param timestamp The FILETIME value to validate.
 * @return true if the timestamp is valid, false otherwise.
 */
bool validFiletime(FILETIME timestamp)
{
    if (timestamp.dwHighDateTime == 0 && timestamp.dwLowDateTime ==0)
    {
        return false;
    }
    if (timestamp.dwHighDateTime < minTime)
    {
        return false;
    }
    uint64_t tempTime = (uint64_t(timestamp.dwHighDateTime) << 32 | uint64_t(timestamp.dwLowDateTime));
    SYSTEMTIME st;
    GetSystemTime(&st);
    st.wYear +=2;
    FILETIME ft;
    bool check = SystemTimeToFileTime(&st,&ft);
    uint64_t checkTime = (uint64_t(ft.dwHighDateTime) << 32) | uint64_t(ft.dwLowDateTime);
    if (tempTime > checkTime)
    {
        return false;
    }
    return true;
}
