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
BOOL sqlDatabaseExists(char path[]);
ExtractOptions loadOptions(char path[]);
int saveOptions(char path[], ExtractOptions opt);
void outputErrorStats(sqlite3* sqlDB,WORD versionNo);

const int optionsSchemaVersion = 2;

//1.50 defined points in options table to make updating easier
#define OPTION_GRIFFEYE               10
#define OPTION_EXTRACT_START          11

/**
 * @brief Main in-memory SQLite database used to store file information for
 *        processing into Project VICS records.
 *
 * @see AlternativeHash
 * @see EvidenceObjects
 * @see MediaMetadata
 * @see VICSMovies
 * @see VICSMoviesRecords
 * @see VICSPics
 * @see VICSPicsRecords
 */

/**
 * @brief SQLite table containing data about video media exported from X-Ways.
 *
 * Relates to the Project VICS Media entry. Tables are split between pictures
 * and videos for ease of export.
 *
 * Columns: MediaID (INTEGER), Category (INTEGER), SHA1 (TEXT), MD5Hash (TEXT PRIMARY KEY),
 * VictimID (INT), OffenderID (INT), IsDistributed (INT), Comments (TEXT, unused),
 * Tags (TEXT, unused), Series (TEXT, unused), MediaSize (INT), RelativeFilePath (TEXT),
 * DateUpdated (INT), Timezone (INT), PreCatSource (TEXT, unused), IsSuspected (INT, unused),
 * MimeType (TEXT), PhotoDNA (TEXT).
 */
/**
 * @brief SQLite table containing data about picture media exported from X-Ways.
 *
 * Relates to the Project VICS Media entry. Tables are split between pictures
 * and videos for ease of export.
 *
 * Columns: MediaID (INTEGER), Category (INTEGER), SHA1 (TEXT), MD5Hash (TEXT PRIMARY KEY),
 * VictimID (INT), OffenderID (INT), IsDistributed (INT), Comments (TEXT, unused),
 * Tags (TEXT, unused), Series (TEXT, unused), MediaSize (INT), RelativeFilePath (TEXT),
 * DateUpdated (INT), Timezone (INT), PreCatSource (TEXT, unused), IsSuspected (INT, unused),
 * MimeType (TEXT), PhotoDNA (TEXT).
 */

/**
 * @brief SQLite table storing instances of video files extracted from X-Ways.
 *
 * Relates to the VICS Mediafile record.
 *
 * Columns: MD5Hash (TEXT), Filename (TEXT), FilePath (TEXT), Created (INT),
 * Modified (INT), Accessed (INT), Unallocated (INT), SourceID (TEXT),
 * PhysicalLocation (INT), Deleted (INT), ParentMD5 (TEXT), ParentPath (TEXT),
 * parentPhysLoc (INT).
 */

/**
 * @brief SQLite table storing instances of picture files extracted from X-Ways.
 *
 * Relates to the VICS Mediafile record.
 *
 * Columns: MD5Hash (TEXT), Filename (TEXT), FilePath (TEXT), Created (INT),
 * Modified (INT), Accessed (INT), Unallocated (INT), SourceID (TEXT),
 * PhysicalLocation (INT), Deleted (INT), ParentMD5 (TEXT), ParentPath (TEXT),
 * parentPhysLoc (INT).
 */

/**
 * @brief SQLite table retaining details of evidence objects in the case.
 *
 * Columns: ID (INT PRIMARY KEY), Name (TEXT), SourceID (TEXT), ParentID (INT),
 * FileID (INT, index of the active XML output file), selected (INT, whether the
 * item was selected for processing).
 */

/**
 * @brief SQLite table designed to store error messages encountered during processing.
 *
 * Columns: ErrorType (TEXT), FileID (INT), FileName (TEXT), FilePath (TEXT).
 */

/**
 * @brief SQLite table storing additional media metadata, implementing the VICS MEDIAMETADATA entry.
 *
 * Primary key is the combination of MD5Hash and PropertyName.
 *
 * Columns: MD5Hash (TEXT), PropertyName (TEXT), PropertyValue (TEXT).
 */

/**
 * @brief SQLite table storing additional hash types, implementing the VICS ALTERNATIVEHASH entry.
 *
 * Currently unused. Under VICS 1.3 this stored PhotoDNA; from v2.0 PhotoDNA moved to the MediaFile record.
 *
 * Columns: MD5Hash (TEXT), PropertyName (TEXT, hash type e.g. SHA256/EDK), PropertyValue (TEXT).
 */


const char* sqlCreateLastRun = "CREATE TABLE lastSettings (PicPath TEXT, VidPath TEXT, extractPics INT, extractVids INT,  ignoreCarvedWithin INT,"
                          "ignoreThumbs INT,exceptMismatch INT, exportReportTables INT, debug INT,createGriffeye INT,exportVICS INT, exportXML INT, exportCompressed INT);";


//start of functions

/**
 * @brief Callback procedure for any SQLite errors encountered.
 *
 * Outputs the error code and message to the X-Ways messages window.
 *
 * @param iErrCode SQLite error code.
 * @param zMsg     Null-terminated error message string.
 *
 * @see sqlInit
 */

