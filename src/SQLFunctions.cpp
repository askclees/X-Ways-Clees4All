#include <cwchar>
#include <cstdio>
#include <wchar.h>
#include "sqlite3.h"
#include "X-Tension.h"
#include "main.h"
#include "SQLFunctions.h"
#include "debugMessage.h"
#include "utility.h"
#include "options.h"


//prototyping

int setupVics(sqlite3** sqlDB);
INT64 getVicsRecord(sqlite3* sqlDB, wchar_t* MD5, int picture);
int insertEvObjRecord(sqlite3* sqlDB, EvidenceProps &record);
void createSQLNameList(sqlite3* sqlDB, HANDLE evObj);
int updateEvObjFILE(sqlite3* sqlDB, DWORD evObj, int fileNo);
DWORD getEvObjParent(sqlite3* sqlDB, DWORD evObj);
ObjectNames* retrieveEvidenceNames(sqlite3* sqlDB, int* retCounter);
void setParentSelected(sqlite3* sqlDB, DWORD parentID);
BOOL checkParentSelected(sqlite3* sqlDB, DWORD parentID);
int updateFileNumber(sqlite3* sqlDB,DWORD objID,int fileNo);
int getFileNumber(sqlite3* sqlDB,DWORD objID);
DWORD getRootObj(sqlite3* sqlDB,DWORD objID);
int recordError(sqlite3* sqlDB,int errorCode, LONG objID, LPWSTR srcText);
int sqlCreateOptions(char path[]);
BOOL SQLDatabaseExists(char path[]);
ExtractOptions loadOptions(char path[]);
int saveOptions(char path[], ExtractOptions opt);
void outputErrorStats(sqlite3* sqlDB,WORD versionNo);

const int optionsSchemaVersion = 2;

//1.50 defined points in options table to make updating easier
#define OPTION_GRIFFEYE               10
#define OPTION_EXTRACT_START          11

/*database: VICS Database
    Main database used to store file information for processing into project VICS records.

    See Also:
        <AlternativeHash>

        <EvidenceObjects>

        <MediaMetadata>

        <VICSMovies>

        <VICSMoviesRecords>

        <VICSPics>

        <VICSPicsRecords>

*/

/*Table: VICSMovies

    This table contains data about Video media exported from X-Ways. Relates to the Project VICS Media entry.

    Tables are split between pictures and videos for ease of export

         MediaID - INTEGER - Integer field that uniquely identifies media file
         Category - INTEGER - Integer field that relates to Project VICS category
         SHA1 - TEXT - SHA1 hash value of file
         MD5Hash - TEXT PRIMARY KEY - MD5 Hash value of file
         VictimID - INT - Flag for VictimID being set
         OffenderID - INT - Flag for OffenderID being set
         IsDistributed - INT - Flag for IsDistributed being set
         Comments - TEXT - TEXT field containing comments data - Currently unused
         Tags - TEXT - TEXT field containing tags, separated by commas - Currently unused
         Series - TEXT - TEXT field containing series information - Currently unused
         MediaSize - INT - Size of file in bytes
         RelativeFilePath - TEXT - Contains relative file path of the exported file on local machine. Not same as path in evidence
         DateUpdated - INT - Date record was updated - Unsure if used
         Timezone - INT - Timezone information - Not required in VICS data. May be removed in future
         PreCatSource - TEXT -  Source that categorisation information came from - Currently Unused.
         IsSuspected - INT - Flag to be used for images that should be reviewed. Currently unused
         MimeType - TEXT - String that contains the type of file i.e. image/JPEG
         PhotoDNA - TEXT - String containin the PhotoDNA hash in base64 format (standard).

*/
/*database Table: VICSPics

    This table contains data about Picture media exported from X-Ways. Relates to the Project VICS Media entry

    Tables are split between pictures and videos for ease of export

         MediaID - INTEGER - Integer field that uniquely identifies media file
         Category - INTEGER - Integer field that relates to Project VICS category
         SHA1 - TEXT - SHA1 hash value of file
         MD5Hash - TEXT PRIMARY KEY - MD5 Hash value of file
         VictimID - INT - Flag for VictimID being set
         OffenderID - INT - Flag for OffenderID being set
         IsDistributed - INT - Flag for IsDistributed being set
         Comments - TEXT - TEXT field containing comments data - Currently unused
         Tags - TEXT - TEXT field containing tags, separated by commas - Currently unused
         Series - TEXT - TEXT field containing series information - Currently unused
         MediaSize - INT - Size of file in bytes
         RelativeFilePath - TEXT - Contains relative file path of the exported file on local machine. Not same as path in evidence
         DateUpdated - INT - Date record was updated - Unsure if used
         Timezone - INT - Timezone information - Not required in VICS data. May be removed in future
         PreCatSource - TEXT -  Source that categorisation information came from - Currently Unused.
         IsSuspected - INT - Flag to be used for images that should be reviewed. Currently unused
         MimeType - TEXT - String that contains the type of file i.e. image/JPEG
         PhotoDNA - TEXT - String containin the PhotoDNA hash in base64 format (standard).
*/

/*database Table: VICSMoviesRecords

    Table that stores instances of Video files extracted from X-Ways

    This relates to the VICS Mediafile record

     MD5Hash - TEXT - MD5 of file. Links to VICSMovies table.
     Filename - TEXT - Filename of file as it was on evidence object
     FilePath - TEXT - File path as it was on evidence. Does not include filename
     Created - INT - Timestamp of recorded Created time
     Modified - INT - Timestamp of recorded Modified time
     Accessed - INT - Timestamp of recorded Accessed time
     Unallocated - INT - Flag to state if file was recovered from Free space. This being set will also have deleted set
     SourceID - TEXT - String that contains the Evidence Object name (as ammended by user if applicable)
     PhysicalLocation - INT - Number of bytes from start of media to Media file
     Deleted - INT - Flag to state if file was recorded as being deleted
     ParentMD5 - TEXT - Contains MD5 hash of parent object
     ParentPath - TEXT - String containing path of parent object, as per location on evidence
     parentPhysLoc - INT - Integer showing offset in bytes from start of evidence object to parent file.

*/

/*database Table: VICSPicsRecords

    Table that stores instances of Picture files extracted from X-Ways

    This relates to the VICS Mediafile record

     MD5Hash - TEXT - MD5 of file. Links to VICSMovies table.
     Filename - TEXT - Filename of file as it was on evidence object
     FilePath - TEXT - File path as it was on evidence. Does not include filename
     Created - INT - Timestamp of recorded Created time
     Modified - INT - Timestamp of recorded Modified time
     Accessed - INT - Timestamp of recorded Accessed time
     Unallocated - INT - Flag to state if file was recovered from Free space. This being set will also have deleted set
     SourceID - TEXT - String that contains the Evidence Object name (as ammended by user if applicable)
     PhysicalLocation - INT - Number of bytes from start of media to Media file
     Deleted - INT - Flag to state if file was recorded as being deleted
     ParentMD5 - TEXT - Contains MD5 hash of parent object
     ParentPath - TEXT - String containing path of parent object, as per location on evidence
     parentPhysLoc - INT - Integer showing offset in bytes from start of evidence object to parent file.

*/

/*database Table: EvidenceObjects

    Table that retains details of Evidence objects in case

     ID - INT - Primary Key
     Name -  TEXT - Field containing Name of evidence object (as displayed in X-Ways?)
     SourceID - TEXT - Field containing Name of evidence object (as decided by user? If so, may match Name)
     ParentID - INT - ID of parent Evidence object, 0 if no parent
     FileID - INT - This field denotes which XML output file is in use for this object.
     selected - INT - Flag for whether this evidence item was selected for processing
*/

/*database Table: VICSError

    Designed to store error messages. Unclear as to whether this is used.

     ErrorType - TEXT - Text describing error
     FileID - INT - FileID of file causing error
     FileName - TEXT - Filename of file causing error
     FilePath - TEXT - Path of file causing error

*/

/*database Table: MediaMetadata

    Used to store additional media metatdata, Implements VICS MEDIAMETADATA entry

    Primary key is MD5Hash and PropertyName Columns combined

     MD5Hash - TEXT -   MD5 hash value that links to VICS base record
     PropertyName - TEXT -  TEXT that contains the title of the property being stored
     PropertyValue - TEXT - TEXT that contains the data of the property

*/

/*database Table: AlternativeHash

    Used to store additional hash types. Implements VICS ALTERNATIVEHASH entry. Currently unused.

    Under version 1.3 of VICS, this would be how PhotoDNA would be stored. It was added to MediaFile in version 2.0.

     MD5Hash - TEXT - MD5 Hash that links to MediaFile Record
     PropertyName - TEXT - This would be hash type i.e. SHA256/EDK
     PropertyValue - TEXT - Hash value of type in Property Name

*/


const char* sqlCreateLastRun = "CREATE TABLE lastSettings (PicPath TEXT, VidPath TEXT, extractPics INT, extractVids INT,  ignoreCarvedWithin INT,"
                          "ignoreThumbs INT,exceptMismatch INT, exportReportTables INT, debug INT,createGriffeye INT,exportVICS INT, exportXML INT, exportCompressed INT);";


//start of functions

/*Section: Functions */


/*Function: errorLogCallback
    Callback procedure for any SQL errors encountered.

    Outputs message with error code to X-Ways messages window

    Called from <SQLInit>

    See Also:
        <SQLInit>
*/

static void errorLogCallback(void *, int iErrCode, const char *zMsg)
{
    wchar_t errMsg[8192];
    errMsg[0]=L'\0';
    swprintf(errMsg, 8192, L"SQLite Error code: %d\nError Msg %s",iErrCode,zMsg);
    XWF_OutputMessage(errMsg,0);
}

/*Function: SQLInit
    Initializes SQLite and generates and error log callback procedure

    Called from <XT_Prepare>

    Returns:
        -1 - Function Failed
        0  - Function Completed Successfully

    See Also:
        <errorLogCallback>

        <XT_Prepare>
*/

int SQLInit()
{
    //setup SQLite Config Options
    int checkLog = sqlite3_config(SQLITE_CONFIG_SERIALIZED);
    if (checkLog != SQLITE_OK)
    {
        wchar_t temp[256]={0};
        swprintf(temp,L"Cannot set serialized sql, error code: %i", checkLog);
        XWF_OutputMessage(temp,0);
        return -1;
    }
    checkLog = sqlite3_config(SQLITE_CONFIG_LOG,errorLogCallback, NULL);
    if (checkLog != SQLITE_OK)
    {
        wchar_t temp[256]={0};
        swprintf(temp,L"Cannot setup SQLite logging, error code: %i", checkLog);
        XWF_OutputMessage(temp,0);
        return -1;
    }
    return 0;
}

