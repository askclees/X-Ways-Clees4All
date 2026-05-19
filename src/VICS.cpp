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



/*  Section: VICS File Functions  */

/*Function: openVICSFile
    Function to create new file and write VICS case information to start of file.

    Parameters are a valid file path for the file output and the program version

    Output is not complete (valid) VICS JSON entry as it requires closing via <closeVICSFile>

    Parameters:
        char* filePath              - NULL terminated string containing filepath for file to be created
        const wchar_t* progVersion  - Program version provided in Wide character text format

    Returns:
        FILE* of opened VICS file

    See Also:
        Related Functions   - <closeVICSFile>
        Called by           - <setupVicsExport>

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

/*Function: closeVICSFile
    Writes data to VICS file that closes the entries and then closes the file.

    Requires valid FILE* as parameter, should have been opened using openVICSFile

    Parameters:
        FILE* vFile -   Valid VICS output FILE

    Returns:
        Int result of fclose function on FILE parameter

    See Also:
        Related function    -   <openVICSFile>
        Called by           -   <writeRecords>
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

//initialisation records
void InitializeMediaRecord(VICSMedia& record)
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

void InitializeAltHashRecord(VICSAltHash& record)
{
    record.hashName = NULL;
    record.hashValue = NULL;
    record.MD5[0] = L'\0';
}

void InitializeMediaFileRecord(VICSMediaFile& record)
{
    record.deleted = FALSE;
    record.unallocated = FALSE;

    record.fileName = NULL;
    record.filePath = NULL;
    record.parentFilePath = NULL;
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

void InitializeVICSRecord(VICSRecord& record)
{
    record.noMediaFiles = 0;
    record.noAltHash = 0;
    record.noExif = 0;
    record.noSegments = 0;
    record.noRepository = 0;

    InitializeMediaRecord(record.vMedia);
}

void InitializeRepositoryRecord(VICSRepository& record)
{
    record.repositoryName = NULL;
    record.MD5[0] = L'\0';
}

void InitializeExifRecord(VICSExif& record)
{
    record.propertyName = NULL;
    record.propertyValue = NULL;
    record.MD5 =  NULL;
}

void InitializeSegmentRecord(VICSSegment& record)
{
    record.Start = NULL;
    record.End = NULL;
    record.MD5[0] = L'\0';

    record.segmentIndex = 0;
    record.category = 0;
}

/*Section: VICS Record Deallocation Functions*/

void deallocateVICSRecord(VICSRecord record)
{
    deallocateMediaRecord(record.vMedia);
    if (record.noMediaFiles !=0)
    {
        for (int i=0;i<record.noMediaFiles;i++)
        {
            deallocateMediaFileRecord(record.vMediaFiles[i]);
        }
        record.noMediaFiles= 0;
    }
    //1.41 add cleaning of media metadata records
    if (record.noMediaMetadata !=0)
    {
        for (int i=0;i<record.noMediaMetadata;i++)
        {
            deallocateMediaMetadataRecord(record.vMediaMetaData[i]);
        }
        record.noMediaFiles= 0;
    }
}

void deallocateMediaRecord(VICSMedia &record)
{
    if (record.Comments != NULL) {delete[] record.Comments;}
    if (record.MimeType != NULL) {delete[] record.MimeType;}
    if (record.PrecatSource != NULL) {delete[] record.PrecatSource;}
    if (record.RelativeFilePath != NULL) {delete[] record.RelativeFilePath;}
    if (record.Series != NULL) {delete[] record.Series;}
    if (record.Tags != NULL) {delete[] record.Tags;}
}

void deallocateMediaMetadataRecord(VICSMediaMetadata &record)
{
    if (record.PropertyName != NULL) {delete[] record.PropertyName;}
    if (record.PropertyValue != NULL) {delete[] record.PropertyValue;}
}

void deallocateMediaFileRecord(VICSMediaFile &record)
{
    if (record.fileName != NULL) {delete[] record.fileName;}
    if (record.filePath != NULL) {delete[] record.filePath;}
    if (record.parentFilePath != NULL) {delete[] record.parentFilePath;}
    if (record.parentName != NULL) {delete[] record.parentName;}
    if (record.sourceID != NULL) {delete[] record.sourceID;}
}