static void errorLogCallback(void *, int iErrCode, const char *zMsg)
{
    wchar_t errMsg[8192];
    errMsg[0]=L'\0';
    swprintf(errMsg, 8192, L"SQLite Error code: %d\nError Msg %s",iErrCode,zMsg);
    XWF_OutputMessage(errMsg,0);
}

/**
 * @brief Initialises SQLite and registers the error log callback.
 *
 * @return 0 on success, -1 if SQLite configuration fails.
 *
 * @see errorLogCallback
 * @see XT_Prepare
 */

int sqlInit()
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

/**
 * @brief Creates the in-memory SQLite VICS database, tables, and indexes.
 *
 * Quite a long function; SQL string constants could be stored elsewhere as they are very long.
 *
 * @param sqlDB Output handle for the created in-memory SQLite database.
 * @return 0 on success, -1 if the database could not be created.
 *
 * @see XT_Prepare
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
        delete[] message;
    }
    sqlite3_free(errMsg);
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE VICSPics (MediaID INT,Category INT,SHA1 TEXT,MD5Hash TEXT PRIMARY KEY NOT NULL, VictimID INT DEFAULT NULL, OffenderID INT DEFAULT NULL,IsDistributed INT DEFAULT NULL,  Comments TEXT DEFAULT \'\', Tags TEXT DEFAULT \'\',Series TEXT DEFAULT \'\', MediaSize INT,  RelativeFilePath TEXT DEFAULT \'\',DateUpdated INT, Timezone INT, PreCatSource TEXT, IsSuspected INT, MimeType TEXT DEFAULT \'\',PhotoDNA TEXT DEFAULT\'\')", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        delete[] message;
    }
    sqlite3_free(errMsg);
    //creation of new tables for records
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE VICSPicsRecords (MD5Hash TEXT NOT NULL, FileName TEXT NOT NULL,FilePath TEXT NOT NULL,Created INT,Modified INT,Accessed INT,Unallocated INT,SourceID TEXT,PhysicalLocation INT,Deleted INT,parentMD5 TEXT,parentName TEXT,parentPath TEXT,parentPhysLoc INT, itemID INT)", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        delete[] message;
    }
    sqlite3_free(errMsg);
    //creation of new tables for records
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE VICSError (ErrorType TEXT NOT NULL, FileID INT, FileName TEXT NOT NULL,FilePath TEXT NOT NULL)", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        delete[] message;
    }
    sqlite3_free(errMsg);
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE VICSMoviesRecords (MD5Hash TEXT NOT NULL, FileName TEXT NOT NULL,FilePath TEXT NOT NULL,Created INT,Modified INT,Accessed INT,Unallocated INT,SourceID TEXT,PhysicalLocation INT,Deleted INT,parentMD5 TEXT,parentName TEXT,parentPath TEXT,parentPhysLoc INT, itemID INT)", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        delete[] message;
    }
    sqlite3_free(errMsg);
    //add indexes for speed!!!
    rc = sqlite3_exec(*sqlDB,"CREATE INDEX IndexPics on VICSPics(MD5Hash);", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        delete[] message;
    }
    sqlite3_free(errMsg);
    rc = sqlite3_exec(*sqlDB,"CREATE INDEX IndexPicsRecords on VICSPicsRecords(MD5Hash);", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        delete[] message;
    }
    sqlite3_free(errMsg);

    //table for evidence objects
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE EvidenceObjects (ID INT PRIMARY KEY NOT NULL,Name TEXT NOT NULL DEFAULT \'\', SourceID TEXT NOT NULL DEFAULT \'\', ParentID INT NOT NULL, FileID INT, selected INT DEFAULT 0)", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        delete[] message;
    }
    sqlite3_free(errMsg);

    //table for alternative hashes
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE AlternativeHash (ID INT PRIMARY KEY NOT NULL,MD5Hash TEXT NOT NULL DEFAULT \'\', HashType TEXT NOT NULL DEFAULT \'\', HashValue TEXT NOT NULL DEFAULT \'\')", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        delete[] message;
    }
    sqlite3_free(errMsg);

    //1.41 table for mediametadata
    rc = sqlite3_exec(*sqlDB,"CREATE TABLE MediaMetadata (MD5Hash TEXT NOT NULL, PropertyName TEXT NOT NULL DEFAULT \'\', PropertyValue TEXT NOT NULL DEFAULT \'\', PRIMARY KEY (MD5Hash, PropertyName));", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        delete[] message;
    }
    sqlite3_free(errMsg);
    return 0;
}

/**
 * @brief Creates an SQLite record for each evidence object in the case.
 *
 * Records are inserted via insertEvObjRecord.
 *
 * @param sqlDB Handle to the in-memory SQLite database.
 * @param evObj Handle to the evidence object whose properties are to be recorded.
 *
 * @see insertEvObjRecord
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

/**
 * @brief Checks each selected evidence object and marks its parent as selected if needed.
 *
 * Ensures that sub-objects are associated with the top-level object even when the
 * top-level object itself is not selected. For example, if AB-1 Partition 2 is
 * selected but AB-1 is not, AB-1 is marked selected for reporting purposes.
 *
 * @param sqlDB Handle to the in-memory SQLite database.
 *
 * @see checkParentSelected
 * @see setParentSelected
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

/**
 * @brief Checks the SQLite records to determine whether an evidence object is selected.
 *
 * @param sqlDB    Handle to the in-memory SQLite database.
 * @param parentID X-Ways ID of the evidence object to check.
 * @return TRUE if the evidence object is selected, FALSE otherwise.
 *
 * @see checkParentObjectsSelected
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

/**
 * @brief Sets the selected flag on the SQLite record for a given evidence object.
 *
 * Currently no return value is provided if an error occurs.
 *
 * @param sqlDB    Handle to the in-memory SQLite database.
 * @param parentID X-Ways ID of the evidence object to mark as selected.
 *
 * @see checkParentObjectsSelected
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

/**
 * @brief Inserts a SQLite record for an evidence object.
 *
 * Note: a possible memory leak may occur if an error is encountered.
 *
 * @param sqlDB  Handle to the in-memory SQLite database.
 * @param record Reference to an EvidenceProps struct containing the data to insert.
 * @return 0 on success, -1 if the query could not be prepared, -2 if the query could not be executed.
 *
 * @see checkParentObjectsSelected
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

/**
 * @brief Checks whether a file with the same offset and hash value already exists in the database.
 *
 * @param sqlDB     Handle to the in-memory SQLite database.
 * @param offset    Physical byte offset of the file on the source medium.
 * @param MD5       Wide string containing the MD5 hash of the file.
 * @param currSrcID Wide string containing the source ID of the current evidence object.
 * @param nItemID   On duplicate found, receives the X-Ways item ID of the existing record.
 * @param picture   1 if the file is a picture, 0 for video.
 * @return 1 if a duplicate exists, 0 if no matching record exists, negative on error.
 */