/*Function: setupVics
    Function that creates an in-memory SQLite database, called from XT_Prepare

    Creates the Tables and Indexes for the VICS data to be stored in

    Quite a long function and String constants could be stored elsewhere as they are very long!

    Returns:
        0  - Function Completed Successfully
        -1 - Could not create the database

    See Also:
        <XT_Prepare>
*/
int setupVics(sqlite3** sqlDB)
{
    void* unused;
    int rc = sqlite3_open_v2(":memory:",sqlDB,SQLITE_OPEN_FULLMUTEX|SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK)
    {
        int extError = sqlite3_extended_errcode(*sqlDB);
        wchar_t* errormsg = (wchar_t*) sqlite3_errmsg16(*sqlDB);
        XWF_OutputMessage(errormsg,0);
        XWF_OutputMessage(L"Cannot create VICS Sqlite database, exiting",0);
        return -1;
    }
    char* errMsg = 0;
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE VICSMovies (MediaID INT,Category INT,SHA1 TEXT,MD5Hash TEXT PRIMARY KEY NOT NULL, VictimID INT DEFAULT NULL, OffenderID INT DEFAULT NULL,IsDistributed INT DEFAULT NULL,  Comments TEXT DEFAULT \'\', Tags TEXT DEFAULT \'\',Series TEXT DEFAULT \'\', MediaSize INT,  RelativeFilePath TEXT DEFAULT \'\',DateUpdated INT, Timezone INT, PreCatSource TEXT, IsSuspected INT, MimeType TEXT DEFAULT \'\',PhotoDNA TEXT DEFAULT\'\')", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
    }
    sqlite3_free(errMsg);
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE VICSPics (MediaID INT,Category INT,SHA1 TEXT,MD5Hash TEXT PRIMARY KEY NOT NULL, VictimID INT DEFAULT NULL, OffenderID INT DEFAULT NULL,IsDistributed INT DEFAULT NULL,  Comments TEXT DEFAULT \'\', Tags TEXT DEFAULT \'\',Series TEXT DEFAULT \'\', MediaSize INT,  RelativeFilePath TEXT DEFAULT \'\',DateUpdated INT, Timezone INT, PreCatSource TEXT, IsSuspected INT, MimeType TEXT DEFAULT \'\',PhotoDNA TEXT DEFAULT\'\')", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
    }
    sqlite3_free(errMsg);
    //creation of new tables for records
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE VICSPicsRecords (MD5Hash TEXT NOT NULL, FileName TEXT NOT NULL,FilePath TEXT NOT NULL,Created INT,Modified INT,Accessed INT,Unallocated INT,SourceID TEXT,PhysicalLocation INT,Deleted INT,parentMD5 TEXT,parentName TEXT,parentPath TEXT,parentPhysLoc INT, itemID INT)", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
    }
    sqlite3_free(errMsg);
    //creation of new tables for records
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE VICSError (ErrorType TEXT NOT NULL, FileID INT, FileName TEXT NOT NULL,FilePath TEXT NOT NULL)", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
    }
    sqlite3_free(errMsg);
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE VICSMoviesRecords (MD5Hash TEXT NOT NULL, FileName TEXT NOT NULL,FilePath TEXT NOT NULL,Created INT,Modified INT,Accessed INT,Unallocated INT,SourceID TEXT,PhysicalLocation INT,Deleted INT,parentMD5 TEXT,parentName TEXT,parentPath TEXT,parentPhysLoc INT, itemID INT)", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
    }
    sqlite3_free(errMsg);
    //add indexes for speed!!!
    rc = sqlite3_exec(*sqlDB,"CREATE INDEX IndexPics on VICSPics(MD5Hash);", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
    }
    sqlite3_free(errMsg);
    rc = sqlite3_exec(*sqlDB,"CREATE INDEX IndexPicsRecords on VICSPicsRecords(MD5Hash);", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
    }
    sqlite3_free(errMsg);

    //table for evidence objects
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE EvidenceObjects (ID INT PRIMARY KEY NOT NULL,Name TEXT NOT NULL DEFAULT \'\', SourceID TEXT NOT NULL DEFAULT \'\', ParentID INT NOT NULL, FileID INT, selected INT DEFAULT 0)", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
    }
    sqlite3_free(errMsg);

    //table for alternative hashes
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE AlternativeHash (ID INT PRIMARY KEY NOT NULL,MD5Hash TEXT NOT NULL DEFAULT \'\', HashType TEXT NOT NULL DEFAULT \'\', HashValue TEXT NOT NULL DEFAULT \'\')", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
    }
    sqlite3_free(errMsg);

    //1.41 table for mediametadata
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE MediaMetadata (MD5Hash TEXT NOT NULL, PropertyName TEXT NOT NULL DEFAULT \'\', PropertyValue TEXT NOT NULL DEFAULT \'\', PRIMARY KEY (MD5Hash, PropertyName));", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
    }
    sqlite3_free(errMsg);
    return 0;
}

/*Function: createSQLNameList

    Function to create an SQLite record for each evidence object in case

    Records inserted by insertEvObjRecord function

    Called by createNameList, a seemingly pointless function

    See Also:
        <createNameList>

        <insertEvObjRecord>
*/

void createSQLNameList(sqlite3* sqlDB, HANDLE evObj)
{
    EvidenceProps record;
    record.ID = XWF_GetEvObjProp(evObj,1,NULL);
    record.parentID = XWF_GetEvObjProp(evObj,2,NULL);
    INT64 evObjLength;
    INT64 flagCheck = XWF_GetEvObjProp(evObj,18,NULL);
    if (!(flagCheck & 0x08))
    {
        record.selected = 0;
    }
    else
    {
        record.selected = 1;
    }
    wchar_t* buffer = new wchar_t[2048];
    evObjLength = XWF_GetEvObjProp(evObj,7,buffer);
    record.Name = new wchar_t[evObjLength+1];
    wcscpy(record.Name,buffer);
    record.SourceID = new wchar_t[evObjLength+1];
    wcscpy(record.SourceID,buffer);
    record.fileID = 0;
    int result = insertEvObjRecord(sqlDB, record);
    if (result !=0)
    {
        XWF_OutputMessage(L"Error adding evidence object to list",0);
    }
    delete[] buffer;
}

/*Function: checkParentObjectsSelected

    Function to check each selected evidence object and determine its parent.

    Used to ensure that sub objects are associated with top level object even if top level object is not selected.

    I.e. AB-1, Partition 2 is selected, but AB-1 itself is not. Need to show AB-1 as selected for reporting.

    See Also:
        <checkParentSelected>

        <setParentSelected>
*/

void checkParentObjectsSelected(sqlite3* sqlDB)
{
    int rc = 0;
    sqlite3_stmt *statement;
    char sqlQuery[1024]={0};
    //get all items which are selected and are not root objects
    sprintf(sqlQuery,"Select ParentID from EvidenceObjects where ParentID <> 0 and selected = 1;");
    rc = sqlite3_prepare_v2(sqlDB,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        do
        {
            if (rc == SQLITE_ROW)
            {
                //check if parent is selected
                DWORD parentId = sqlite3_column_int(statement,0);
                BOOL check = checkParentSelected(sqlDB,parentId);
                if (!check)
                {
                    //not set to selected, make it so!
                    setParentSelected(sqlDB, parentId);
                }
                rc = sqlite3_step(statement);
            }
        } while (rc == SQLITE_ROW);
     }
     sqlite3_finalize(statement);
}

/*Function: checkParentObjectsSelected

    Function to check the SQLite records to see if an evidence object is selected

    Called from <checkParentObjectsSelected>

    Return:
        false   - evidence object is not selected
        true    - evidence object is selected

    See Also:
        <checkParentObjectsSelected>

*/

BOOL checkParentSelected(sqlite3* sqlDB, DWORD parentID)
{
    int rc = 0;
    sqlite3_stmt *statement;
    char sqlQuery[1024]={0};
    //get all items which are selected and are not root objects
    sprintf(sqlQuery,"Select selected from EvidenceObjects where ID = %lu;",parentID);
    rc = sqlite3_prepare_v2(sqlDB,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        sqlite3_step(statement);
        int result = sqlite3_column_int(statement,0);
        if (result == 0)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        //error
    }
    return false;
}

/*Function: setParentSelected

    Function to set SQLite record to selected for given parent ID

    Currently no return value for showing an error occurred.

    Called from <checkParentObjectsSelected>

    See Also:
        <checkParentObjectsSelected>

*/

void setParentSelected(sqlite3* sqlDB, DWORD parentID)
{
    int rc = 0;
    sqlite3_stmt *statement;
    char sqlQuery[1024]={0};
    //get all items which are selected and are not root objects
    sprintf(sqlQuery,"Update EvidenceObjects set selected = 1 where ID = %lu;",parentID);
    rc = sqlite3_prepare_v2(sqlDB,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc != SQLITE_DONE && rc != SQLITE_OK)
        {
            //error

        }

    }
    sqlite3_finalize(statement);
}

/*Function: insertEvObjRecord

    Function to insert a SQLite record relating to an evidence object

    Called from <checkParentObjectsSelected>

    Possible memory leak if an error occurs.

    Return:
        0   - Success
        -1  - Error Preparing Query
        -2  - Error Executing Query

    See Also:
        <checkParentObjectsSelected>

*/

int insertEvObjRecord(sqlite3* sqlDB, EvidenceProps &record)
{
    wchar_t* sqlQuery;
    sqlQuery = new wchar_t[1024];
    sqlQuery[0] = '\0';
    swprintf(sqlQuery,L"INSERT INTO EvidenceObjects VALUES (%lu, \'%ls\', \'%ls\', %lu, %i, %i)",record.ID, record.Name, record.SourceID,record.parentID, record.fileID, record.selected);
    sqlite3_stmt *statement;
    int rc = sqlite3_prepare16_v2(sqlDB , sqlQuery,(wcslen(sqlQuery)+1)*sizeof(wchar_t),&statement, NULL);
    if (rc != SQLITE_OK)
    {
        //do error stuff here
        XWF_OutputMessage(sqlQuery,0);
        return -1;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE)
    {
        //do error stuff here
        XWF_OutputMessage(sqlQuery,0);
        return -2;
    }
    sqlite3_finalize(statement);
    delete[] sqlQuery;
    return 0;
}

