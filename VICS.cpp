//std headers
#include <cwchar>
#include <cstdio>
#include <windows.h>
#include <stdint.h>

//project headers
#include "VICS.h"
#include "utility.h"

//prototyping
wchar_t* generateVicsMediaMetadataString(VICSMediaMetadata record);
wchar_t* generateVicsSegmentString(VICSSegment record);
wchar_t* generateVicsEXIFString(VICSExif record);
wchar_t* getMediaMetadataString(VICSMediaMetadata* records, int number);

//1.50 function to validate FILETIME
bool validFiletime(FILETIME timestamp);

//globals
VICSCaseData vCaseData;
static unsigned char BOM[]={0xFF,0xFE};
//1.50 added minimum value for FILETIME timestamp
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
	//fwrite(BOM,1,sizeof(BOM),newFile);
	//1.38 removed L from beginning of strings to change from wide char to single byte
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
	//fprintf(newFile,"\"CaseNumber\":\"%ls\",\r\n\t", vCaseData.CaseNumber);
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
    //1.38 removed L from beginning of strings to change from wide char to single byte
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
    //set all ints to 0
    record.Category = 0;
    record.MediaID = 0;
    record.MediaSize = 0;
    record.timeZone = 0;

    //set all BOOL to false
    record.IsDistributed = FALSE;
    record.IsPreCat = FALSE;
    record.IsSuspected = FALSE;
    record.OffenderID = FALSE;
    record.VictimID = FALSE;

    //set all wchar_t* to NULL
    record.Comments = NULL;
    record.MimeType = NULL;
    record.PrecatSource = NULL;
    record.RelativeFilePath = NULL;
    record.Series = NULL;
    record.Tags = NULL;

    //set fixed length wchar_t to '\0'
    record.MD5[0] = L'\0';
    record.SHA1[0] = L'\0';

    //set FILETIME to 0
    record.DateUpdated.dwHighDateTime = 0;
    record.DateUpdated.dwLowDateTime = 0;

}

void InitializeAltHashRecord(VICSAltHash& record)
{
    //all wchar_t
    record.hashName = NULL;
    record.hashValue = NULL;
    record.MD5[0] = L'\0';
}

void InitializeMediaFileRecord(VICSMediaFile& record)
{
    //BOOL
    record.deleted = FALSE;
    record.unallocated = FALSE;

    //wchar_t*
    record.fileName = NULL;
    record.filePath = NULL;
    record.parentFilePath = NULL;
    record.parentMD5[0]=L'\0';
    record.parentName = NULL;
    record.sourceID = NULL;
    record.parentMD5[0]=L'\0';

    //FILETIME
    record.accessed.dwHighDateTime=0;
    record.accessed.dwLowDateTime=0;
    record.created.dwHighDateTime=0;
    record.created.dwLowDateTime=0;
    record.written.dwHighDateTime=0;
    record.written.dwLowDateTime=0;

    //ints
    record.parentPhysLoc = 0;
    record.physicalLocation = 0;
}


void InitializeVICSRecord(VICSRecord& record)
{
    //set int's to 0
    record.noMediaFiles = 0;
    record.noAltHash = 0;
    record.noExif = 0;
    record.noSegments = 0;
    record.noRepository = 0;

    InitializeMediaRecord(record.vMedia);

}

void InitializeRepositoryRecord(VICSRepository& record)
{
    //all wchar_t
    record.repositoryName = NULL;
    record.MD5[0] = L'\0';
}


void InitializeExifRecord(VICSExif& record)
{
    //all wchar_t
    record.propertyName = NULL;
    record.propertyValue = NULL;
    record.MD5 =  NULL;
}


void InitializeSegmentRecord(VICSSegment& record)
{
    //all wchar_t
    record.Start = NULL;
    record.End = NULL;
    record.MD5[0] = L'\0';

    //ints next
    record.segmentIndex = 0;
    record.category = 0;
}

/*Section: VICS Record Deallocation Functions*/

/*Function: deallocateVICSRecord
    Safely cleans deallocated resources for a VICS Record.
    Requires a VICSRecord record as a parameter

    Currently only deallocates MediaFile and MediaMetadata Entries

    Returns:
        None

    See Also:
        Called by:  -   <writeRecords>
        Calls       -   None
        Related     -   <VICSRecord>
*/