int checkDuplicateFile(sqlite3* sqlDB, INT64 offset, wchar_t* MD5, wchar_t* currSrcID, long* nItemID,int picture)
{
    if (extractInfo.debugSet){debugWriteDetails(*nItemID, L"checkDuplicateFile Start");}
    //Select * from VICSPicsRecords where PhysicalLocation = offset and MD5Hash = MD5
    sqlite3_stmt *statement;
    const char* tableName = (picture == 1) ? "VICSPicsRecords" : "VICSMoviesRecords";
    char sqlQuery[128] = {0};
    snprintf(sqlQuery, sizeof(sqlQuery), "SELECT itemID FROM %s WHERE MD5Hash = ? AND SourceID = ? AND PhysicalLocation = ?;", tableName);
    int rc = sqlite3_prepare_v2(sqlDB, sqlQuery, -1, &statement, NULL);
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_text16(statement, 1, MD5,       -1, SQLITE_STATIC);
        if (rc == SQLITE_OK) rc = sqlite3_bind_text16(statement, 2, currSrcID, -1, SQLITE_STATIC);
        if (rc == SQLITE_OK) rc = sqlite3_bind_int64(statement,  3, offset);
    }
    if (rc != SQLITE_OK)
    {
        sqlite3_finalize(statement);
        if (extractInfo.debugSet){debugWriteDetails(*nItemID, L"checkDuplicateFile END - Error");}
        return -1;
    }
    rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW)
    {
        *nItemID = sqlite3_column_int(statement, 0);
        sqlite3_finalize(statement);
        if (extractInfo.debugSet){debugWriteDetails(*nItemID, L"checkDuplicateFile End - Duplicate located");}
        return 1;
    }
    else
    {
        sqlite3_finalize(statement);
        if (extractInfo.debugSet){debugWriteDetails(*nItemID, L"checkDuplicateFile End - No Duplicate located");}
        return 0;
    }
}

/**
 * @brief Checks whether an MD5 hash already exists in the VICS media table.
 *
 * @param sqlDB   Handle to the in-memory SQLite database.
 * @param MD5     Wide string containing the MD5 hash to search for.
 * @param picture 1 to search the pictures table, 0 to search the movies table.
 * @return 1 if an entry was found, 0 if not found, -1 on error.
 *
 * @see createVICSRecord
 */
INT64 getVicsRecord(sqlite3* sqlDB, wchar_t* MD5, int picture)
{
    sqlite3_stmt *statement;
    const char* sqlQuery = (picture == 1)
        ? "SELECT MD5Hash FROM VICSPics WHERE MD5Hash = ?;"
        : "SELECT MD5Hash FROM VICSMovies WHERE MD5Hash = ?;";
    int rc = sqlite3_prepare_v2(sqlDB, sqlQuery, -1, &statement, NULL);
    if (rc == SQLITE_OK) rc = sqlite3_bind_text16(statement, 1, MD5, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK)
    {
        sqlite3_finalize(statement);
        return -1;
    }
    rc = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (rc == SQLITE_ROW) return 1;
    return 0;
}