//1.41 - commented out unused function
/*
int updateEvObjFILE(sqlite3* sqlDB, DWORD evObj, int fileNo)
{
    //update the FILE array reference
    wchar_t* sqlQuery;
    sqlQuery = new wchar_t[1024];
    sqlQuery[0] = '\0';
    swprintf(sqlQuery,L"UPDATE EvidenceObjects SET FileID = %i WHERE ID = %lu",fileNo, evObj);
    sqlite3_stmt *statement;
    int rc = sqlite3_prepare16_v2(sqlDB , sqlQuery,(wcslen(sqlQuery)+1)*sizeof(wchar_t),&statement, NULL);
    if (rc != SQLITE_OK)
    {
        wchar_t errMsg[wcslen(sqlQuery)+1024];
        swprintf(errMsg, L"Error preparing SQL Statement to update FileNumber: %ls",sqlQuery);
        XWF_OutputMessage(errMsg,0);
        return -1;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE)
    {
        wchar_t errMsg[wcslen(sqlQuery)+1024];
        swprintf(errMsg, L"Error executing SQL Statement to update FileNumber: %ls",sqlQuery);
        XWF_OutputMessage(errMsg,0);
        return -2;
    }
    sqlite3_finalize(statement);
    delete[] sqlQuery;
    return 0;
}*/

//1.41 - appears to be an unused function
/*
DWORD getEvObjParent(sqlite3* sqlDB, DWORD evObj)
{
    //update the FILE array reference
    wchar_t* sqlQuery;
    sqlQuery = new wchar_t[1024];
    sqlQuery[0] = '\0';
    swprintf(sqlQuery,L"Select ParentID from EvidenceObjects where ID = %lu;", evObj);
    sqlite3_stmt *statement;
    int rc = sqlite3_prepare16_v2(sqlDB , sqlQuery,(wcslen(sqlQuery)+1)*sizeof(wchar_t),&statement, NULL);
    if (rc != SQLITE_OK)
    {
        wchar_t errMsg[wcslen(sqlQuery)+1024];
        swprintf(errMsg, L"Error preparing SQL Statement to locate ParentID: %ls",sqlQuery);
        XWF_OutputMessage(errMsg,0);
        return -1;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE)
    {
        wchar_t errMsg[wcslen(sqlQuery)+1024];
        swprintf(errMsg, L"Error executing SQL Statement to locate ParentID: %ls",sqlQuery);
        XWF_OutputMessage(errMsg,0);
        return -2;
    }
    //get parent id and return
    DWORD parentID = sqlite3_column_int(statement,0);
    sqlite3_finalize(statement);
    delete[] sqlQuery;
    return parentID;
}*/

//1.50
/*Function: checkDuplicateFile

    Function that checks if a file with same offset and hash value exists.

    Returns 1 if exists, 0 if no record exists, negative number for error
*/

int checkDuplicateFile(sqlite3* sqlDB, INT64 offset, wchar_t* MD5, wchar_t* currSrcID, long* nItemID,int picture)
{
    if (extractInfo.debugSet){debugWriteDetails(*nItemID, L"checkDuplicateFile Start");}
    //Select * from VICSPicsRecords where PhysicalLocation = offset and MD5Hash = MD5
    sqlite3_stmt *statement;
    char* tableName;
    if (picture ==1) { tableName = "VICSPicsRecords";}
    else { tableName = "VICSMoviesRecords";}
    char sqlQuery[256]={0};
    sprintf(sqlQuery,"Select itemID from %s where MD5Hash = \'%ls\' and SourceID = \'%ls\' and PhysicalLocation = %llu;",tableName,MD5,currSrcID,offset);
    if (extractInfo.debugSet){debugWriteDetails(sqlQuery);}
    int rc = sqlite3_prepare_v2(sqlDB,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW)
        {
            //data here
            *nItemID = sqlite3_column_int(statement,0);
            sqlite3_finalize(statement);
            if (extractInfo.debugSet){debugWriteDetails(*nItemID, L"checkDuplicateFile End - Duplicate located");}
            return 1;
        }
        else
        {
            //no data
            sqlite3_finalize(statement);
            if (extractInfo.debugSet){debugWriteDetails(*nItemID, L"checkDuplicateFile End - No Duplicate located");}
            return 0;
        }
    }
    else
    {
        if (extractInfo.debugSet){debugWriteDetails(*nItemID, L"checkDuplicateFile END - Error");}
        return -1;
    }
}

/*Function: getVicsRecord

    Function to check if a MD5 hash exists in VICS database.

    Called from <createVICSRecord>

    Return:
        1   - Located entry
        0   - Entry not located
        -1  - Error

    See Also:
        <createVICSRecord>

*/

INT64 getVicsRecord(sqlite3* sqlDB, wchar_t* MD5, int picture)
{
    sqlite3_stmt *statement;
    char sqlQuery[256]={0};
    if (picture == 1)
    {
        sprintf(sqlQuery,"Select MD5Hash from VICSPics where MD5Hash = \'%ls\';",MD5);
    }
    else
    {
        sprintf(sqlQuery,"Select MD5Hash from VICSMovies where MD5Hash = \'%ls\';",MD5);
    }
    int rc = sqlite3_prepare_v2(sqlDB,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW)
        {
            //data here
            sqlite3_finalize(statement);
            return 1;
        }
        else
        {
            sqlite3_finalize(statement);
            return 0;
        }
    }
    else
    {
        return -1;
    }
}

static int countCallback(void *valCount,int argc, char **argv, char **azColName)
{
    int *c = (int*)valCount;
    *c = atoi(argv[0]);
    return 0;
}

/*Section: Evidence Object Functions */

/*Function: retrieveEvidenceNames

    Returns an array of ObjectNames where the parentID is 0 i.e. root object.

    Called from <getCaseOptions>

    Return:
        ObjectNames* containing the details of evidence objects

    See Also:
        <getCaseOptions>

*/

ObjectNames* retrieveEvidenceNames(sqlite3* sqlDB,int *retCounter)
{
    char* zErrMsg = 0;
    int noObjs=0;
    int rc = 0;
    sqlite3_stmt *statement;
    //get count of objects in table
    rc = sqlite3_exec(sqlDB,"Select count(*) from EvidenceObjects where parentID = 0  and selected = 1 ;",countCallback, &noObjs, &zErrMsg);
    //create array of objects
    ObjectNames* retArray = new ObjectNames[noObjs];
    char sqlQuery[1024]={0};
    sprintf(sqlQuery,"Select ID, Name, SourceID from EvidenceObjects where ParentID = 0 and selected = 1;");
    rc = sqlite3_prepare_v2(sqlDB,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        int recCount = 0;
        rc = sqlite3_step(statement);
        do
        {
            if (rc == SQLITE_ROW)
            {
                retArray[recCount].objectID = sqlite3_column_int(statement,0);
                wcscpy(retArray[recCount].actualName, (wchar_t*)sqlite3_column_text16(statement, 1));
                wcscpy(retArray[recCount].prefName, (wchar_t*)sqlite3_column_text16(statement, 2));
                rc = sqlite3_step(statement);
                recCount++;
            }
        } while (rc == SQLITE_ROW);
        *retCounter = recCount;
    }
    sqlite3_finalize(statement);
    return retArray;
}


/*Function: updateEvidenceNames

    Updates evidence objects to use their preferred names.
    Errors output to message window, but no value returned that indicates an error.

    Called from <getCaseOptions>

    Return:
        0   - Always

    See Also:
        <getCaseOptions>

*/

int updateEvidenceNames(sqlite3* sqlDB,ObjectNames* listEvObj, int noObjs)
{
    if (extractInfo.debugSet){debugWriteDetails("Start of updateEvidenceNames Function");}
    //update sourceID in database where it doesn't match original
    for (int i = 0;i<noObjs;i++)
    {
        if (wcscmp(listEvObj[i].actualName,listEvObj[i].prefName)!=0)
        {
            char sqlQuery[2048];
            sqlite3_stmt *statement;
            sprintf(sqlQuery,"Update EvidenceObjects SET sourceID = \'%ls\' where ID = %lu ;",listEvObj[i].prefName,listEvObj[i].objectID);
            int rc = sqlite3_prepare_v2(sqlDB,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
            if (rc == SQLITE_OK)
            {
                rc = sqlite3_step(statement);
                if (rc != SQLITE_DONE && rc != SQLITE_OK)
                {
                    //problem
                    wchar_t errMsg[2048];
                    swprintf(errMsg,L"Error updating source ID for: %lu . SQL Query: %s",listEvObj[i].objectID, sqlQuery);
                    XWF_OutputMessage(errMsg, 0);
                }
            }
            else
            {
                XWF_OutputMessage(L"Error preparing statement for updating Source ID",0);
            }
            sqlite3_finalize(statement);
        }
    }
    if (extractInfo.debugSet){debugWriteDetails("End of updateEvidenceNames Function");}
    return 0;
}

/*Function: getSourceIDName

    Retrieve the stored name for a given evidence object.

    Called from <XT_Prepare> and used to denote current item name

    Return:
        !NULL   - Wide character String with source name
        NULL    - Function failed
    See Also:
        <XT_Prepare>

*/

wchar_t* getSourceIDName(sqlite3* sqlDB, DWORD evID)
{
    char sqlQuery[256]={0};
    sqlite3_stmt *statement;
    sprintf(sqlQuery,"Select SourceID from EvidenceObjects where ID = %lu;", evID);
    int rc = sqlite3_prepare_v2(sqlDB,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW)
        {
            wchar_t* tempResult = (wchar_t*)sqlite3_column_text16(statement,0);
            wchar_t* returnString = new wchar_t[wcslen(tempResult)+2];
            wcscpy(returnString,tempResult);
            return returnString;
        }
    }
    return NULL;
}

/*Function: updateFileNumber

    Updates the XML file number for a given ID.

    Called from <createC4POutput>

    Return:
        -1  -   Error
        0   -   Success

    See Also:
        <createC4POutput>

*/