void freeVicsCaseData()
{
    if (vCaseData.CaseNumber != nullptr)    { delete[] vCaseData.CaseNumber; }
    if (vCaseData.ContactEmail != nullptr)  { delete[] vCaseData.ContactEmail; }
    if (vCaseData.ContactName != nullptr)   { delete[] vCaseData.ContactName; }
    if (vCaseData.ContactOrg != nullptr)    { delete[] vCaseData.ContactOrg; }
    if (vCaseData.ContactPhone != nullptr)  { delete[] vCaseData.ContactPhone; }
    if (vCaseData.ContactTitle != nullptr)  { delete[] vCaseData.ContactTitle; }
}

/*Section: VICS Record Size Functions*/

/*Function: getMediaFileRecordSize
    Returns the total character count of all variable-length wchar_t* fields in a VICSMediaFile record.
    Used by insertMediaFileRecord to size the SQL query buffer.

    See Also: <insertMediaFileRecord>
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

/*Function: getMediaRecordSize
    Returns the total character count of all variable-length wchar_t* fields in a VICSMedia record.
    Used by insertMediaRecord to size the SQL query buffer.

    See Also: <insertMediaRecord>
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

/*Section: cJSON Helper Functions*/

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
    char* utf8 = convertWideToChar((wchar_t*)wstr);
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
        snprintf(buf, sizeof(buf), "%d-%02d-%02dT%02d:%02d:%02d.%07dZ",
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

/*Section: VICS Writing Functions*/

/*Function: writeMediaRecord
    Writes a single VICS Media record (with its MediaFiles array) to a FILE previously opened
    with openVICSFile. Uses cJSON to build and serialise the record — all string escaping is
    handled automatically.

    Returns:
         0  - success
        -1  - NULL vicFile or record pointer, or mandatory MD5 absent
        -2  - cJSON serialisation failure

    See Also:
        <openVICSFile>
        <VICSRecord>
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

    char* jsonStr = cJSON_Print(mediaObj);
    cJSON_Delete(mediaObj);

    if (jsonStr == NULL) return -2;

    fprintf(vicFile, "%s", jsonStr);
    cJSON_free(jsonStr);
    fflush(vicFile);
    return 0;
}

/*Section: VICS SQL Extraction Functions*/

/*Function: extractVICSMediaSQL
    Functions takes a pointer a VICSMedia record and a sqlite3_stmt
    Fills in the details from the SQL results into the VICSMedia record

    See also: <VICSMedia>
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
        wcscpy(recMedia.SHA1, (wchar_t*)sqlite3_column_text16(statement,2));
    }
    wcscpy(recMedia.MD5, (wchar_t*)sqlite3_column_text16(statement,3));
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
            wcscpy((wchar_t*)recMedia.PhotoDNA, (wchar_t*)sqlite3_column_text16(statement,17));
        }
}

/*Function: extractVICSMediaFileSQL
    Functions takes a pointer a VICSMediaFile record and a sqlite3_stmt
    Fills in the details from the SQL results into the VICSMediaFile record

    See also: <VICSMediaFile>
*/

void extractVICSMediaFileSQL(VICSMediaFile &recMediaFile,sqlite3_stmt* statement)
{
    wcscpy(recMediaFile.MD5, (wchar_t*)sqlite3_column_text16(statement,0));
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
    recMediaFile.sourceID =  new wchar_t[CheckSize + 2];
    wcscpy(recMediaFile.sourceID, (wchar_t*)sqlite3_column_text16(statement,7));
    recMediaFile.physicalLocation = sqlite3_column_int64(statement,8);
    recMediaFile.deleted = sqlite3_column_int(statement,9);
    //parentMD5
    CheckSize = sqlite3_column_bytes16(statement,10);
    if (CheckSize == 0) { recMediaFile.parentMD5[0] = L'\0'; }
    else {
            wcscpy(recMediaFile.parentMD5, (wchar_t*)sqlite3_column_text16(statement,10));
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
    recMediaFile.parentPhysLoc = sqlite3_column_int(statement,13);
}

/*Function: extractVICSMediaMetadataSQL
    Functions takes a pointer a VICSMediaMetadata record and a sqlite3_stmt
    Fills in the details from the SQL results into the VICSMediaMetadata record

    See also: <VICSMediaMetadata>
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

/*  Section: VICS Validation Functions  */

/*Function: validFiletime
    Validates a FILETIME: rejects zero, values below minTime, and timestamps more than
    2 years in the future.  Added in 1.50.

    Returns: true if valid, false otherwise
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