static int countCallback(void *valCount,int argc, char **argv, char **azColName)
{
    int *c = (int*)valCount;
    *c = atoi(argv[0]);
    return 0;
}

/**
 * @brief Returns an array of ObjectNames for all selected root evidence objects (parentID == 0).
 *
 * @param sqlDB      Handle to the in-memory SQLite database.
 * @param retCounter Output parameter set to the number of entries in the returned array.
 * @return Newly allocated array of ObjectNames; caller is responsible for freeing.
 *
 * @see getCaseOptions
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


/**
 * @brief Updates evidence object records to use their preferred (user-supplied) source names.
 *
 * Errors are reported to the X-Ways message window; no error value is returned.
 *
 * @param sqlDB     Handle to the in-memory SQLite database.
 * @param listEvObj Array of ObjectNames containing the actual and preferred names.
 * @param noObjs    Number of entries in listEvObj.
 * @return 0 always.
 *
 * @see getCaseOptions
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

/**
 * @brief Retrieves the stored source name for a given evidence object.
 *
 * The returned buffer is allocated with new[] and must be freed by the caller.
 *
 * @param sqlDB Handle to the in-memory SQLite database.
 * @param evID  X-Ways ID of the evidence object whose source name is to be retrieved.
 * @return Newly allocated wide string containing the source name, or NULL on failure.
 *
 * @see XT_Prepare
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
            sqlite3_finalize(statement);
            return returnString;
        }
    }
    sqlite3_finalize(statement);
    return NULL;
}

/**
 * @brief Updates the XML output file number for a given evidence object ID.
 *
 * @param sqlDB  Handle to the in-memory SQLite database.
 * @param objID  X-Ways ID of the evidence object to update.
 * @param fileNo The XML output file index to associate with the object.
 * @return 0 on success, -1 on error.
 *
 * @see createC4POutput
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

/**
 * @brief Returns the XML output file index for the given evidence object.
 *
 * Called each time XT_Prepare is invoked for a new volume so the correct XML output file can be set.
 *
 * @param sqlDB Handle to the in-memory SQLite database.
 * @param objID X-Ways ID of the evidence object to look up.
 * @return The XML output file index (>=0) on success, -1 on error.
 *
 * @see XT_Prepare
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

/**
 * @brief Returns the root evidence object for a given object by traversing parent links.
 *
 * For example, AB/1 Partition 1 has a root object of AB/1. Called prior to getFileNumber.
 *
 * @param sqlDB Handle to the in-memory SQLite database.
 * @param objID X-Ways ID of the evidence object to resolve.
 * @return X-Ways ID of the root evidence object, or NULL on error.
 *
 * @see XT_Prepare
 * @see getFileNumber
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

/**
 * @brief Inserts an error record into the VICSError table.
 *
 * Looks up the file name and path for the given item ID and binds them as parameters to avoid
 * SQL injection from file names.
 *
 * @param sqlDB     Handle to the in-memory SQLite database.
 * @param errorCode Index into the errorValues array identifying the error type.
 * @param objID     X-Ways item ID of the file that caused the error.
 * @param srcText   Wide string of the current evidence source, used to build the full file path.
 * @return 0 on success, -1 if the query could not be prepared, -2 if execution failed,
 *         -3 if binding the file name failed, -4 if binding the file path failed.
 *
 * @see outputErrorStats
 */
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
        XWF_OutputMessage(sqlQuery,0);
        delete[] filePath;
        delete[] sqlQuery;
        return -1;
    }
    rc = sqlite3_bind_text16(statement,1,xName,(nameLen+1)*sizeof(wchar_t),SQLITE_STATIC);
    if (rc != SQLITE_OK)
    {
        XWF_OutputMessage(L"Error Binding file name",0);
        sqlite3_finalize(statement);
        delete[] filePath;
        delete[] sqlQuery;
        return -3;
    }
    rc = sqlite3_bind_text16(statement,2,filePath,(pathLen+1)*sizeof(wchar_t),SQLITE_STATIC);
    if (rc != SQLITE_OK)
    {
        XWF_OutputMessage(L"Error Binding file path",0);
        sqlite3_finalize(statement);
        delete[] filePath;
        delete[] sqlQuery;
        return -4;
    }
    rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE)
    {
        XWF_OutputMessage(sqlQuery,0);
        sqlite3_finalize(statement);
        delete[] filePath;
        delete[] sqlQuery;
        return -2;
    }
    sqlite3_finalize(statement);
    delete[] filePath;
    delete[] sqlQuery;
    return 0;

}

/**
 * @brief Creates the ExtractionOptions table in the options SQLite database.
 *
 * Stores minimum and maximum file size limits, output paths, and format flags for the
 * extraction run. Updated in v1.50 to include minimum file sizes.
 *
 * @param sqlDB Handle to the options SQLite database.
 * @return 0 on success, -1 if the table could not be created.
 */
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
        delete[] message;
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}

/**
 * @brief Inserts or replaces the current schema version record in the SchemaVersion table.
 *
 * @param sqlDB Handle to the options SQLite database.
 * @return 0 on success, -2 if the insert fails.
 *
 * @see createOptionsSchemaTable
 * @see updateOptionsSchema
 */
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