int updateFileNumber(sqlite3* sqlDB,DWORD objID,int fileNo)
{
    int rc = 0;
    sqlite3_stmt *statement;
    char sqlQuery[1024]={0};
    //Update items to where ID matches parameter
    sprintf(sqlQuery,"Update EvidenceObjects set FileID = %i where ID = %lu;",fileNo,objID);
    rc = sqlite3_prepare_v2(sqlDB,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc != SQLITE_DONE && rc != SQLITE_OK)
        {
            //error
            return -1;
        }

    }
    sqlite3_finalize(statement);
    return 0;
}

/*Function: getFileNumber

    Returns the corresponding XML output file for the selected sourceID.
    Called each time XT_Prepare is called for each new volume and XML output file is then set

    Called from <XT_Prepare>

    Return:
        -1  -   Error
        >0  -   Number of corresponding XML output file.

    See Also:
        <XT_Prepare>

*/

int getFileNumber(sqlite3* sqlDB,DWORD objID)
{
    int rc = 0;
    int result = 0;
    sqlite3_stmt *statement;
    char sqlQuery[1024]={0};
    //get all items which are selected and are not root objects
    sprintf(sqlQuery,"Select FileID from EvidenceObjects where ID = %lu;",objID);
    rc = sqlite3_prepare_v2(sqlDB,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc != SQLITE_ROW)
        {
            //error
            return -1;
        }
        result = sqlite3_column_int(statement,0);
    }
    sqlite3_finalize(statement);
    return result;
}

/*Function: getRootObj

    Returns the root object for a given object.

    For example AB/1, Partition 1 has a root object of AB/1. This is called prior to getFileNumber.

    Called from <XT_Prepare>

    Return:
        NULL    -   Error
        >0      -   X-Ways ID of root object

    See Also:
        <XT_Prepare>

*/

DWORD getRootObj(sqlite3* sqlDB,DWORD objID)
{
    HANDLE hEvidence = XWF_GetEvObj(objID);
    DWORD parent = (DWORD)XWF_GetEvObjProp(hEvidence,2,NULL);
    if (parent == 0)
    {
        return objID;
    }
    else
    {
        DWORD currObj = parent;
        do
        {
            hEvidence = XWF_GetEvObj(currObj);
            parent = (DWORD)XWF_GetEvObjProp(hEvidence,2,NULL);
            if (parent != 0)
            {
                currObj = parent;
            }
        } while (parent != 0);
        return currObj;
    }
    return NULL;
}

int recordError(sqlite3* sqlDB,int errorCode, LONG objID, LPWSTR srcText)
{
    //get file details
    LPWSTR xName = (LPWSTR)XWF_GetItemName(objID);
    int nameLen = wcslen(xName);
    wchar_t* filePath = getFullPath(srcText,objID,1);
    int pathLen = wcslen(filePath);
    wchar_t* sqlQuery;
    sqlQuery = new wchar_t[nameLen + pathLen  + 512];
    sqlQuery[0] = '\0';
    swprintf(sqlQuery, nameLen + pathLen + 512, L"INSERT INTO VICSError VALUES ('%ls', %lu, ?, ? );", errorValues[errorCode], objID);
    sqlite3_stmt *statement;
    int rc = sqlite3_prepare16_v2(sqlDB , sqlQuery,(wcslen(sqlQuery)+1)*sizeof(wchar_t) ,&statement, NULL);
    if (rc != SQLITE_OK)
    {
        //do error stuff here
        XWF_OutputMessage(sqlQuery,0);
        return -1;
    }
    rc = sqlite3_bind_text16(statement,1,xName,(nameLen+1)*sizeof(wchar_t),SQLITE_STATIC);
    if (rc != SQLITE_OK)
    {
        XWF_OutputMessage(L"Error Binding file name",0);
        return -3;
    }
    rc = sqlite3_bind_text16(statement,2,filePath ,(pathLen+1)*sizeof(wchar_t),SQLITE_STATIC);
    if (rc != SQLITE_OK)
    {
        XWF_OutputMessage(L"Error Binding file path",0);
        return -4;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE)
    {
        //do error stuff here
        XWF_OutputMessage(sqlQuery,0);
        return -2;
    }
    sqlite3_finalize(statement);
    return 0;

}

/*Section: SQL Options Functions */