//cleaning up records
void deallocateVICSRecord(VICSRecord record)
{
    //used to clear up record after usage
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


/*Function: deallocateMediaRecord
    Safely cleans deallocated resources for a VICS Media Record.

    Requires a pointer to a VICSMedia record as a parameter

    Returns:
        None

    See Also:
        Called by:  -   <deallocateVICSRecord>
        Calls       -   None
        Related     -   <VICSMedia>
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

/*Function: deallocateMediaMetadataRecord

    Safely cleans deallocated resources for a VICS Media Metadata Record.

    Requires a pointer to a VICSMediaMetadata record as a parameter

    Returns:
        None

    See Also:
        Called by:  -   <deallocateVICSRecord>
        Calls       -   None
        Related     -   <VICSMediaMetadata>
*/

void deallocateMediaMetadataRecord(VICSMediaMetadata &record)
{
    if (record.PropertyName != NULL) {delete[] record.PropertyName;}
    if (record.PropertyValue != NULL) {delete[] record.PropertyValue;}
}

/*Function: deallocateMediaFileRecord
    Safely cleans deallocated resources for a VICS MediaFile Record.
    Requires a pointer to a VICSMediaFile record as a parameter
    Currently only used by an unused function

    Returns:
        None

    See Also:
        <deallocateVICSRecord>
        <VICSMediaFile>
*/

void deallocateMediaFileRecord(VICSMediaFile &record)
{
    if (record.fileName != NULL) {delete[] record.fileName;}
    if (record.filePath != NULL) {delete[] record.filePath;}
    if (record.parentFilePath != NULL) {delete[] record.parentFilePath;}
    if (record.parentName != NULL) {delete[] record.parentName;}
    if (record.sourceID != NULL) {delete[] record.sourceID;}
}


/*Function: freeVicsCaseData
    Safely cleans deallocated resources for a VICS Case Record.
    No arguments, currently accesses global variable, should be renamed to deallocatedCaseRecord
    This should be changed to take record as parameter
    Called from caseCleanup function

    Returns:
        None

    See Also:
        <caseCleanup>
        <VICSCaseData>
*/

void freeVicsCaseData()
{
    if (vCaseData.CaseNumber != nullptr)
    {
        delete[] vCaseData.CaseNumber;
    }
    if (vCaseData.ContactEmail != nullptr)
    {
        delete[] vCaseData.ContactEmail;
    }
    if (vCaseData.ContactName != nullptr)
    {
        delete[] vCaseData.ContactName;
    }
    if (vCaseData.ContactOrg != nullptr)
    {
        delete[] vCaseData.ContactOrg;
    }
    if (vCaseData.ContactPhone != nullptr)
    {
        delete[] vCaseData.ContactPhone;
    }
    if (vCaseData.ContactTitle != nullptr)
    {
        delete[] vCaseData.ContactTitle;
    }

}

/*Section: VICS Writing Functions*/

/*Function: writeMediaRecord
    Writes a VICS Record to a FILE previously opened with openVICSFile
    Converts output to UTF-8 before writing to file as of version 1.38
    Function is currently unused.

    Returns:
        0 - if successful
        < 0 - on error
        -1 - denotes generateVicsMediaString returned NULL

    See Also:
        <openVICSFile>
        <VICSRecord>
*/

//writing records
int writeMediaRecord(FILE* vicFile, VICSRecord* record)
{
    INT64 recordSize = 0;
	//get the different sections
	wchar_t* vMediaTxt = generateVicsMediaString(record->vMedia);
    if (vMediaTxt == NULL)
    {
        return -1;
    }
    else
    {
        recordSize+= wcslen(vMediaTxt);
    }
    //create alternative hashes
    wchar_t* vAltHashtxt=NULL;
    if (record->noAltHash !=0)
    {
        vAltHashtxt =getAltHashRecordsString(record->vAltHashes,record->noAltHash);
        recordSize += wcslen(vAltHashtxt);
    }
    //if mediafile records, open it!
    wchar_t* vMediaFiletxt =NULL;
    if (record->noMediaFiles!=0)
    {
        vMediaFiletxt =getMediaRecordString(record->vMediaFiles, record->noMediaFiles);
        recordSize+= wcslen(vMediaFiletxt);
    }
    //create segment records
    wchar_t* vSegmenttxt=NULL;
    if (record->vSegment!=0)
    {
        vSegmenttxt = getSegmentRecordString(record->vSegment, record->noSegments);
        recordSize+= wcslen(vSegmenttxt);
    }
    //create exif
    wchar_t* vExiftxt=NULL;
    if (record->noExif !=0)
    {
        vExiftxt = getExifRecordsString(record->vExif, record->noExif);
        recordSize+= wcslen(vExiftxt);
    }
    //add repositories
    wchar_t* vRepositorytxt=NULL;
    if (record->noRepository!=0)
    {
        vRepositorytxt = getRepositoryRecordString(record->vRepository, record->noRepository);
        recordSize+= wcslen(vRepositorytxt);
    }
    //add Media Metadata
    wchar_t* vMediaMetadatatxt=NULL;
    if (record->noMediaMetadata!=0)
    {
        vMediaMetadatatxt = getMediaMetadataString(record->vMediaMetaData, record->noMediaMetadata);
        recordSize+= wcslen(vMediaMetadatatxt);
    }
    //add 2048 to size for good measure!
    recordSize+=2048;
    wchar_t* output = new wchar_t[recordSize];
    output[0]=L'\0';
    //open the record before writing data
    wcscat(output,L"{");
    wcscat(output,vMediaTxt);
    delete[] vMediaTxt;
    if (record->noAltHash != 0)
    {
        wcscat(output, vAltHashtxt);
        delete[] vAltHashtxt;
    }
    if (record->noExif != 0)
    {
        //add a comman between output
        wcscat(output,L",");
        wcscat(output, vExiftxt);
        delete[] vExiftxt;
    }
    if (record->noMediaFiles != 0)
    {
        //add a comman between output
        wcscat(output,L",");
        wcscat(output, vMediaFiletxt);
        delete[] vMediaFiletxt;
    }
    if (record->noRepository != 0)
    {
        wcscat(output, vRepositorytxt);
        delete[] vRepositorytxt;
    }
    if (record->noSegments != 0)
    {
        wcscat(output, vSegmenttxt);
        delete[] vSegmenttxt;
    }
    //1.41 added media metadata
    if (record->noMediaMetadata != 0)
    {
        wcscat(output,L",\r\n\t\t\t");
        wcscat(output, vMediaMetadatatxt);
        delete[] vMediaMetadatatxt;
    }
    //more to come here
    //1.38 need to change output from UTF-16 to UTF-8
    char* UTF8Output = convertWideToChar(output);
    fprintf(vicFile,"%s\r\n",UTF8Output);
    delete[] UTF8Output;
    //close media record
    //1.38 removed L from beginning of strings to change from wide char to single byte
    fprintf(vicFile,"\t\t}");
    fflush(vicFile);
    delete[] output;
    return 0;
}

/*Section: VICS Record Size Functions*/

/*Function: getMediaFileRecordSize
    Function to determine the size of strings associated with any wchar_t* variables in a VICSMediaFile record
    Ignores any values that are NULL. Can be used to determine how much space will be required to house a record.
    Currently only called from insertMediaFileRecord

    Returns:
        INT64 value - size of all non-fixed strings in characters

    See Also:
        <insertMediaFileRecord>
        <VICSMediaFile>
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
    Function to determine the size of strings associated with any wchar_t* variables in a VICSMedia record
    Ignores any values that are NULL. Can be used to determine how much space will be required to house a record.
    Currently only called from insertMediaRecord

    Returns:
        INT64 value - size of all non-fixed strings in characters

    See Also:
        <insertMediaRecord>
        <VICSMedia>
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

/*Function: getAltHashRecordsString
    Function to create a JSON representation of an VICSAltHash record
    Returned buffer will currently be 65536 bytes in length regardless of data size
    Currently unused - Requires some work prior to implementing

    Returns:
        wchar_t* pointing to a NULL terminated VICS JSON string

    See Also:
        <VICSAltHash>
*/



/*Section: VICS String Generation Functions*/

/*Function: generateVicsMediaString
    Creates a VICS JSON string representation of a VICS Media Record.
    Requires a VICSMedia record as a parameter

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently maximum size of buffer is 8192 bytes

    See Also:
        <VICSMedia>
*/

wchar_t* generateVicsMediaString(VICSMedia record)
{
	wchar_t* retString = new wchar_t[8192];
	wchar_t buffer[2048];
	retString[0]=L'\0';
	swprintf(retString,L"\n\t\t\"MediaID\":%d",record.MediaID);
	if (record.Category != 0)
	{
		swprintf(buffer,L",\n\t\t\"Category\":%d",record.Category);
		wcscat(retString,buffer);
		buffer[0]=L'\0';
	}
	if (record.MD5 != NULL)
	{
		swprintf(buffer,L",\n\t\t\"MD5\":\"%ls\"",record.MD5);
		wcscat(retString,buffer);
		buffer[0]=L'\0';
	}
    else {
        delete[] retString;
        return NULL;}
	if (record.Comments != NULL)
	{
		swprintf(buffer,L",\n\t\t\"Comments\":\"%ls\"",record.Comments);
		wcscat(retString,buffer);
		buffer[0]=L'\0';
	}
	if (record.VictimID == TRUE){
		wcscat(retString,L",\n\t\t\"VictimIdentified\":\"true\"\0");
	}
	if (record.IsDistributed == TRUE){
		wcscat(retString,L",\n\t\t\"IsDistributed\":\"true\"\0");
	}
	if (record.Series != NULL)
	{
		swprintf(buffer,L",\n\t\t\"Series\":\"%ls\"",record.Series);
		wcscat(retString,buffer);
		buffer[0]=L'\0';
	}
	if (record.SHA1 != NULL)
	{
		swprintf(buffer,L",\n\t\t\"SHA1\":\"%ls\"",record.SHA1);
		wcscat(retString,buffer);
		buffer[0]=L'\0';
	}
	if (record.Tags != NULL)
	{
		swprintf(buffer,L",\n\t\t\"Tags\":\"%ls\"",record.Tags);
		wcscat(retString,buffer);
		buffer[0]=L'\0';
	}
	//sort out filetime
	//1.50 if (record.DateUpdated.dwHighDateTime != 0 && record.DateUpdated.dwLowDateTime != 0)
	if (validFiletime(record.DateUpdated))
    {
        SYSTEMTIME retTime;
        LPSYSTEMTIME timePtr= &retTime;
        if (FileTimeToSystemTime(&record.DateUpdated,timePtr))
        {
            swprintf(buffer,L",\n\t\t\"DateUpdated\":\"%d-%02d-%02dT%02d:%02d:%02d.%03d%+02d:00Z\"",retTime.wYear, retTime.wMonth, retTime.wDay, retTime.wHour, retTime.wMinute, retTime.wSecond, retTime.wMilliseconds,record.timeZone);
            wcscat(retString,buffer);
            buffer[0]=L'\0';
        }
    }
	if (record.MediaSize != 0)
	{
		swprintf(buffer,L",\n\t\t\"MediaSize\":%llu",record.MediaSize);
		wcscat(retString,buffer);
		buffer[0]=L'\0';
	}
	if (record.RelativeFilePath != NULL)
    {
        swprintf(buffer,L",\n\t\t\"RelativeFilePath\":\"%ls\"",record.RelativeFilePath);
		wcscat(retString,buffer);
		buffer[0]=L'\0';
    }
	if (record.OffenderID == TRUE){
		wcscat(retString,L",\n\t\t\"OffenderIdentified\":\"true\"");
	}
	if (record.IsPreCat == TRUE){
		wcscat(retString,L",\n\t\t\"IsPrecategorized\":\"true\"");
		//check if precat source is set
		if (record.PrecatSource!=NULL)
        {
            swprintf(buffer,L",\n\t\t\"PrecategorizationSource\":\"%ls\"",record.RelativeFilePath);
            wcscat(retString,buffer);
            buffer[0]=L'\0';
        }
	}
	if (record.IsSuspected == TRUE){
		wcscat(retString,L",\n\t\t\"IsSuspected\":\"true\"\"");
	}
	if (record.PhotoDNA[0] != L'\0')
    {
        swprintf(buffer,L",\n\t\t\"PhotoDNA\":\"%ls\"",record.PhotoDNA);
		wcscat(retString,buffer);
		buffer[0]=L'\0';
    }
	return retString;
}

/*Function: generateVicsAltHashString
    Creates a VICS JSON string representation of a VICS Alternative Hash Record.
    Requires a VICSAltHash record as a parameter
    Currently unused

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently maximum size of buffer is 8192 bytes

    See Also:
        <VICSAltHash>
*/

wchar_t* generateVicsAltHashString(VICSAltHash record)
{
    wchar_t* retString = new wchar_t[8192];
    wchar_t buffer[2048];
    buffer[0] = L'\0';
    retString[0]=L'\0';
	//must have MD5, hashname and Hashvalue
	if (record.MD5[0] != L'\0')
    {
        swprintf(buffer,L"\t\t\t\t\"MD5\":\"%ls\"",record.MD5);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
	if (record.hashName != NULL)
    {
        swprintf(buffer,L",\"HashName\":\"%ls\"",record.hashName);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
	if (record.hashName != NULL)
    {
        swprintf(buffer,L",\"HashValue\":\"%ls\"",record.hashName);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
    wcscat(retString,L"\r\n");
    return retString;
}

/*Function: generateVicsMediaFileString
    Creates a VICS JSON string representation of a VICS Media File Record.
    Requires a VICSMediaFile record as a parameter

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently maximum size of buffer is 8192 bytes

    See Also:
        <VICSMediaFile>
*/

wchar_t* generateVicsMediaFileString(VICSMediaFile* record)
{
    wchar_t* retString = new wchar_t[16384];
    wchar_t buffer[4096];
    buffer[0] = L'\0';
    retString[0]=L'\0';
    //must have MD5, filename and filepath
	if (record->MD5[0] != L'\0')
    {
        swprintf(buffer,L"\t\t\t{\"MD5\":\"%ls\"",record->MD5);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
	if (record->fileName[0] != L'\0')
    {
        swprintf(buffer,L",\r\t\t\t\"FileName\":\"%ls\"",record->fileName);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
	if (record->filePath[0] != L'\0')
    {
        swprintf(buffer,4095,L",\r\t\t\t\"FilePath\":\"%ls\"",record->filePath);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
    //all optional, start with times
    //1.50 changed to use new function
    //if (record->created.dwHighDateTime != 0 && record->created.dwLowDateTime !=0)
    if (validFiletime(record->created))
    {
        SYSTEMTIME retTime;
        LPSYSTEMTIME timePtr= &retTime;
        if (FileTimeToSystemTime(&record->created,timePtr))
        {
            swprintf(buffer,L",\r\t\t\t\"Created\":\"%d-%02d-%02dT%02d:%02d:%02d.%07dZ\"",retTime.wYear, retTime.wMonth, retTime.wDay, retTime.wHour, retTime.wMinute, retTime.wSecond, retTime.wMilliseconds);
            wcscat(retString,buffer);
            buffer[0]=L'\0';
        }
    }
    //if (record->written.dwHighDateTime != 0 && record->written.dwLowDateTime !=0)
    if (validFiletime(record->written))
    {
        SYSTEMTIME retTime;
        LPSYSTEMTIME timePtr= &retTime;
        if (FileTimeToSystemTime(&record->written,timePtr))
        {
            swprintf(buffer,L",\r\t\t\t\"Written\":\"%d-%02d-%02dT%02d:%02d:%02d.%07dZ\"",retTime.wYear, retTime.wMonth, retTime.wDay, retTime.wHour, retTime.wMinute, retTime.wSecond, retTime.wMilliseconds);
            wcscat(retString,buffer);
            buffer[0]=L'\0';
        }
    }
    //if (record->accessed.dwHighDateTime != 0 && record->accessed.dwLowDateTime !=0)
    if (validFiletime(record->accessed))
    {
        SYSTEMTIME retTime;
        LPSYSTEMTIME timePtr= &retTime;
        if (FileTimeToSystemTime(&record->accessed,timePtr))
        {
            swprintf(buffer,L",\r\t\t\t\"Accessed\":\"%d-%02d-%02dT%02d:%02d:%02d.%07dZ\"",retTime.wYear, retTime.wMonth, retTime.wDay, retTime.wHour, retTime.wMinute, retTime.wSecond, retTime.wMilliseconds);
            wcscat(retString,buffer);
            buffer[0]=L'\0';
        }
    }
	if (record->unallocated == TRUE){
		wcscat(retString,L",\r\t\t\t\"Unallocated\":\"true\"");
		wcscat(retString, buffer);
        buffer[0]=L'\0';
	}
	if (record->deleted == TRUE){
		wcscat(retString,L",\r\t\t\t\"Deleted\":\"true\"");
		wcscat(retString, buffer);
        buffer[0]=L'\0';
	}
	if (record->parentMD5[0] != L'\0')
    {
        swprintf(buffer,L",\r\t\t\t\"ParentMD5\":\"%ls\"",record->parentMD5);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
	if (record->parentName != NULL)
    {
        swprintf(buffer,L",\r\t\t\t\"ParentFileName\":\"%ls\"",record->parentName);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
	if (record->parentFilePath != NULL)
    {
        swprintf(buffer,4095,L",\r\t\t\t\"ParentFilePath\":\"%ls\"",record->parentFilePath);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
	if (record->parentPhysLoc != 0)
	{
		swprintf(buffer,L",\r\t\t\t\"ParentPhysicalLocation\":%llu",record->parentPhysLoc);
		wcscat(retString,buffer);
		buffer[0]=L'\0';
	}
	if (record->physicalLocation != 0)
	{
		swprintf(buffer,L",\r\t\t\t\"PhysicalLocation\":%llu",record->physicalLocation);
		wcscat(retString,buffer);
		buffer[0]=L'\0';
	}
	if (record->sourceID != NULL)
    {
        swprintf(buffer,L",\r\t\t\t\"SourceID\":\"%ls\"",record->sourceID);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    wcscat(retString,L"}\0");
    return retString;

}


/*Function: generateVicsRepositoryString
    Creates a VICS JSON string representation of a VICS Repositiory Record.
    Requires a VICSRepository record as a parameter

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently maximum size of buffer is 8192 bytes

    See Also:
        <VICSRepository>
*/

wchar_t* generateVicsRepositoryString(VICSRepository record)
{
    wchar_t* retString = new wchar_t[8192];
    wchar_t buffer[2048];
    buffer[0] = L'\0';
    retString[0]=L'\0';
	//must have MD5, hashname and Repository Name
	if (record.MD5[0] != L'\0')
    {
        swprintf(buffer,L"\t\t\t\t\"MD5\":\"%ls\"",record.MD5);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
	if (record.repositoryName != NULL)
    {
        swprintf(buffer,L",\"RepositoryName\":\"%ls\"",record.repositoryName);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
    wcscat(retString,L"\r\n");
    return retString;
}

/*Function: generateVicsMediaMetadataString
    Creates a VICS JSON string representation of a VICS Media Metadata Record.
    Requires a VICSMediaMetadata record as a parameter
    Currently Unused
    Added in version 1.41

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently maximum size of buffer is 8192 bytes

    See Also:
        <VICSMediaMetadata>
*/

//1.41 added for generating metadata string
wchar_t* generateVicsMediaMetadataString(VICSMediaMetadata record)
{
    wchar_t buffer[2048];
    buffer[0] = L'\0';
	//must have MD5, PropertyName and PropertyValue
	if (record.MD5[0] == L'\0' or record.PropertyName == nullptr
     || record.PropertyValue == nullptr)
    {
        return NULL;
    }
    //calculate length and allocate data
    int dataLength = wcslen(record.PropertyValue) + wcslen(record.PropertyName) + 1024;
    wchar_t* retString = new wchar_t[dataLength];
    retString[0]=L'\0';
    //output MD5
    swprintf(buffer,L"\t\t\t\t\"MD5\":\"%ls\"",record.MD5);
    wcscat(retString, buffer);
    buffer[0]=L'\0';
    //output PropertyName
    swprintf(buffer,L",\"PropertyName\":\"%ls\"",record.PropertyName);
    wcscat(retString, buffer);
    buffer[0]=L'\0';
    //output PropertyValue
    swprintf(buffer,L",\"PropertyValue\":\"%ls\"",record.PropertyValue);
    wcscat(retString, buffer);
    buffer[0]=L'\0';
    //close record and return
    //wcscat(retString,L"\r\n");
    return retString;

}

/*Function: generateVicsSegmentString
    Creates a VICS JSON string representation of a VICS Segment Record.
    Requires a VICSSegment record as a parameter
    Currently Unused

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently maximum size of buffer is 8192 bytes

    See Also:
        <VICSSegment>
*/

wchar_t* generateVicsSegmentString(VICSSegment record)
{
    wchar_t* retString = new wchar_t[8192];
    wchar_t buffer[2048];
    buffer[0] = L'\0';
    retString[0]=L'\0';
	//must have MD5, hashname and Repository Name
	if (record.MD5[0] != L'\0')
    {
        swprintf(buffer,L"\t\t\t\t\"MD5\":\"%ls\"",record.MD5);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
    //add segment index
    swprintf(buffer,L",\"SegmentIndex\":%d",record.segmentIndex);
    wcscat(retString, buffer);
    buffer[0]=L'\0';
	if (record.Start != NULL)
    {
        swprintf(buffer,L",\"Start\":\"%ls\"",record.Start);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
	if (record.End != NULL)
    {
        swprintf(buffer,L",\"End\":\"%ls\"",record.End);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}

    //add Category
    swprintf(buffer,L",\"Category\":%d",record.category);
    wcscat(retString, buffer);
    buffer[0]=L'\0';
    wcscat(retString,L"\r\n");
    return retString;
}

/*Function: generateVicsEXIFString
    Creates a VICS JSON string representation of a VICS Exif Record.
    Requires a VICSExif record as a parameter
    Currently Unused

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently maximum size of buffer is 8192 bytes

    See Also:
        <VICSExif>
*/

wchar_t* generateVicsEXIFString(VICSExif record)
{
    wchar_t* retString = new wchar_t[8192];
    wchar_t buffer[2048];
    buffer[0] = L'\0';
    retString[0]=L'\0';
	//must have MD5, hashname and Repository Name
	if (record.MD5 != NULL)
    {
        swprintf(buffer,L"\t\t\t\t\"MD5\":\"%ls\"",record.MD5);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
	if (record.propertyName != NULL)
    {
        swprintf(buffer,L",\"PropertyName\":\"%ls\"",record.propertyName);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
	if (record.propertyValue != NULL)
    {
        swprintf(buffer,L",\"PropertyValue\":\"%ls\"",record.propertyValue);
        wcscat(retString, buffer);
        buffer[0]=L'\0';
    }
    else {
        delete[] retString;
        return NULL;}
    wcscat(retString,L"\r\n");
    return retString;
}

/*Function: getAltHashRecordsString
    Creates a VICS JSON string representation of an array of VICS Althash Record.
    Requires a VICSAltHash array of records and an integer defining number of records as parameters
    Function calling is responsible for freeing memory associated with return value
    Currently Unused

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently size of buffer is set to 65536 bytes

    See Also:
        <VICSAltHash>
        <generateVicsAltHashString>
*/

wchar_t* getAltHashRecordsString(VICSAltHash* records,int number)
{
    wchar_t* retString = new wchar_t[65536];
    retString[0]=L'\0';
    swprintf(retString,L"\"AlternativeHashes@odata.navigationLinkUrl\":\"/Media(%ls)/AlternativeHashes\",\"AlternativeHashes\":[\r\n\t\t\t{\r\n",records[0].MD5);
    for (int i=0;i<number;i++)
    {
        wchar_t* vAltHashtmp = generateVicsAltHashString(records[i]);
        if (vAltHashtmp!= NULL)
        {
                wcscat(retString,vAltHashtmp);
                delete[] vAltHashtmp;
        }
        if (i==number-1)
        {
            wcscat(retString,L"\t\t\t}\r\n");
        }
        else
        {
            wcscat(retString,L"\t\t\t},{\r\n");
        }
    }
    wcscat(retString,L"\t\t],");
    return retString;
}

/*Function: getExifRecordsString
    Creates a VICS JSON string representation of an array of VICS Althash Record.
    Requires a VICSExif array of records and an integer defining number of records as parameters
    Function calling is responsible for freeing memory associated with return value
    Currently Unused

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently size of buffer is set to 65536 bytes

    See Also:
        <VICSExif>
        <generateVicsEXIFString>
*/

wchar_t* getExifRecordsString(VICSExif* records,int number)
{
    wchar_t* retString = new wchar_t[65536];
    retString[0]=L'\0';
    swprintf(retString,L"\"Exifs@odata.navigationLinkUrl\":\"/Media(%ls)/Exifs\",\"Exifs\":[\r\n\t\t\t{\r\n",records[0].MD5);
    for (int i=0;i<number;i++)
    {
        wchar_t* vExiftmp = generateVicsEXIFString(records[i]);
        if (vExiftmp!= NULL)
        {
                wcscat(retString,vExiftmp);
                delete[] vExiftmp;
        }
        if (i==number-1)
        {
            wcscat(retString,L"\t\t\t}\r\n");
        }
        else
        {
            wcscat(retString,L"\t\t\t},{\r\n");
        }
    }
    wcscat(retString,L"\t\t],");
    return retString;
}

/*Function: getMediaRecordString
    Creates a VICS JSON string representation of an array of VICS MediaFile Record.
    Requires a VICSMediaFile array of records and an integer defining number of records as parameters
    Function calling is responsible for freeing memory associated with return value

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently size of buffer is set to 65536 to start with bytes

    See Also:
        <VICSMediaFile>
        <generateVicsMediaFileString>
*/

wchar_t* getMediaRecordString(VICSMediaFile* records, int number)
{
    INT64 bufferSize = 65536;
    wchar_t* vMediaFiletxt = new wchar_t[bufferSize];
    vMediaFiletxt[0]=L'\0';
    INT64 recordLength;
    //swprintf(vMediaFiletxt,L"\"MediaFiles@odata.navigationLinkUrl\":\"/Media(%ls)/MediaFiles\",\"MediaFiles\":[\r\n\t\t\t{\r\n",records[0].MD5);
    swprintf(vMediaFiletxt,L"\r\t\t\"MediaFiles\":[\r",records[0].MD5);
    for (int i=0;i<number;i++)
    {
        recordLength = wcsnlen(vMediaFiletxt,bufferSize);
        wchar_t* vMediaFiletmp = generateVicsMediaFileString(&records[i]);
        if (vMediaFiletmp!= NULL)
        {
            int length = wcslen(vMediaFiletmp);
            if (length + recordLength > bufferSize - 128)
            {
                vMediaFiletxt = extendBuffer(vMediaFiletxt,bufferSize, bufferSize*2);
                bufferSize = bufferSize * 2;
            }
            wcsncat(vMediaFiletxt,vMediaFiletmp,bufferSize);
            delete[] vMediaFiletmp;
        }
        //if last one, don#t add comma
        if (i!=number-1)
        {
            wcsncat(vMediaFiletxt,L",",bufferSize);
        }
    }
    //if mediafile records, close it
    if (number!=0)
    {
        wcsncat(vMediaFiletxt,L"]",bufferSize);
    }
    return vMediaFiletxt;
}


/*Function: getMediaMetadataString
    Creates a VICS JSON string representation of an array of VICS MediaMetadata Record.
    Requires a VICSMediaMetadata array of records and an integer defining number of records as parameters
    Function calling is responsible for freeing memory associated with return value

    Added in 1.41 to allow for additional metadata - Currently Unused

    Todo: Add a function that figures the buffer size rather than allocating large amount.

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently size of buffer is set to 65536 bytes

    See Also:
        <VICSMediaMetadata>

        <generateVicsMediaMetadataString>
*/

wchar_t* getMediaMetadataString(VICSMediaMetadata* records, int number)
{
    wchar_t* vMetadatatxt = new wchar_t[65536];
    //need to include

    vMetadatatxt[0]=L'\0';
    swprintf(vMetadatatxt,L"\"MediaMetadata\":[{\r\n");
    for (int i=0;i<number;i++)
    {
        wchar_t* vMetadatatmp = generateVicsMediaMetadataString(records[i]);
        if (vMetadatatmp!= NULL)
        {
                wcscat(vMetadatatxt,vMetadatatmp);
                delete[] vMetadatatmp;
        }
        if (i==number-1)
        {
            wcscat(vMetadatatxt,L"}");
        }
        else
        {
            wcscat(vMetadatatxt,L"},{\r\n");
        }
    }
    if (number!=0)
    {
        wcscat(vMetadatatxt,L"]");
    }
    return vMetadatatxt;
}

/*Function: getSegmentRecordString
    Creates a VICS JSON string representation of an array of VICS Segment Record.
    Requires a VICSSegment array of records and an integer defining number of records as parameters
    Function calling is responsible for freeing memory associated with return value
    Currently Unused

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently size of buffer is set to 65536 bytes

    See Also:
        <VICSSegment>
        <generateVicsSegmentString>
*/

wchar_t* getSegmentRecordString(VICSSegment* records, int number)
{
    wchar_t* vSegmenttxt = new wchar_t[65536];
    vSegmenttxt[0]=L'\0';
    swprintf(vSegmenttxt,L"\"Segments@odata.navigationLinkUrl\":\"/Media(%ls)/Segments\",\"Segments\":[\r\n\t\t\t{\r\n",records[0].MD5);
    for (int i=0;i<number;i++)
    {
        wchar_t* vSegmenttmp = generateVicsSegmentString(records[i]);
        if (vSegmenttmp!= NULL)
        {
                wcscat(vSegmenttxt,vSegmenttmp);
                delete[] vSegmenttmp;
        }
        if (i==number-1)
        {
            wcscat(vSegmenttxt,L"\t\t\t}\r\n");
        }
        else
        {
            wcscat(vSegmenttxt,L"\t\t\t},{\r\n");
        }
    }
    if (number!=0)
    {
        wcscat(vSegmenttxt,L"\t\t],");
    }
    return vSegmenttxt;
}

/*Function: getRepositoryRecordString
    Creates a VICS JSON string representation of an array of VICS Repository Record.
    Requires a VICSRepository array of records and an integer defining number of records as parameters
    Function calling is responsible for freeing memory associated with return value
    Currently Unused

    Returns:
        wchar_t* that points to a NULL terminated string.
        Currently size of buffer is set to 65536 bytes

    See Also:
        <VICSRepository>
        <generateVicsRepositoryString>
*/

wchar_t* getRepositoryRecordString(VICSRepository* records, int number)
{
    wchar_t* vRepositorytxt = new wchar_t[65536];
    vRepositorytxt[0]=L'\0';
    swprintf(vRepositorytxt,L"\"Repositories@odata.navigationLinkUrl\":\"/Media(%ls)/Repositories\",\"Repositories\":[\r\n\t\t\t{\r\n",records[0].MD5);
        for (int i=0;i<number;i++)
    {
        wchar_t* vRepositorytmp = generateVicsRepositoryString(records[i]);
        if (vRepositorytmp!= NULL)
        {
                wcscat(vRepositorytxt,vRepositorytmp);
                delete[] vRepositorytmp;
        }
        if (i==number-1)
        {
            wcscat(vRepositorytxt,L"\t\t\t}\r\n");
        }
        else
        {
            wcscat(vRepositorytxt,L"\t\t\t},{\r\n");
        }
    }
    //if mediafile records, close it
    if (number!=0)
    {
        wcscat(vRepositorytxt,L"\t\t]\r\n\t\t,");
    }
    return vRepositorytxt;
}

/*Section: VICS String Validation Functions*/

/*Function: containsInvalidChar
    Checks a NULL terminated wchar_t* string for following characters:
    "
    \ (un-escaped)
    {tab} - Added 1.381

    Returns:
        true - string contains invalid characters
        false - string contains no invalid characters

*/

BOOL containsInvalidChar(wchar_t* text)
{
    int length = wcslen(text);
    for (int i =0;i< length;i++)
    {
        if (text[i] == L'\"')
        {
            return true;
        }
        if (text[i] == L'\\')
        {
            if (i==0)
            {
                if (text[i+1] != L'\\')
                {
                    return true;
                }
            }
            else if (text[i-1] != L'\\' && text[i+1] != L'\\')
            {
                return true;
            }
        }
        //1.381 add tab character - 1.382 changed to \t rather than hex
        if (text[i] == L'\t')
        {
            return true;
        }
    }
    return false;
}

/*Function: checkJsonText
    Checks a NULL terminated wchar_t* string for following characters:
    "
    \ (un-escaped)
    {tab} - Added 1.381
    and escapes the characters if they exist.

    Returns:
        wchar_t* string with all special characters escaped

    See also:
        <containsInvalidChar>
*/

wchar_t* checkJsonText(wchar_t* textIn)
{
    BOOL chk = containsInvalidChar(textIn);
    if (!chk)
    {
        //no invalid chars
        return textIn;
    }
    //else we need to do something
    wchar_t* tempStr;
    int inLen = wcslen(textIn);
    int j = 0, newStrLen;
    //make new string larger to accomodate lots of
    newStrLen = inLen + 200;
    tempStr = new wchar_t[newStrLen];
    //set buffer to \0's
    memset(tempStr,'\0', sizeof(wchar_t)*newStrLen);
    for (int i = 0;i<inLen;i++)
    {
        //need to check that current character is " and previous character is not a backslash
        //also need to check that previous 2 chars are not backslash
        //****needs further work*** currently doesn't deal with multiple backslashes well i.e. "\\\\"
        if (textIn[i]==L'\"')
        {
            if ((textIn[i-1] != L'\\') || (textIn[i-1] == L'\\' && textIn[i-2] == L'\\') )
            {
                tempStr[j] = L'\\';
                j++;
                tempStr[j] = L'\"';
            }
        }
        //if not a speech mark, might be a single backslash
        else if (textIn[i] ==L'\\')
        {
            if (i>0)
            {
                if (textIn[i+1] !=L'\\')
                {
                    tempStr[j] = L'\\';
                    j++;
                    tempStr[j] = L'\\';
                }
            }
            else if (textIn[i-1] !=L'\\' && textIn[i+1] !=L'\\')
            {
                tempStr[j] = L'\\';
                j++;
                tempStr[j] = L'\\';
            }
        }
        //1.381 add tab char
        else if (textIn[i] == L'\t')
        {
            if ((textIn[i-1] != L'\\') || (textIn[i-1] == L'\\' && textIn[i-2] == L'\\') )
            {
                tempStr[j] = L'\\';
                j++;
                tempStr[j] = L't';
            }
        }
        else
        {
            tempStr[j] = textIn[i];
        }
        j++;
    }
    tempStr[j] = L'\0';
    //delete original memory
    delete[] textIn;
    return tempStr;
}

/*Section: VICS SQL Extraction Functions*/

/*Function: extractVICSMediaSQL
    Functions takes a pointer a VICSMedia record and a sqlite3_stmt
    Fills in the details from the SQL results into the VICSMedia record


    See also:
        <VICSMedia>
*/

void extractVICSMediaSQL(VICSMedia &recMedia,sqlite3_stmt* statement)
{
    recMedia.MediaID = sqlite3_column_int64(statement,0);
    recMedia.Category = sqlite3_column_int(statement,1);
    int CheckSize = sqlite3_column_bytes16(statement,2);
    if (CheckSize < 10)
    {
        //no hash
        recMedia.SHA1[0] = L'\0';
    }
    else
    {
        wcscpy(recMedia.SHA1, (wchar_t*)sqlite3_column_text16(statement,2));
    }
    //MD5 hash to exist
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
    //path has to exist
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

    See also:
        <VICSMediaFile>

*/

void extractVICSMediaFileSQL(VICSMediaFile &recMediaFile,sqlite3_stmt* statement)
{
    //first 3 mandatory
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
    //unallocated
    recMediaFile.unallocated = sqlite3_column_int(statement,6);
    //sourceID
    CheckSize = sqlite3_column_bytes16(statement,7);
    recMediaFile.sourceID =  new wchar_t[CheckSize + 2];
    wcscpy(recMediaFile.sourceID, (wchar_t*)sqlite3_column_text16(statement,7));
    //Physical Location
    recMediaFile.physicalLocation = sqlite3_column_int64(statement,8);
    //deleted
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
    if (CheckSize == 0) { recMediaFile.parentName = NULL; }
    else {
            recMediaFile.parentName = new wchar_t[CheckSize + 2];
            wcscpy(recMediaFile.parentName, (wchar_t*)sqlite3_column_text16(statement,12));
        }
    //Parent Physical Location
    recMediaFile.parentPhysLoc = sqlite3_column_int(statement,13);
}

/*Function: extractVICSMediaMetadataSQL
    Functions takes a pointer a VICSMediaMetadata record and a sqlite3_stmt
    Fills in the details from the SQL results into the VICSMediaMetadata record

    See also:
        <VICSMediaMetadata>

*/
void extractVICSMediaMetadataSQL(VICSMediaMetadata* record,sqlite3_stmt* statement)
{
    //MD5
    wcscpy(record->MD5, (wchar_t*)sqlite3_column_text16(statement,0));
    //Property name
    int CheckSize = sqlite3_column_bytes16(statement,1);
    record->PropertyName =  new wchar_t[CheckSize + 2];
    wcscpy(record->PropertyName, (wchar_t*)sqlite3_column_text16(statement,1));
    //Property Value
    CheckSize = sqlite3_column_bytes16(statement,2);
    record->PropertyValue =  new wchar_t[CheckSize + 2];
    wcscpy(record->PropertyValue, (wchar_t*)sqlite3_column_text16(statement,2));
}

/*Function: createVICSstring
    Takes and SQLite database pointer to the database containing VICS records
    Also takes a pointer to a VICSMedia record, 2 flag indicators; picture and first

    This function needs re-writing, possibly along with the two associated functions
    Called from writeRecords - may also need re-writing

    It appears all the component functions are there to replace this function such as
    the generateVics* functions and get*RecordsString functions.

    Possible that all 3 of these functions need moving to the SQLFunctions side

    Returns a wchar_t* that contains a JSON VICS string (NULL terminated) of a Media record, with its media file records.

    See also:
        <extractVICSMediaFileSQL>
        <extractVICSMediaSQL>
        <writeRecords>

*/

wchar_t* createVICSstring(sqlite3* vicsDB,VICSMedia &record, int picture, int first)
{
    VICSMediaFile recMediaFile;
    InitializeMediaFileRecord(recMediaFile);
    INT64 bufferSize = 5242880;
    wchar_t* strMedia = generateVicsMediaString(record);
    wchar_t* output = NULL, *vMediaFiletxt;
    vMediaFiletxt = new wchar_t[bufferSize];
    if (vMediaFiletxt == NULL)
    {
        wchar_t errorMsg[2048];
        errorMsg[0] = L'\0';
        swprintf(errorMsg, L"Null filename for itemID: %llu",record.MediaID);
        XWF_OutputMessage(errorMsg,0);
        return NULL;
    }
    sqlite3_stmt *statement;
    wchar_t tableName[20] = {0}, sqlQuery[128]= {0};
    if (picture == 1)
    {wcscpy(tableName, L"VICSPicsRecords");}
    else{wcscpy(tableName,L"VICSMoviesRecords");}
    swprintf(sqlQuery, L"Select * from %ls where MD5Hash = \'%ls\';",tableName,record.MD5);
    int rc = sqlite3_prepare16_v2(vicsDB,sqlQuery,(wcslen(sqlQuery)+1)*sizeof(wchar_t),&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        int firstMedia = 1;
        if (rc == SQLITE_ROW)
        {
            vMediaFiletxt[0]=L'\0';
            if (first == 1)
            {
                //swprintf(vMediaFiletxt,L"\t\t\t\"odata.id\":\"Media(\\\"%llu\\\")\",\"MediaFiles@odata.navigationLinkUrl\":\"/Media(%ls)/MediaFiles\",\"MediaFiles\":[\r\n\t\t\t{\r\n",record.MediaID,record.MD5);
                swprintf(vMediaFiletxt,L"\t\t\t\"MediaFiles\":[\r\n\t\t\t{");
            }
            else
            {
                //swprintf(vMediaFiletxt,L"{\r\n\t\t\t\"odata.id\":\"Media(\\\"%llu\\\")\",\"MediaFiles@odata.navigationLinkUrl\":\"/Media(%ls)/MediaFiles\",\"MediaFiles\":[\r\n\t\t\t{\r\n",record.MediaID,record.MD5);
                swprintf(vMediaFiletxt,L"{\r\n\t\t\t\"MediaFiles\":[\r\n\t\t\t{");
            }
            //data here
            do
            {
                //extract VICS Media
                if (firstMedia == 1) {
                        //so we don't get multiple recMediaFile entries!
                        firstMedia = 0;
                }
                else{
                        wcscat(vMediaFiletxt,L"},{\r\n");
                }
                extractVICSMediaFileSQL(recMediaFile,statement);
                wchar_t* vMediaFiletmp = generateVicsMediaFileString(&recMediaFile);
                if (vMediaFiletmp!= NULL)
                {
                        wcscat(vMediaFiletxt,vMediaFiletmp);
                        INT64 currSize = wcslen(vMediaFiletxt);
                        if (currSize > ((bufferSize/4)*3))
                        {
                            //buffer 3/4 full
                            //XWF_OutputMessage(vMediaFiletxt,0);
                            bufferSize = bufferSize * 2;
                            wchar_t* tempStr = new wchar_t[bufferSize];
                            memcpy(tempStr,vMediaFiletxt, (currSize*sizeof(wchar_t)));
                            delete[] vMediaFiletxt;
                            vMediaFiletxt = tempStr;
                        }
                        delete[] vMediaFiletmp;
                }
                rc = sqlite3_step(statement);
                deallocateMediaFileRecord(recMediaFile);
            }
            while (rc == SQLITE_ROW);
            wcscat(vMediaFiletxt,L"}\r\n\t\t\t],");
            long sizeOutput = wcslen(vMediaFiletxt);
            output = new wchar_t[sizeOutput + wcslen(strMedia)+128];
            swprintf(output,L"%ls%ls\r\n\t\t\t",vMediaFiletxt,strMedia);
            sqlite3_finalize(statement);
        }
        else
        {
            //no records....
            sqlite3_finalize(statement);
            XWF_OutputMessage(L"No record located",0);
			delete[] vMediaFiletxt;
            return NULL;
        }
    }
    delete[] strMedia;
	delete[] vMediaFiletxt;
    return output;
}

/*  Section: VICS Validation Functions  */

/*Function: validFiletime
    Function that validates the FILETIME prior to producing.

    Uses minTime variable to find lowest timestamp

    Added in 1.50

    Parameters:
        FILETIME timestamp  - FILETIME to be validated

    Returns:
        true if valid FILETIME, false otherwise

    See Also:
        Called by           - <generateVicsMediaFileString>

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
    //if timestamp is far into future (year 2222 used below) return false.
    //1.52 clearly this was a shit way to do it! Create a system time, add 2 years and check its less than that
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