/**
 * @brief Creates the SchemaVersion table and inserts the current schema version record.
 *
 * @param sqlDB Handle to the options SQLite database.
 * @return 0 on success, -1 if the table could not be created, or the result of insertOptionsSchemaRecord on failure.
 *
 * @see insertOptionsSchemaRecord
 */
int createOptionsSchemaTable(sqlite3* sqlDB)
{
    char* errMsg = 0;
    int rc = sqlite3_exec(sqlDB,"CREATE TABLE SchemaVersion (version INT);", NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        delete[] message;
        sqlite3_free(errMsg);
        return -1;
    }
    return insertOptionsSchemaRecord(sqlDB);
}

/**
 * @brief Creates the lastSettings table used to persist the most recent extraction run details.
 *
 * @param sqlDB Handle to the options SQLite database.
 * @return 0 on success, -1 if the table could not be created.
 */
int createOptionsLastrunTable(sqlite3* sqlDB)
{
    char* errMsg = 0;
    int rc = sqlite3_exec(sqlDB,sqlCreateLastRun, NULL, NULL, &errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        delete[] message;
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}

/**
 * @brief Deletes all rows from the ExtractionOptions table.
 *
 * @param db Handle to the options SQLite database.
 * @return 0 on success, -1 if the DELETE statement fails.
 *
 * @see saveOptions
 */
int clearExtractionOptionsTable(sqlite3* db)
{
    char* errMsg = 0;
    const char* sqlQuery = "DELETE FROM ExtractionOptions;";
    int rc = sqlite3_exec(db,sqlQuery,NULL,NULL,&errMsg);
    if (rc!=SQLITE_OK)
    {
        wchar_t* message = new wchar_t[strlen(errMsg)+1];
        swprintf(message,L"%s\0",errMsg);
        XWF_OutputMessage(message,0);
        delete[] message;
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}

/**
 * @brief Inserts an extraction options record into the ExtractionOptions table.
 *
 * @param sqlDB  Handle to the options SQLite database.
 * @param record ExtractOptions struct containing the values to store.
 * @return 0 on success, -1 if the statement could not be prepared, -2 if execution fails.
 *
 * @see saveOptions
 * @see updateOptionsSchema
 */
int insertOptionsExtraction(sqlite3* sqlDB, ExtractOptions record)
{
    sqlite3_stmt* stmt;
    const char* sqlQuery = "INSERT INTO ExtractionOptions VALUES (?,?,?,?,?,?,?,?,?);";
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


/**
 * @brief Constructs and returns an ExtractOptions struct populated with default values.
 *
 * Default paths are derived from generateOptionsFolderString. All file type and status
 * filter flags are set to their fully-inclusive defaults.
 *
 * @return ExtractOptions struct initialised with default values.
 *
 * @see insertOptionsDefaultExtraction
 * @see loadOptions
 */
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

/**
 * @brief Inserts the default extraction options into the ExtractionOptions table.
 *
 * @param sqlDB Handle to the options SQLite database.
 * @param path  Path to the options database file (unused; present for API consistency).
 * @return The result of insertOptionsExtraction.
 *
 * @see getDefaultOptions
 * @see insertOptionsExtraction
 */
int insertOptionsDefaultExtraction(sqlite3* sqlDB, char path[])
{
    ExtractOptions exDetails = getDefaultOptions();
    return insertOptionsExtraction(sqlDB,exDetails);
}

/**
 * @brief Converts a bool to an integer (1 for true, 0 for false).
 *
 * @param value The boolean value to convert.
 * @return 1 if value is true, 0 otherwise.
 */
int boolToInt(bool value)
{
    if (value){
        return 1;
    }
    return 0;
}

/**
 * @brief Converts an integer to a bool (non-zero is true, zero is false).
 *
 * @param value The integer value to convert.
 * @return true if value is non-zero, false otherwise.
 */
int intToBool(int value)
{
    if (value!=0){
        return true;
    }
    return false;
}

/**
 * @brief Inserts the last-run extraction settings into the lastSettings table.
 *
 * @param db     Handle to the options SQLite database.
 * @param record Pointer to an ExtractionDetails struct containing the settings to persist.
 * @return 0 on success, -1 if the statement could not be prepared.
 *
 * @see readExtractionSettings
 * @see clearExtractionDetails
 */
int insertExtractionDetails(sqlite3* db, ExtractionDetails *record)
{
    const char* updateQuery = "Insert into lastSettings Values(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
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

/**
 * @brief Reads the last-run extraction settings from the lastSettings table.
 *
 * Note: the debug flag is always forced to false on read regardless of the stored value.
 *
 * @param db     Handle to the options SQLite database.
 * @param record Pointer to an ExtractionDetails struct to populate with the stored settings.
 * @return 0 on success, -1 if the statement could not be prepared.
 *
 * @see insertExtractionDetails
 */
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
        record->extractVideos = intToBool(sqlite3_column_int(stmt,3));
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

/**
 * @brief Deletes all rows from the lastSettings table.
 *
 * @param db Handle to the options SQLite database.
 * @return 0 on success, -1 if the DELETE statement fails.
 *
 * @see insertExtractionDetails
 */
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

/**
 * @brief Creates the options SQLite database file and initialises all required tables with defaults.
 *
 * Opens (or creates) the options file at path\opt.sqlite and calls the individual table-creation
 * functions, then populates ExtractionOptions with default values.
 *
 * @param path Directory path in which to create the opt.sqlite options database file.
 * @return 0 on success, -1 if the database could not be opened, -2 if ExtractionOptions table
 *         creation fails, -3 if SchemaVersion table creation fails, -4 if lastSettings table
 *         creation fails, -5 if inserting default options fails.
 *
 * @see createOptionsExtractionTable
 * @see createOptionsSchemaTable
 * @see createOptionsLastrunTable
 * @see insertOptionsDefaultExtraction
 */
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

/**
 * @brief Reads the schema version number stored in the SchemaVersion table.
 *
 * @param db Handle to the options SQLite database.
 * @return The stored schema version, or 0 if the table is empty or on error.
 *
 * @see getOptionsSchemaVersion
 */
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

/**
 * @brief Determines the effective schema version of the options database.
 *
 * Checks whether the SchemaVersion table exists. If it does not, returns 0 (legacy schema).
 * If it does, returns the value from getOptionsSchemaValue.
 *
 * @param db Handle to the options SQLite database.
 * @return The schema version number, 0 if no SchemaVersion table exists, or negative on error.
 *
 * @see getOptionsSchemaValue
 * @see loadOptions
 */
int getOptionsSchemaVersion(sqlite3* db)
{
    const char* query = "Select count(*) from SQLITE_MASTER where Tbl_name like '%SchemaVersion%';";
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

/**
 * @brief Reads extraction options from the legacy (pre-v1.50) database schema.
 *
 * Reads a reduced column set: MaxPicSize, MaxVidSize, ReportDir, Overwrite, GriffeyeDir.
 * MinPicSize and MinVidSize are not present in this schema version.
 *
 * @param db    Handle to the options SQLite database.
 * @param error Set to true if no row is found or a query error occurs.
 * @return ExtractOptions populated from the database, or a zeroed struct on error.
 *
 * @see selectSchema
 */
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
            sqlite3_finalize(statement);
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

/**
 * @brief Reads extraction options from the v1 database schema (introduced in v1.50).
 *
 * Reads all ExtractionOptions columns including MinPicSize and MinVidSize, but excluding
 * TypeStatusFlags and FileTypeFlag which were added in schema v2.
 *
 * @param db    Handle to the options SQLite database.
 * @param error Set to true if no row is found or a query error occurs.
 * @return ExtractOptions populated from the database, or a zeroed struct on error.
 *
 * @see selectSchema
 */
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
            sqlite3_finalize(statement);
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

/**
 * @brief Reads extraction options from the v2 database schema (introduced in v1.51).
 *
 * Reads all ExtractionOptions columns including TypeStatusFlags and FileTypeFlag.
 *
 * @param db    Handle to the options SQLite database.
 * @param error Set to true if no row is found or a query error occurs.
 * @return ExtractOptions populated from the database, or a zeroed struct on error.
 *
 * @see selectSchema
 */
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
            sqlite3_finalize(statement);
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

/**
 * @brief Dispatches to the appropriate schema-specific extraction function based on the schema version.
 *
 * @param db            Handle to the options SQLite database.
 * @param schemaVersion Schema version number returned by getOptionsSchemaVersion.
 * @param error         Set to true if extraction fails or an unknown schema version is encountered.
 * @return ExtractOptions populated from the database, or a zeroed struct on error.
 *
 * @see extractOldSchemaOptions
 * @see extractV1SchemaOptions
 * @see extractV2SchemaOptions
 * @see loadOptions
 */
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

/**
 * @brief Drops the ExtractionOptions, SchemaVersion, and LastSettings tables from the options database.
 *
 * SchemaVersion and LastSettings drops are not checked for errors as those tables may not exist.
 *
 * @param db Handle to the options SQLite database.
 * @return 0 on success, -1 if the ExtractionOptions table could not be dropped.
 *
 * @see updateOptionsSchema
 */
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

/**
 * @brief Fills in default values for fields that do not exist in older schema versions.
 *
 * For schema versions 1 and below, sets TypeStatusFlags and FileTypeFlag to their
 * fully-inclusive defaults.
 *
 * @param record        Pointer to the ExtractOptions struct to update.
 * @param schemaVersion The schema version from which the options were loaded.
 *
 * @see updateOptionsSchema
 */
void addDefaultValues(ExtractOptions* record, int schemaVersion)
{
    if (schemaVersion <= 1)
    {
        record->TypeStatusFlags = NOT_VERIFIED |IRRELEVANT|NOT_IN_LIST|CONFIRMED|
                                NOT_CONFIRMED|NEWLY_IDENTIFIED|MISMATCH_DETECTED;
        record->FileTypeFlag =  UNKNOWN|OK|IRREGULAR|CORRUPT;
    }
}

/**
 * @brief Migrates the options database from an older schema version to the current one.
 *
 * Drops and recreates tables as needed, fills in default values for new fields,
 * and re-inserts the provided options record under the current schema.
 *
 * @param db            Handle to the options SQLite database.
 * @param record        Pointer to the ExtractOptions to migrate and re-save.
 * @param path          Path to the options database file (reserved for future use).
 * @param schemaVersion The schema version that was loaded.
 * @return 0 on success, -1 if the tables could not be dropped.
 *
 * @see optionsDropTable
 * @see addDefaultValues
 * @see insertOptionsExtraction
 */
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

/**
 * @brief Loads extraction options from the options SQLite database at the given path.
 *
 * If the database cannot be opened, default options are returned. If the schema is outdated
 * or corrupt, the schema is migrated or reset to defaults automatically.
 *
 * @param path Path to the options SQLite database file.
 * @return ExtractOptions struct loaded from the database, or defaults on failure.
 *
 * @see getOptionsSchemaVersion
 * @see selectSchema
 * @see updateOptionsSchema
 * @see getDefaultOptions
 */
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


/**
 * @brief Saves extraction options to the options SQLite database at the given path.
 *
 * Clears the existing ExtractionOptions rows and inserts the provided options.
 *
 * @param path Path to the options SQLite database file.
 * @param opt  ExtractOptions struct containing the values to save.
 * @return 0 on success, -1 if the database could not be opened.
 *
 * @see clearExtractionOptionsTable
 * @see insertOptionsExtraction
 */
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

/**
 * @brief Outputs the count of each error type recorded in the VICSError table.
 *
 * Iterates over all known error types and queries the VICSError table for each.
 * Results are sent to the X-Ways message window; from X-Ways v19.4 the messages
 * are displayed in a separate log section.
 *
 * @param sqlDB     Handle to the in-memory SQLite database.
 * @param versionNo X-Ways version number; controls the output message flags.
 *
 * @see recordError
 */
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

/**
 * @brief Checks whether a valid SQLite database file exists at the given path.
 *
 * @param path Path to the file to test.
 * @return TRUE if the database can be opened read-only, FALSE otherwise.
 *
 * @see loadOrCreateOptions
 */
BOOL sqlDatabaseExists(char path[])
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

/**
 * @brief Saves an in-memory SQLite database to a file, or loads a file into an in-memory database.
 *
 * Uses the SQLite backup API to copy between the in-memory database and a file-based database.
 *
 * @param pInMemory Handle to the in-memory SQLite database.
 * @param zFilename Path to the file to save to or load from.
 * @param isSave    Non-zero to save pInMemory to file; zero to load file into pInMemory.
 * @return An SQLite error code; SQLITE_OK on success.
 *
 * @see writeSQLMediaRecord
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

/**
 * @brief Updates an existing VICS MediaFile record in the database, used for duplicate detection.
 *
 * Identifies the record to update by matching MD5 hash and item ID.
 *
 * @param vicsDB   Handle to the in-memory SQLite database.
 * @param record   Pointer to a VICSMediaFile struct containing the updated data.
 * @param picture  1 if the file is a picture, 0 for video.
 * @param MD5      Wide string containing the MD5 hash of the existing record to update.
 * @param dupItemID X-Ways item ID of the duplicate item to update.
 * @return 0 on success, -2 if the update statement fails to execute, -3 if preparation or a bind fails.
 *
 * @see writeSQLMediaRecord
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


/**
 * @brief Inserts a VICS MediaFile record into the appropriate database table.
 *
 * On error, the offending SQL query is output to the X-Ways message window.
 * File name and path are bound as parameters to support wide characters safely.
 *
 * @param vicsDB  Handle to the in-memory SQLite database.
 * @param record  Reference to the VICSMediaFile struct to insert.
 * @param picture 1 if the file is a picture (inserts into VICSPicsRecords), 0 for video.
 * @return 0 on success, -1 if the statement could not be prepared, -2 if execution fails,
 *         -3 if binding the file name fails, -4 if binding the file path fails.
 *
 * @see writeSQLMediaRecord
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

/**
 * @brief Inserts a VICS Media record into the appropriate database table.
 *
 * On error, the offending SQL query is output to the X-Ways message window.
 *
 * @param vicsDB  Handle to the in-memory SQLite database.
 * @param record  Reference to the VICSMedia struct to insert.
 * @param picture 1 if the file is a picture (inserts into VICSPics), 0 for video.
 * @return 0 on success, -1 if the statement could not be prepared, -2 if execution fails.
 *
 * @see writeSQLMediaRecord
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


/**
 * @brief Inserts a VICS MediaMetadata record into the MediaMetadata table.
 *
 * @param vicsDB Handle to the in-memory SQLite database.
 * @param record VICSMediaMetadata struct containing the MD5, PropertyName, and PropertyValue to insert.
 * @return 0 on success, -1 if the statement could not be prepared, -2 if execution fails.
 */
int insertMediaMetadataRecord(sqlite3* vicsDB, VICSMediaMetadata record)
{
    const char* sqlQuery = "INSERT INTO MediaMetadata VALUES (?,?,?);";
    sqlite3_stmt* statement;
    int rc = sqlite3_prepare_v2(vicsDB, sqlQuery, -1, &statement, NULL);
    if (rc != SQLITE_OK)
    {
        XWF_OutputMessage(L"Error preparing MediaMetadata insert",0);
        return -1;
    }
    rc = sqlite3_bind_text16(statement, 1, record.MD5,          -1, SQLITE_STATIC);
    if (rc == SQLITE_OK)
        rc = sqlite3_bind_text16(statement, 2, record.PropertyName,  -1, SQLITE_STATIC);
    if (rc == SQLITE_OK)
        rc = sqlite3_bind_text16(statement, 3, record.PropertyValue, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK)
    {
        XWF_OutputMessage(L"Error binding MediaMetadata values",0);
        sqlite3_finalize(statement);
        return -1;
    }
    rc = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (rc != SQLITE_DONE)
    {
        XWF_OutputMessage(L"Error inserting MediaMetadata record",0);
        return -2;
    }
    return 0;
}

/**
 * @brief Checks whether a PropertyName entry already exists for the given MD5 hash in the MediaMetadata table.
 *
 * @param database     Handle to the in-memory SQLite database.
 * @param MD5          Wide string containing the MD5 hash to search for.
 * @param PropertyName Wide string containing the metadata property name to check.
 * @return 1 if the record exists, 0 if it does not, -1 or -2 on error.
 */
int existsMediaMetadata(sqlite3* database, wchar_t* MD5, wchar_t* PropertyName)
{
    const char* sqlQuery = "SELECT count(*) FROM MediaMetadata WHERE MD5Hash = ? AND PropertyName = ?;";
    sqlite3_stmt* statement;
    int rc = sqlite3_prepare_v2(database, sqlQuery, -1, &statement, NULL);
    if (rc != SQLITE_OK)
        return -2;
    rc = sqlite3_bind_text16(statement, 1, MD5,          -1, SQLITE_STATIC);
    if (rc == SQLITE_OK)
        rc = sqlite3_bind_text16(statement, 2, PropertyName, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK)
    {
        sqlite3_finalize(statement);
        return -2;
    }
    int retVal = 0;
    rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW)
        retVal = sqlite3_column_int(statement, 0);
    else
        retVal = -1;
    sqlite3_finalize(statement);
    if (retVal <= 0) return retVal;
    return 1;
}


/**
 * @brief Returns the number of rows in a table matching the given MD5 hash value.
 *
 * @param database  Handle to the in-memory SQLite database.
 * @param tablename Name of the table to query.
 * @param hashValue Wide string containing the MD5 hash to match against.
 * @return Number of matching rows (>=0), -1 if the query step fails, -2 if preparation fails.
 *
 * @see returnMediaFileRecords
 */
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

/**
 * @brief Returns the total number of rows in a given table.
 *
 * @param database  Handle to the in-memory SQLite database.
 * @param tablename Name of the table to count rows in.
 * @return Number of rows (>=0), -1 if the query step fails, -2 if preparation fails.
 *
 * @see returnMediaRecords
 * @see returnMediaFileRecords
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

/**
 * @brief Retrieves all MediaFile records matching an MD5 hash from the picture or video table.
 *
 * The statement must be blank (unused or previously finalised). The calling function is
 * responsible for finalising the statement after use.
 *
 * @param database  Handle to the in-memory SQLite database.
 * @param statement Output parameter set to the prepared and stepped SQLite statement.
 * @param picture   1 to query VICSPicsRecords, 0 to query VICSMoviesRecords.
 * @param hashValue Wide string containing the MD5 hash to search for.
 * @return Number of matching records (>0), 0 if none found, -1 on row-count error,
 *         -2 if the query step fails, -3 if statement preparation fails.
 *
 * @see extractIntoVicsRecord
 * @see getRowCount
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

/**
 * @brief Retrieves all records from the VICSPics or VICSMovies table.
 *
 * The statement must be blank (unused or previously finalised). The calling function is
 * responsible for finalising the statement after use.
 *
 * @param database  Handle to the in-memory SQLite database.
 * @param statement Output parameter set to the prepared and stepped SQLite statement.
 * @param picture   1 to query VICSPics, 0 to query VICSMovies.
 * @return Number of records (>0), 0 if the table is empty, or negative on error.
 *
 * @see writeRecords
 * @see getRowCount
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

/**
 * @brief Retrieves all MediaMetadata records for a given MD5 hash.
 *
 * The statement must be blank (unused or previously finalised). The calling function is
 * responsible for finalising the statement after use. Returns immediately if no records exist.
 *
 * @param database  Handle to the in-memory SQLite database.
 * @param statement Output parameter set to the prepared and stepped SQLite statement.
 * @param hashValue Wide string containing the MD5 hash to match against.
 * @return Number of matching records (>0), 0 if none exist, or negative on error.
 *
 * @see writeRecords
 * @see getRowCount
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
    int rc = sqlite3_prepare_v2(database,sqlQuery,-1,statement,NULL);
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