//1.50 moved to new function & updated to include min sizes for files
int createOptionsExtractionTable(sqlite3* sqlDB)
{
    char* errMsg = 0;
    //1.50 updated to include min sizes for files
    int rc = sqlite3_exec(sqlDB,"CREATE TABLE ExtractionOptions (MinPicSize INT, MaxPicSize INT,MinVidSize INT, MaxVidSize INT,ReportDir TEXT,Overwrite INT, GriffeyeDir TEXT, TypeStatus INT, FileFormatStatus INT);", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}

//1.51 made schema version its own function
int insertOptionsSchemaRecord(sqlite3* sqlDB)
{
    char* errMsg;
    char sqlQuery[2048];
    int rc = sqlite3_exec(sqlDB,"DELETE FROM SchemaVersion", NULL, NULL, &errMsg);
    sprintf(sqlQuery,"INSERT INTO SchemaVersion VALUES (%i)",optionsSchemaVersion);
    rc = sqlite3_exec(sqlDB,sqlQuery, NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK) {return -2;}
    return 0;
}

//1.50 added schema version field
int createOptionsSchemaTable(sqlite3* sqlDB)
{
    char* errMsg = 0;
    int rc = sqlite3_exec(sqlDB,"CREATE TABLE SchemaVersion (version INT);", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        sqlite3_free(errMsg);
        return -1;
    }
    return insertOptionsSchemaRecord(sqlDB);
}

//1.50 add table to store last run details.
int createOptionsLastrunTable(sqlite3* sqlDB)
{
    char* errMsg = 0;
    int rc = sqlite3_exec(sqlDB,sqlCreateLastRun, NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}

int clearExtractionOptionsTable(sqlite3* db)
{
    char* errMsg = 0;
    char* sqlQuery = "DELETE FROM ExtractionOptions;";
    int rc = sqlite3_exec(db,sqlQuery,NULL,NULL,&errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}

int insertOptionsExtraction(sqlite3* sqlDB, ExtractOptions record)
{
    sqlite3_stmt* stmt;
    char* sqlQuery = "INSERT INTO ExtractionOptions VALUES (?,?,?,?,?,?,?,?,?);";
    int rc = sqlite3_prepare_v2(sqlDB,sqlQuery, strlen(sqlQuery), &stmt, NULL);
    if (rc!= SQLITE_OK)
    {
        XWF_OutputMessage(L"Error preparing ExtractionOptions Insert Record",0);
        return -1;
    }
    rc =sqlite3_bind_int64(stmt, 1, record.minPictureSize);
    rc =sqlite3_bind_int64(stmt, 2, record.maxPictureSize);
    rc =sqlite3_bind_int64(stmt, 3, record.minMovieSize);
    rc =sqlite3_bind_int64(stmt, 4, record.maxMovieSize);
    rc =sqlite3_bind_text16(stmt, 5, record.errorReportPath,
                            wcslen(record.errorReportPath)*sizeof(wchar_t), SQLITE_STATIC);
    int overwrite = 0;
    if (record.overwriteFiles) {overwrite = 1;}
    rc =sqlite3_bind_int(stmt, 6, overwrite);
    rc =sqlite3_bind_text16(stmt, 7, record.GriffeyePath,
                            wcslen(record.GriffeyePath)*sizeof(wchar_t), SQLITE_STATIC);
    //1.51 added new fields
    rc =sqlite3_bind_int(stmt, 8, record.TypeStatusFlags);
    rc =sqlite3_bind_int(stmt, 9, record.FileTypeFlag);
    rc = sqlite3_step(stmt);
    if (rc!=SQLITE_DONE)
    {
        XWF_OutputMessage(L"Error executing ExtractionOptions Insert Record",0);
        return -2;
    }
    return 0;
}


//1.50 new function
ExtractOptions getDefaultOptions()
{
    ExtractOptions retVal = {0};
    retVal.maxMovieSize = 0;
    retVal.maxPictureSize = 0;
    retVal.minPictureSize = 0;
    retVal.minMovieSize = 0;
    retVal.overwriteFiles = false;
    const char* path = "C:\\Program Files\\Griffeye Technologies\\Griffeye Analyze\\";
    snwprintf(retVal.GriffeyePath,2047,L"%s",path);
    char* reportPath = generateOptionsFolderString();
    snwprintf(retVal.errorReportPath,2047,L"%s",reportPath);
    delete[] reportPath;
    retVal.TypeStatusFlags = NOT_VERIFIED |IRRELEVANT|NOT_IN_LIST|CONFIRMED|
                            NOT_CONFIRMED|NEWLY_IDENTIFIED|MISMATCH_DETECTED;
    retVal.FileTypeFlag =  UNKNOWN|OK|IRREGULAR|CORRUPT;
    return retVal;
}

//1.50 new function for inserting default values
int insertOptionsDefaultExtraction(sqlite3* sqlDB, char path[])
{
    ExtractOptions exDetails = getDefaultOptions();
    return insertOptionsExtraction(sqlDB,exDetails);
}

int boolToInt(bool value)
{
    if (value){
        return 1;
    }
    return 0;
}

int intToBool(int value)
{
    if (value!=0){
        return true;
    }
    return false;
}

//1.50 - new function to insert last run settings
int insertExtractionDetails(sqlite3* db, ExtractionDetails *record)
{
    char* updateQuery = "Insert into lastSettings Values(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db,updateQuery,-1,&stmt,NULL);
    if (rc != SQLITE_OK) {
        int extError = sqlite3_extended_errcode(db);
        wchar_t* errormsg = (wchar_t*) sqlite3_errmsg16(db);
        XWF_OutputMessage(errormsg,0);
        XWF_OutputMessage(L"Cannot clear previously used last settings from lastSettings Table",0);
        return -1;
    }
    //bind variables
    // rc = sqlite3_bind_text16(statement,1,xName,(nameLen+1)*sizeof(wchar_t),SQLITE_STATIC);
    rc = sqlite3_bind_text16(stmt,1,record->C4PPath,-1,SQLITE_STATIC);
    rc = sqlite3_bind_text16(stmt,2,record->C4MPath,-1,SQLITE_STATIC);
    //set extraction options
    rc = sqlite3_bind_int(stmt,3,boolToInt(record->extractPictures));
    if (rc != SQLITE_OK){
            int extError = sqlite3_extended_errcode(db);
        wchar_t* errormsg = (wchar_t*) sqlite3_errmsg16(db);
        XWF_OutputMessage(errormsg,0);
        XWF_OutputMessage(L"Cannot clear previously used last settings from lastSettings Table",0);
    }
    rc = sqlite3_bind_int(stmt,4,boolToInt(record->extractVideos));
    rc = sqlite3_bind_int(stmt,5,boolToInt(record->checkParent));
    rc = sqlite3_bind_int(stmt,6,boolToInt(record->ignoreThumbs));
    rc = sqlite3_bind_int(stmt,7,boolToInt(record->exceptMismatch));
    rc = sqlite3_bind_int(stmt,8,boolToInt(record->exportReportTables));
    rc = sqlite3_bind_int(stmt,9,boolToInt(record->debugSet));
    //griffeye option
    rc = sqlite3_bind_int(stmt,OPTION_GRIFFEYE,boolToInt(record->createGriffeye));
    //output formats
    rc = sqlite3_bind_int(stmt,OPTION_EXTRACT_START,boolToInt(record->VICExport));
    rc = sqlite3_bind_int(stmt,OPTION_EXTRACT_START+1,boolToInt(record->C4ALLExport));
    rc = sqlite3_bind_int(stmt,OPTION_EXTRACT_START+2,boolToInt(record->VICSCompressed));


    rc = sqlite3_step(stmt);
    if (rc!= SQLITE_OK && rc != SQLITE_DONE){
        //error
    }
    sqlite3_finalize(stmt);
    return 0;
}

//1.50 read last run settings
int readExtractionSettings(sqlite3* db, ExtractionDetails *record)
{
    const char* sqlQuery = "Select PicPath,vidPath,extractPics,extractVids, ignoreCarvedWithin,ignoreThumbs,exceptMismatch, exportReportTables, debug, createGriffeye, exportVics, exportXML, exportCompressed from lastSettings;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db,sqlQuery,-1,&stmt,NULL);
    if (rc != SQLITE_OK) {
        int extError = sqlite3_extended_errcode(db);
        wchar_t* errormsg = (wchar_t*) sqlite3_errmsg16(db);
        XWF_OutputMessage(errormsg,0);
        XWF_OutputMessage(L"Cannot clear previously used last settings from lastSettings Table",0);
        return -1;
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        //actual data
        record->extractPictures = intToBool(sqlite3_column_int(stmt,2));
        record->extractPictures = intToBool(sqlite3_column_int(stmt,3));
        record->checkParent = intToBool(sqlite3_column_int(stmt,4));
        record->ignoreThumbs = intToBool(sqlite3_column_int(stmt,5));
        record->exceptMismatch = intToBool(sqlite3_column_int(stmt,6));
        record->exportReportTables = intToBool(sqlite3_column_int(stmt,7));
        //1.54 debug should always be false
        record->debugSet = false;
        record->createGriffeye = intToBool(sqlite3_column_int(stmt,9));
        record->VICExport = intToBool(sqlite3_column_int(stmt,10));
        record->C4ALLExport = intToBool(sqlite3_column_int(stmt,11));
        record->VICSCompressed = intToBool(sqlite3_column_int(stmt,12));
    }
    sqlite3_finalize(stmt);
    return 0;
}

//1.50 - new function to clear old settings
int clearExtractionDetails(sqlite3* db)
{
    char* errMsg = 0;
    const char* clearQuery = "DELETE FROM lastSettings;";
    int rc = sqlite3_exec(db,clearQuery,NULL,NULL,&errMsg);
    if (rc != SQLITE_OK){
        int extError = sqlite3_extended_errcode(db);
        wchar_t* errormsg = (wchar_t*) sqlite3_errmsg16(db);
        XWF_OutputMessage(errormsg,0);
        XWF_OutputMessage(L"Cannot clear previously used last settings from lastSettings Table",0);
        return -1;
    }
    return 0;
}

int sqlCreateOptions(char path[])
{
    sqlite3 *sqlDB;
    char optPath[MAX_PATH];
    sprintf(optPath,"%s\\%s",path,"opt.sqlite");
    int rc = sqlite3_open_v2(optPath,&sqlDB,SQLITE_OPEN_FULLMUTEX|SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK)
    {
        int extError = sqlite3_extended_errcode(sqlDB);
        wchar_t* errormsg = (wchar_t*) sqlite3_errmsg16(sqlDB);
        XWF_OutputMessage(errormsg,0);
        XWF_OutputMessage(L"Cannot create Options Sqlite database, exiting",0);
        sqlite3_close(sqlDB);
        return -1;
    }

    //1.50 updated to have seperate functions for table creation
    rc = createOptionsExtractionTable(sqlDB);
    if (rc <0) {sqlite3_close(sqlDB); return -2;}
    rc = createOptionsSchemaTable(sqlDB);
    if (rc <0) {sqlite3_close(sqlDB); return -3;}
    rc = createOptionsLastrunTable(sqlDB);
    if (rc <0) {sqlite3_close(sqlDB); return -4;}

    //add default values to extraction details
    rc = insertOptionsDefaultExtraction(sqlDB,path);
    if (rc <0) {sqlite3_close(sqlDB); return -5;}

    sqlite3_close(sqlDB);
    return 0;
}

int getOptionsSchemaValue(sqlite3* db)
{
    sqlite3_stmt *statement;
    int retVal = 0;
    const char* selectQuery = "Select * from SchemaVersion;";
    int rc = sqlite3_prepare_v2(db,selectQuery, strlen(selectQuery),&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW){
            retVal = sqlite3_column_int(statement,0);
        }
    }
    sqlite3_finalize(statement);
    return retVal;
}

int getOptionsSchemaVersion(sqlite3* db)
{
    char* query = "Select count(*) from SQLITE_MASTER where Tbl_name like '%SchemaVersion%';";
    sqlite3_stmt *statement;
    int rc = sqlite3_prepare_v2(db,query, strlen(query),&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc != SQLITE_ROW){
            sqlite3_finalize(statement);
            return -1;
        }
        //1.51 fixed as it currently returns count of table, not actual version!
        int rows = sqlite3_column_int(statement,0);
        if (rows == 0) {
            sqlite3_finalize(statement);
            return 0;
        }
        sqlite3_finalize(statement);
        return getOptionsSchemaValue(db);
    }
    return -2;
}

//1.50 moved each schema version into own
ExtractOptions extractOldSchemaOptions(sqlite3* db, bool* error)
{
    ExtractOptions retOpt = {0};
    char sqlQuery[1024]={0};
    sqlite3_stmt* statement;
    sprintf(sqlQuery,"Select MaxPicSize, MaxVidSize, ReportDir, Overwrite, GriffeyeDir from ExtractionOptions;");
    int rc = sqlite3_prepare_v2(db,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc != SQLITE_ROW)
        {
            //error
            *error = true;
            return retOpt;
        }
        retOpt.maxPictureSize = sqlite3_column_int64(statement,0);
        retOpt.maxMovieSize = sqlite3_column_int64(statement,1);
        int overwrite = sqlite3_column_int(statement,3);
        if (overwrite == 0){
            retOpt.overwriteFiles = FALSE;
        }
        else{
            retOpt.overwriteFiles = TRUE;
        }
        int bytes = sqlite3_column_bytes(statement,2);
        char* temp = (char*)sqlite3_column_text(statement,2);
        swprintf(retOpt.errorReportPath, L"%s",temp);
        bytes = sqlite3_column_bytes(statement,4);
        char* temp2 = (char*)sqlite3_column_text(statement,4);
        swprintf(retOpt.GriffeyePath, L"%s",temp2);
    }
    sqlite3_finalize(statement);
    return retOpt;
}

//1.50 add new schema for extraction
ExtractOptions extractV1SchemaOptions(sqlite3* db, bool* error)
{
    ExtractOptions retOpt = {0};
    char sqlQuery[1024]={0};
    sqlite3_stmt* statement;
    sprintf(sqlQuery,"Select * from ExtractionOptions;");
    int rc = sqlite3_prepare_v2(db,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc != SQLITE_ROW)
        {
            //error
            *error = true;
            return retOpt;
        }
        retOpt.minPictureSize = sqlite3_column_int64(statement,0);
        retOpt.maxPictureSize = sqlite3_column_int64(statement,1);
        retOpt.minMovieSize = sqlite3_column_int64(statement,2);
        retOpt.maxMovieSize = sqlite3_column_int64(statement,3);
        int overwrite = sqlite3_column_int(statement,5);
        if (overwrite == 0){
            retOpt.overwriteFiles = FALSE;
        }
        else{
            retOpt.overwriteFiles = TRUE;
        }
        int bytes = sqlite3_column_bytes(statement,4);
        char* temp = (char*)sqlite3_column_text(statement,4);
        swprintf(retOpt.errorReportPath, L"%s",temp);
        bytes = sqlite3_column_bytes(statement,6);
        char* temp2 = (char*)sqlite3_column_text(statement,6);
        swprintf(retOpt.GriffeyePath, L"%s",temp2);
    }
    sqlite3_finalize(statement);
    return retOpt;
}

//1.51 add new schema for extraction
ExtractOptions extractV2SchemaOptions(sqlite3* db, bool* error)
{
    ExtractOptions retOpt = {0};
    char sqlQuery[1024]={0};
    sqlite3_stmt* statement;
    sprintf(sqlQuery,"Select * from ExtractionOptions;");
    int rc = sqlite3_prepare_v2(db,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc != SQLITE_ROW)
        {
            //error
            *error = true;
            return retOpt;
        }
        retOpt.minPictureSize = sqlite3_column_int64(statement,0);
        retOpt.maxPictureSize = sqlite3_column_int64(statement,1);
        retOpt.minMovieSize = sqlite3_column_int64(statement,2);
        retOpt.maxMovieSize = sqlite3_column_int64(statement,3);
        int overwrite = sqlite3_column_int(statement,5);
        if (overwrite == 0){
            retOpt.overwriteFiles = FALSE;
        }
        else{
            retOpt.overwriteFiles = TRUE;
        }
        int bytes = sqlite3_column_bytes(statement,4);
        char* temp = (char*)sqlite3_column_text(statement,4);
        swprintf(retOpt.errorReportPath, L"%s",temp);
        bytes = sqlite3_column_bytes(statement,6);
        char* temp2 = (char*)sqlite3_column_text(statement,6);
        swprintf(retOpt.GriffeyePath, L"%s",temp2);
        retOpt.TypeStatusFlags = sqlite3_column_int(statement,7);
        retOpt.FileTypeFlag = sqlite3_column_int(statement,8);
    }
    sqlite3_finalize(statement);
    return retOpt;
}

//1.50 extract data from the stored schema.
ExtractOptions selectSchema(sqlite3* db, int schemaVersion, bool* error)
{
    switch (schemaVersion){
        case 0:
            return extractOldSchemaOptions(db, error);
            break;
        case 1:
            return extractV1SchemaOptions(db, error);
            break;
        case 2:
            return extractV2SchemaOptions(db, error);
        default:
            ExtractOptions retVal = {0};
            *error = true;
            return retVal;
    }
}

//1.50 new function
int optionsDropTable(sqlite3* db)
{
    char* errMsg=0;
    int rc = sqlite3_exec(db,"DROP TABLE ExtractionOptions",NULL, NULL, &errMsg);
    if (rc != SQLITE_OK && rc != SQLITE_DONE) {return -1;}
    //don't check if these succeed as may not exist
    rc = sqlite3_exec(db,"DROP TABLE SchemaVersion",NULL, NULL, &errMsg);
    rc = sqlite3_exec(db,"DROP TABLE LastSettings",NULL, NULL, &errMsg);
    return 0;
}

//1.51 add function to fill in default values for options
void addDefaultValues(ExtractOptions* record, int schemaVersion)
{
    if (schemaVersion <= 1)
    {
        record->TypeStatusFlags = NOT_VERIFIED |IRRELEVANT|NOT_IN_LIST|CONFIRMED|
                                NOT_CONFIRMED|NEWLY_IDENTIFIED|MISMATCH_DETECTED;
        record->FileTypeFlag =  UNKNOWN|OK|IRREGULAR|CORRUPT;
    }
}

//1.50 new function to update to latest schema after loading
int updateOptionsSchema(sqlite3* db, ExtractOptions *record, char path[], int schemaVersion)
{
    if (schemaVersion == 0)
    {
        //drop tables
        int rc = optionsDropTable(db);
        if (rc!=0) {return -1;}
        //create tables
        createOptionsExtractionTable(db);
        createOptionsSchemaTable(db);
        createOptionsLastrunTable(db);
        //insert given options
        insertOptionsExtraction(db,*record);
    }
    //1.51 add for moving from version 1 to version 2
    else if (schemaVersion == 1)
    {
        BOOL retValue;
        char* errMsg;
        int rc = sqlite3_exec(db,"DROP TABLE ExtractionOptions",NULL, NULL, &errMsg);
        createOptionsExtractionTable(db);
        addDefaultValues(record,schemaVersion);
        insertOptionsExtraction(db,*record);
        insertOptionsSchemaRecord(db);
    }
    else{
        //unknown schema version
    }

    return 0;
}

ExtractOptions loadOptions(char path[])
{
    ExtractOptions retOpt ={0};
    sqlite3 *sqlDB;
    int rc = sqlite3_open_v2(path,&sqlDB,SQLITE_OPEN_FULLMUTEX|SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK){
        int extError = sqlite3_extended_errcode(sqlDB);
        wchar_t* errormsg = (wchar_t*) sqlite3_errmsg16(sqlDB);
        XWF_OutputMessage(errormsg,0);
        XWF_OutputMessage(L"Cannot open Options Sqlite database, using default values",0);
        sqlite3_close(sqlDB);
        retOpt = getDefaultOptions();
        return retOpt;
    }
    int schemaVersion = getOptionsSchemaVersion(sqlDB);
    bool error;
    retOpt = selectSchema(sqlDB,schemaVersion,&error);
    if (error){
        //error - possibly corrupt. Recreate table with default values
        retOpt = getDefaultOptions();
        updateOptionsSchema(sqlDB, &retOpt, path, schemaVersion);
    }
    if (schemaVersion < optionsSchemaVersion){
        //update to new schema here.
        updateOptionsSchema(sqlDB, &retOpt, path, schemaVersion);
    }
    sqlite3_close_v2(sqlDB);
    return retOpt;
}


int saveOptions(char path[], ExtractOptions opt)
{
    sqlite3 *sqlDB;
    sqlite3_stmt *statement;
    int rc = sqlite3_open_v2(path,&sqlDB,SQLITE_OPEN_FULLMUTEX|SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK)
    {
        int extError = sqlite3_extended_errcode(sqlDB);
        wchar_t* errormsg = (wchar_t*) sqlite3_errmsg16(sqlDB);
        XWF_OutputMessage(errormsg,0);
        XWF_OutputMessage(L"Cannot open Options Sqlite database, exiting",0);
        sqlite3_close(sqlDB);
        return -1;
    }
    rc = clearExtractionOptionsTable(sqlDB);

    rc = insertOptionsExtraction(sqlDB,opt);
    sqlite3_close_v2(sqlDB);
    return 0;
}

//SQLite error reporting

void outputErrorStats(sqlite3* sqlDB,WORD versionNo)
{
    wchar_t sqlQuery[512];
    for (int i=0;i<NoErrorTypes;i++)
    {
        swprintf(sqlQuery, 512, L"Select count(*) from VICSError where errortype = '%ls';", errorValues[i]);
        sqlite3_stmt *statement;
        int rc = sqlite3_prepare16_v2(sqlDB , sqlQuery,(wcslen(sqlQuery)+1)*sizeof(wchar_t) ,&statement, NULL);
        if (rc != SQLITE_OK)
        {
            XWF_OutputMessage(L"Error preparing query for error table",0);
            return;
        }
        rc = sqlite3_step(statement);
        if (rc != SQLITE_ROW)
        {
            XWF_OutputMessage(L"Error executing query for error table",0);
            return;
        }
        int noErrors = sqlite3_column_int(statement,0);
        wchar_t tempMessage[512]={0};
        swprintf(tempMessage, 512, L"Number of \"%ls\" errors: %i", errorValues[i], noErrors);
        sqlite3_finalize(statement);
        if (versionNo >= 1940)
        {
            XWF_OutputMessage(tempMessage,16);
        }
        else
        {
            XWF_OutputMessage(tempMessage,0);
        }
    }
}

/*Section: Database Functions */

/*Function: SQLDatabaseExists

    Function that checks if a valid SQLite database exists at the path provided as a parameter

    Parameters:

        char path[]       - Path to a possible SQLite database file

    Returns:
        false   -   Database does not exist in that location
        true    -   Database exists in that location

    See Also:
        Called by   -   <loadOrCreateOptions>
*/

BOOL SQLDatabaseExists(char path[])
{
    sqlite3 *sqlDB;
    int rc = sqlite3_open_v2(path,&sqlDB,SQLITE_OPEN_FULLMUTEX|SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK)
    {
        return false;
    }
    else
    {
        sqlite3_close(sqlDB);
        return true;
    }
}

/*Function: loadOrSaveDb

    Function that takes the in-memory database and saves it to a given path or loads a filepath into an in-memory database

    Parameters:

        sqlite3* database       - Handle to an in-memory SQLite3 database to be save/loaded to

        const chat*zFilename    - File path to either save to or load from.

        int save                - whether this is an operation to save the db or load it from a file

    Returns:
        rc  -   Can be any valid SQLITE Error code


    See Also:
        Called by   -   <writeSQLMediaRecord>
*/

int loadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave)
{
    int rc;
    sqlite3 *pFile, *pTo, *pFrom;
    sqlite3_backup *pBackup;
    rc = sqlite3_open(zFilename, &pFile);
    if (rc == SQLITE_OK) {
        pFrom = (isSave ? pInMemory :pFile);
        pTo = (isSave ? pFile:pInMemory);
        pBackup = sqlite3_backup_init(pTo,"main",pFrom,"main");
        if (pBackup){
            (void)sqlite3_backup_step(pBackup,-1);
            (void)sqlite3_backup_finish(pBackup);
        }
        rc = sqlite3_errcode(pTo);
    }
    (void)sqlite3_close(pFile);
    return rc;
}

/*Section: Update SQL Records

/*Function: updateMediaFileRecord

    Function that updates a VICS Mediafile record into a VICS database.

    Used as part of the duplicate detection functions

    Parameters:

        sqlite3* database       - Handle to an SQLite3 database

        VICSMediaFile &record   - a VICSMediaFile record that contains data to be inserted into database

        int picture             - flag to indicate item is a picture. Value of 1 is a picture

    Returns:
        -2      -   Error executing SQLite statement
        -1      -   Error preparing SQLite statement
        0       -   No records in table
        >0      -   Number of records in given table


    See Also:
        Called by   -   <writeSQLMediaRecord>
*/

int updateMediaFileRecord(sqlite3* vicsDB, VICSMediaFile* record, int picture, wchar_t* MD5, LONG dupItemID)
{
    sqlite3_stmt* statement;
    wchar_t* sqlQuery;
    wchar_t tableName[20] = {0};
    sqlQuery = new wchar_t[512];
    sqlQuery[0] = '\0';
    if (picture == 1){
        wcscpy(tableName, L"VICSPicsRecords\0");
    }
    else{
        wcscpy(tableName,L"VICSMoviesRecords\0");
    }
    //"CREATE TABLE VICSPicsRecords (MD5Hash TEXT NOT NULL, FileName TEXT NOT NULL,FilePath TEXT NOT NULL,Created INT,Modified INT,Accessed INT,
                                     //Unallocated INT,SourceID TEXT,PhysicalLocation INT,Deleted INT,parentMD5 TEXT,parentName TEXT,parentPath TEXT,parentPhysLoc INT, itemID INT)"
    swprintf(sqlQuery,L"UPDATE %ls SET FileName = ?, FilePath = ?,Created = ?, modified = ?, accessed = ?, unallocated =?, SourceID = ?,PhysicalLocation= ?, Deleted=?,"
             L"parentMD5 = ?, parentName = ?, parentPath = ?, parentPhysLoc = ?, itemID = ? WHERE MD5Hash = ? and itemID = ?;",tableName);
    //swprintf(sqlQuery,L"UPDATE %ls SET FileName = ?, FilePath = ?,Created = %llu, modified = %llu, accessed = %llu, unallocated =%i, SourceID = \'%ls\',PhysicalLocation= %llu, Deleted=%i, parentMD5 = ?, parentName = ?, parentPath = ?, parentPhysLoc = %llu, itemID = %li where MD5Hash = ? and itemID = ?",tableName,record.MD5, Filetime2INT64(record.created),
    //         Filetime2INT64(record.written), Filetime2INT64(record.accessed), record.unallocated, record.sourceID, record.physicalLocation, record.deleted,record.parentPhysLoc, record.XWFitemID);
    int rc = sqlite3_prepare16_v2(vicsDB,sqlQuery,-1,&statement,NULL);
    if (rc != SQLITE_OK){
        XWF_OutputMessage(L"Error Binding file name",0);
        return -3;
    }
    //bind record variables
    rc = sqlite3_bind_text16(statement,1,record->fileName,(wcslen(record->fileName)+1)*sizeof(wchar_t),SQLITE_STATIC);
    if (rc != SQLITE_OK){
        XWF_OutputMessage(L"Error Binding file name",0);
        return -3;
    }
    rc = sqlite3_bind_text16(statement,2,record->filePath,(wcslen(record->filePath)+1)*sizeof(wchar_t),SQLITE_STATIC);
    if (rc != SQLITE_OK){
        XWF_OutputMessage(L"Error Binding file name",0);
        return -3;
    }
    //bind filetimes
    ULARGE_INTEGER timestamp;
    timestamp.HighPart = record->created.dwHighDateTime;
    timestamp.LowPart = record->created.dwLowDateTime;
    sqlite3_bind_int64(statement,3,timestamp.QuadPart);
    timestamp.HighPart = record->written.dwHighDateTime;
    timestamp.LowPart = record->written.dwLowDateTime;
    sqlite3_bind_int64(statement,4,timestamp.QuadPart);
    timestamp.HighPart = record->accessed.dwHighDateTime;
    timestamp.LowPart = record->accessed.dwLowDateTime;
    sqlite3_bind_int64(statement,5,timestamp.QuadPart);
    //bind other attributes
    if (record->unallocated) { sqlite3_bind_int,6,1;} else {sqlite3_bind_int,6,0;}
    rc = sqlite3_bind_text16(statement,7,record->sourceID,-1,SQLITE_STATIC);
    if (rc != SQLITE_OK){
        XWF_OutputMessage(L"Error Binding Source ID",0);
        return -3;
    }
    sqlite3_bind_int64(statement,8,record->physicalLocation);
    if (record->deleted) { sqlite3_bind_int,9,1;} else {sqlite3_bind_int,9,0;}
    //bind parent attributes
    if (record->parentMD5[0] != '\0') { sqlite3_bind_text16(statement,10,record->parentMD5,-1, SQLITE_STATIC); } else { sqlite3_bind_text16(statement,10,"",-1,SQLITE_TRANSIENT); }
    if (record->parentName != NULL) { sqlite3_bind_text16(statement,11,record->parentName,-1,SQLITE_STATIC); } else { sqlite3_bind_text16(statement,11,"",-1,SQLITE_TRANSIENT); }
    if (record->parentFilePath != NULL) { sqlite3_bind_text16(statement,12,record->parentFilePath,-1,SQLITE_STATIC); } else { sqlite3_bind_text16(statement,12,"",-1,SQLITE_TRANSIENT); }
    sqlite3_bind_int64(statement,13,record->parentPhysLoc);
    sqlite3_bind_int(statement,14,record->XWFitemID);
    //bind parameters
    rc = sqlite3_bind_text16(statement,15,MD5,-1,SQLITE_STATIC);
    rc = sqlite3_bind_int(statement,16,dupItemID);
    //step statement
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE)
    {
        //do error stuff here
        XWF_OutputMessage(sqlQuery,0);
        return -2;
    }
    sqlite3_finalize(statement);
    return 0;
}


/*Section: SQL Record Insertion Functions */

/*Function: insertMediaFileRecord

    Function that inserts a VICS Mediafile record into a VICS database.

    In case of an error, SQLite command is put in output window

    Parameters:

        sqlite3* database       - Handle to an SQLite3 database

        VICSMediaFile &record   - a VICSMediaFile record that contains data to be inserted into database

        int picture             - flag to indicate item is a picture. Value of 1 is a picture

    Returns:
        -2      -   Error executing SQLite statement
        -1      -   Error preparing SQLite statement
        0       -   No records in table
        >0      -   Number of records in given table


    See Also:
        Called by   -   <writeSQLMediaRecord>
*/

int insertMediaFileRecord(sqlite3* vicsDB, VICSMediaFile &record, int picture)
{
    wchar_t* sqlQuery;
    wchar_t tableName[20] = {0};
    INT64 sizeofData = getMediaFileRecordSize(record);
    sqlQuery = new wchar_t[sizeofData + 512];
    sqlQuery[0] = '\0';
    if (picture == 1)
    {
        wcscpy(tableName, L"VICSPicsRecords\0");
    }
    else
    {
        wcscpy(tableName,L"VICSMoviesRecords\0");
    }
    swprintf(sqlQuery,L"INSERT INTO %ls VALUES (\'%ls\', ?, ?,%llu, %llu, %llu, %i, \'%ls\', %llu, %i",tableName,record.MD5, Filetime2INT64(record.created),
             Filetime2INT64(record.written), Filetime2INT64(record.accessed), record.unallocated, record.sourceID, record.physicalLocation, record.deleted);
    if (record.parentMD5[0] != '\0') { swprintf(sqlQuery+wcslen(sqlQuery),L", %ls", record.parentMD5); } else { wcscat(sqlQuery,L",\'\'"); }
    if (record.parentName != NULL) { swprintf(sqlQuery+wcslen(sqlQuery),L", %ls", record.parentName); } else { wcscat(sqlQuery,L",\'\'"); }
    if (record.parentFilePath != NULL) { swprintf(sqlQuery+wcslen(sqlQuery),L", %ls", record.parentFilePath); } else { wcscat(sqlQuery,L",\'\'"); }
    //1.50 added itemID
    swprintf(sqlQuery+wcslen(sqlQuery),L", %llu, %lu",record.parentPhysLoc, record.XWFitemID);
    wcscat(sqlQuery,L");");
    sqlite3_stmt *statement;
    int rc = sqlite3_prepare16_v2(vicsDB , sqlQuery,(wcslen(sqlQuery)+1)*sizeof(wchar_t) ,&statement, NULL);
    if (rc != SQLITE_OK)
    {
        //do error stuff here
        XWF_OutputMessage(sqlQuery,0);
        return -1;
    }
    rc = sqlite3_bind_text16(statement,1,record.fileName,(wcslen(record.fileName)+1)*sizeof(wchar_t),SQLITE_STATIC);
    if (rc != SQLITE_OK)
    {
        XWF_OutputMessage(L"Error Binding file name",0);
        return -3;
    }
    rc = sqlite3_bind_text16(statement,2,record.filePath ,(wcslen(record.filePath)+1)*sizeof(wchar_t),SQLITE_STATIC);
    if (rc != SQLITE_OK)
    {
        XWF_OutputMessage(L"Error Binding file path",0);
        return -4;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE)
    {
        //do error stuff here
        XWF_OutputMessage(sqlQuery,0);
        return -2;
    }
    sqlite3_finalize(statement);
    return 0;
}

/*Function: insertMediaRecord

    Function that inserts a VICS Media record into a VICS database.

    In case of an error, SQLite command is put in output window

    Parameters:

        sqlite3* database   - Handle to an SQLite3 database

        VICSMedia &record   - a VICSMedia record that contains data to be inserted into database

        int picture         - flag to indicate item is a picture. Value of 1 is a picture

    Returns:
        -2      -   Error executing SQLite statement
        -1      -   Error preparing SQLite statement
        0       -   No records in table
        >0      -   Number of records in given table


    See Also:
        Called by   -   <writeSQLMediaRecord>
*/

int insertMediaRecord(sqlite3* vicsDB, VICSMedia &record, int picture)
{
    wchar_t* sqlQuery;
    wchar_t tableName[20] = {0};
    INT64 sizeofData = getMediaRecordSize(record);
    sqlQuery = new wchar_t[sizeofData + 512];
    sqlQuery[0] = '\0';
    if (picture == 1)
    {
        wcscpy(tableName, L"VICSPics");
    }
    else
    {
        wcscpy(tableName,L"VICSMovies");
    }
    swprintf(sqlQuery,L"INSERT INTO %ls VALUES (%llu, %i, \'%ls\', \'%ls\', %i, %i, %i",tableName,record.MediaID, record.Category, record.SHA1, record.MD5, record.VictimID, record.OffenderID, record.IsDistributed);
    if (record.Comments != NULL) { swprintf(sqlQuery+wcslen(sqlQuery),L", \'%ls\'", record.Comments); } else { wcscat(sqlQuery+wcslen(sqlQuery),L",\'\'"); }
    if (record.Tags != NULL) { swprintf(sqlQuery+wcslen(sqlQuery),L", \'%ls\'", record.Tags); } else { wcscat(sqlQuery+wcslen(sqlQuery),L",\'\'"); }
    if (record.Series != NULL) { swprintf(sqlQuery+wcslen(sqlQuery),L", \'%ls\'", record.Series); } else { wcscat(sqlQuery+wcslen(sqlQuery),L",\'\'"); }
    swprintf(sqlQuery+wcslen(sqlQuery),L", %llu",record.MediaSize);
    if (record.RelativeFilePath != NULL) { swprintf(sqlQuery+wcslen(sqlQuery),L", \'%ls\'", record.RelativeFilePath); } else { wcscat(sqlQuery+wcslen(sqlQuery),L",\'\'"); }
    swprintf(sqlQuery+wcslen(sqlQuery),L", %llu , %i",Filetime2INT64(record.DateUpdated), record.timeZone);
    if (record.PrecatSource != NULL) { swprintf(sqlQuery+wcslen(sqlQuery),L", \'%ls\'", record.PrecatSource); } else { wcscat(sqlQuery+wcslen(sqlQuery),L",\'\'"); }
    swprintf(sqlQuery+wcslen(sqlQuery),L", %i",record.IsSuspected);
    if (record.MimeType != NULL) { swprintf(sqlQuery+wcslen(sqlQuery),L", \'%ls\'", record.MimeType); } else { wcscat(sqlQuery+wcslen(sqlQuery),L",\'\'"); }
    if (record.PhotoDNA[0] != L'\0') { swprintf(sqlQuery+wcslen(sqlQuery),L", \'%ls\'", record.PhotoDNA); } else { wcscat(sqlQuery+wcslen(sqlQuery),L",\'\'"); }
    wcscat(sqlQuery, L");");
    sqlite3_stmt *statement;
    int rc = sqlite3_prepare16_v2(vicsDB , sqlQuery,(wcslen(sqlQuery)+1)*sizeof(wchar_t),&statement, NULL);
    if (rc != SQLITE_OK)
    {
        //do error stuff here
        XWF_OutputMessage(sqlQuery,0);
        return -1;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE)
    {
        //do error stuff here
        XWF_OutputMessage(sqlQuery,0);
        return -2;
    }
    sqlite3_finalize(statement);
    return 0;
}


/*Function: insertMediaMetadataRecord

    Function to save MediaMetadata record to SQL database.

    Currently unimplemented

    Parameters:


    Returns:


    See Also:

*/

int insertMediaMetadataRecord(sqlite3* vicsDB, VICSMediaMetadata record)
{
    //figure length that data needs to be. MD5 is 32 bytes add 128 bytes to include query as well.
    int dataLen = wcslen(record.PropertyName) + wcslen(record.PropertyValue)+ 128;
    wchar_t* sqlQuery = new wchar_t[dataLen];
    swprintf(sqlQuery,L"INSERT INTO MediaMetadata VALUES (\'%ls\',\'%ls\',\'%ls\');", record.MD5,record.PropertyName, record.PropertyValue );

    //run query
    sqlite3_stmt *statement;
    int rc = sqlite3_prepare16_v2(vicsDB , sqlQuery,(wcslen(sqlQuery)+1)*sizeof(wchar_t),&statement, NULL);
    if (rc != SQLITE_OK)
    {
        //do error stuff here
        XWF_OutputMessage(sqlQuery,0);
        delete[] sqlQuery;
        return -1;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE)
    {
        //do error stuff here
        XWF_OutputMessage(sqlQuery,0);
        sqlite3_finalize(statement);
        delete[] sqlQuery;
        return -2;
    }
    //cleanup
    sqlite3_finalize(statement);
    delete[] sqlQuery;
    return 0;
}

/*Section: SQL Record Existence Functions

    Functions to check if various records already exist in tables
*/




/*Section: SQL Table Extraction

    Functions to get data from tables or counts of files in tables

*/

/*Function: existsMediaMetadata

    Function that checks if a PropertyName already exists for a given MD5 hash value

    Added in 1.41

    Parameters:

        sqlite3* database               - Handle to an SQLite3 database

        wchar_t* MD5                    - Pointer to Wide character string with MD5 hash value

        wchar_t* PropertyName           - Pointer to Wide Character String with PropertyName

    Returns:
        -1      -   Error
        0       -   No records in table
        1       -   Record Exists


    See Also:
        Called by   -
*/

int existsMediaMetadata(sqlite3* database, wchar_t* MD5, wchar_t* PropertyName)
{
    char sqlQuery[512]={0};
    int retVal = 0;
    sqlite3_stmt* statement;
    snprintf(sqlQuery,512, "Select count(*) from MediaMetadata where MD5Hash = \'%ls\' and PropertyName = \'%ls\';",MD5,PropertyName);
    int rc = sqlite3_prepare_v2(database,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW)
        {
            retVal = sqlite3_column_int(statement,0);
        }
        else
        {
            sqlite3_finalize(statement);
            return -1;
        }
    }
    else
    {
        return -2;
    }
    sqlite3_finalize(statement);
    if (retVal == 0) {return 0;}
    return 1;

}


int getRowCount(sqlite3* database, char* tablename, wchar_t* hashValue)
{
    char sqlQuery[256]={0};
    int retVal = 0;
    sqlite3_stmt* statement;
    snprintf(sqlQuery,256, "Select count(*) from %s where MD5Hash = \'%ls\';",tablename,hashValue);
    int rc = sqlite3_prepare_v2(database,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW)
        {
            retVal = sqlite3_column_int(statement,0);
        }
        else
        {
            sqlite3_finalize(statement);
            return -1;
        }
    }
    else
    {
        return -2;
    }
    sqlite3_finalize(statement);
    return retVal;
}

/*Function: getRowCount

    Function that retrieves the number of records that exist within a given tablename.
    Second version

    Parameters:

        sqlite3* database               - Handle to an SQLite3 database

        char* tablename                 - pointer to string containing table name. Must be NULL terminated.

        wchar_t* hashValue (optional)   - Pointer to wide character string containing hash value to match against. Must be null terminated. Omission retrieves all records.

    Returns:
        -2      -   Error preparing SQLite statement
        -1      -   Error executing SQLite statement
        0       -   No records in table
        >0      -   Number of records in given table


    See Also:
        Called by   -   <returnMediaRecords> , <returnMediaFileRecords>
*/

int getRowCount(sqlite3* database, char* tablename)
{
    char sqlQuery[256]={0};
    int retVal = 0;
    sqlite3_stmt* statement;
    snprintf(sqlQuery,256, "Select count(*) from %s ;",tablename);
    int rc = sqlite3_prepare_v2(database,sqlQuery,strlen(sqlQuery)+1,&statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(statement);
        if (rc == SQLITE_ROW)
        {
            retVal = sqlite3_column_int(statement,0);
        }
        else
        {
            sqlite3_finalize(statement);
            return -1;
        }
    }
    else
    {
        return -2;
    }
    sqlite3_finalize(statement);
    return retVal;
}

/*Function: returnMediaFileRecords

    This function will take the Sqlite statement provided as a parameter and
    select all records from the picture/Movie files table that correspond to hash value

    Statement parameter should be blank (i.e. has not been used or has been finalized previously)
    and calling function must finalize after use

    Parameters:

        sqlite3* database           - Handle to an SQLite3 database

        sqlite3_stmt** statement    - Pointer to sqlite3_stmt* that will hold results. Needs to be double ** to ensure results go back to calling function

        int picture                 - Integer to state if getting picture of video records. 1 indicates pictures.

        wchar_t* hashvalue          - Pointer to a wide character string that contains MD5 hash value to be searched for. Must be NULL terminated

    Return:
        -2  -   Error executing SQL Query
        -1  -   Error preparing statement - should never happen
        0   -   Not records located
        >0  -   Number of records located

    See Also:
        Called by   -   <extractIntoVicsRecord>

        <getRowCount>

*/

int returnMediaFileRecords(sqlite3* database, sqlite3_stmt** statement, int picture, wchar_t* hashValue)
{
    char tablename[30] = {0}, sqlQuery[256]={0};
    int retVal = 0;
    if (picture ==1){
        strncpy(tablename,"VICSPicsRecords",20);
    }
    else{
        strncpy(tablename,"VICSMoviesRecords",20);
    }
    //start off with a count of that table
    retVal = getRowCount(database, (char*)&tablename, hashValue);
    if (retVal < 0)
    {
        return -1;
    }
    snprintf(sqlQuery,256, "Select * from %s where MD5Hash = \'%ls\';",tablename,hashValue);
    int rc = sqlite3_prepare_v2(database,sqlQuery,(strlen(sqlQuery)+1)*sizeof(wchar_t),statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(*statement);
        if (rc == SQLITE_ROW){
            return retVal;
        }
        else if (rc == SQLITE_OK){
            return 0;
        }
        else{
            return -2;
        }
    }
    else
    {
        //SQL Error
        return -3;
    }
    return retVal;
}

/*Function: returnMediaRecords

    Function that retrieves the contents of either the VICSPics or VICSMovies table. A blank statement is passed as a parameter.

    Calling function is responsible for freeing memory associated with statement parameter

    Parameters:

        sqlite3* database           - Handle to an SQLite3 database

        sqlite3_stmt** statement    - Pointer to sqlite3_stmt* that will hold results. Needs to be double ** to ensure results go back to calling function

        int picture                 - Integer to state if getting picture of video records. 1 indicates pictures.

    Returns:
        <0      -   Error
        0       -   No records in table
        >0      -   Number of records referenced by statement


    See Also:
        Called by   -   <writeRecords>

        <getRowCount>
*/

int returnMediaRecords(sqlite3* database, sqlite3_stmt** statement, int picture)
{
    char tablename[30] = {0}, sqlQuery[256]={0};
    int retVal = 0;
    if (picture ==1){
        strncpy(tablename,"VICSPics",20);
    }
    else{
        strncpy(tablename,"VICSMovies",20);
    }
    //start off with a count of that table
    retVal = getRowCount(database, (char*)&tablename);
    if (retVal < 0)
    {
        return -1;
    }
    snprintf(sqlQuery,256, "Select * from %s;",tablename);
    int rc = sqlite3_prepare_v2(database,sqlQuery,(strlen(sqlQuery)+1)*sizeof(wchar_t),statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(*statement);
        if (rc == SQLITE_ROW){
            return retVal;
        }
        else if (rc == SQLITE_OK || rc == SQLITE_DONE){
            return 0;
        }
        else{
            return -2;
        }
    }
    else
    {
        //SQL Error
        return -1;
    }
    return retVal;
}

/*Function: returnMediaMetadataRecords

    Function that retrieves the contents of the MediaMetadata table for a given MD5 hash. A blank statement is passed as a parameter.

    Calling function is responsible for freeing memory associated with statement parameter

    Parameters:

        sqlite3* database           - Handle to an SQLite3 database

        sqlite3_stmt** statement    - Pointer to sqlite3_stmt* that will hold results. Needs to be double ** to ensure results go back to calling function

        wchar_t* hashValue          - Pointer to a wide character string the contains the MD5 hash value to match against records.

    Returns:
        <0      -   Error
        0       -   No records in table
        >0      -   Number of records referenced by statement


    See Also:
        Called by   -   <writeRecords>

        <getRowCount>
*/

int returnMediaMetadataRecords(sqlite3* database, sqlite3_stmt** statement, wchar_t* hashValue)
{
    char sqlQuery[256]={0};
    int retVal = 0;
    //start off with a count of that table
    retVal = getRowCount(database, "MediaMetadata", hashValue);
    if (retVal < 0)
    {
        return -1;
    }
    if (retVal == 0)
    {
        //no records, no further requirement
        return 0;
    }
    snprintf(sqlQuery,256, "Select * from MediaMetadata where MD5hash = \'%ls\';",hashValue);
    int rc = sqlite3_prepare_v2(database,sqlQuery,(strlen(sqlQuery)+1)*sizeof(wchar_t),statement,NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(*statement);
        if (rc == SQLITE_ROW){
            return retVal;
        }
        else if (rc == SQLITE_OK || rc ==SQLITE_OK){
            return 0;
        }
        else{
            return -2;
        }
    }
    else
    {
        //SQL Error
        return -1;
    }
    return retVal;
}

