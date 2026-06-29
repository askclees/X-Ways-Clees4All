#ifndef SQLFUNCTIONS_H_INCLUDED
#define SQLFUNCTIONS_H_INCLUDED

#include "VICS.h"

/** @brief Creates the in-memory SQLite VICS database, tables, and indexes. */
extern int setupVics(sqlite3** sqlDB);
/** @brief Checks whether an MD5 hash already exists in the VICS media table. */
extern INT64 getVicsRecord(sqlite3* sqlDB, wchar_t* MD5, int picture);
/** @brief Inserts a SQLite record for an evidence object. */
extern int insertEvObjRecord(sqlite3* sqlDB, EvidenceProps &record);
/** @brief Creates an SQLite record for each evidence object in the case. */
extern void createSQLNameList(sqlite3* sqlDB, HANDLE evObj);
/** @brief Returns the parent evidence object ID for a given evidence object (currently unused). */
extern DWORD getEvObjParent(sqlite3* sqlDB, DWORD evObj);
/** @brief Updates the XML output file index for a given evidence object (currently unused). */
extern int updateEvObjFILE(sqlite3* sqlDB, DWORD evObj, int fileNo);
/** @brief Returns an array of ObjectNames for all selected root evidence objects. */
extern ObjectNames* retrieveEvidenceNames(sqlite3* sqlDB,int *retCounter);
/** @brief Updates evidence object records to use their preferred source names. */
extern int updateEvidenceNames(sqlite3* sqlDB,ObjectNames* listEvObj, int noObjs);
/** @brief Checks each selected evidence object and marks its parent as selected if needed. */
extern void checkParentObjectsSelected(sqlite3* sqlDB);
/** @brief Retrieves the stored source name for a given evidence object. */
extern wchar_t* getSourceIDName(sqlite3* sqlDB, DWORD evID);
/** @brief Updates the XML output file number for a given evidence object ID. */
extern int updateFileNumber(sqlite3* sqlDB,DWORD objID,int fileNo);
/** @brief Returns the XML output file index for the given evidence object. */
extern int getFileNumber(sqlite3* sqlDB,DWORD objID);
/** @brief Returns the root evidence object for a given object by traversing parent links. */
extern DWORD getRootObj(sqlite3* sqlDB,DWORD objID);
/** @brief Inserts an error record into the VICSError table. */
extern int recordError(sqlite3* sqlDB,int errorCode, LONG objID, LPWSTR srcText);
/** @brief Creates the options SQLite database file and initialises all required tables with defaults. */
extern int sqlCreateOptions(char path[]);
/** @brief Checks whether a valid SQLite database file exists at the given path. */
extern BOOL sqlDatabaseExists(char path[]);
/** @brief Loads extraction options from the options SQLite database at the given path. */
extern ExtractOptions loadOptions(char path[]);
/** @brief Saves extraction options to the options SQLite database at the given path. */
int saveOptions(char path[], ExtractOptions opt);
/** @brief Outputs the count of each error type recorded in the VICSError table. */
void outputErrorStats(sqlite3* sqlDB,WORD versionNo);
/** @brief Saves an in-memory SQLite database to a file, or loads a file into an in-memory database. */
int loadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave);
/** @brief Initialises SQLite and registers the error log callback. */
int sqlInit();

/** @brief Inserts a VICS MediaFile record into the appropriate database table. */
int insertMediaFileRecord(sqlite3* vicsDB, VICSMediaFile &record, int picture);
/** @brief Inserts a VICS Media record into the appropriate database table. */
int insertMediaRecord(sqlite3* vicsDB, VICSMedia &record, int picture);
/** @brief Inserts a VICS MediaMetadata record into the MediaMetadata table. */
int insertMediaMetadataRecord(sqlite3* vicsDB, VICSMediaMetadata record);

/** @brief Retrieves all MediaFile records matching an MD5 hash from the picture or video table. */
int returnMediaFileRecords(sqlite3* database, sqlite3_stmt** statement, int picture, wchar_t* hashValue);
/** @brief Retrieves all records from the VICSPics or VICSMovies table. */
int returnMediaRecords(sqlite3* database, sqlite3_stmt** statement, int picture);
/** @brief Retrieves all MediaMetadata records for a given MD5 hash. */
int returnMediaMetadataRecords(sqlite3* database, sqlite3_stmt** statement, wchar_t* hashValue);
/** @brief Checks whether a PropertyName entry already exists for the given MD5 hash in the MediaMetadata table. */
int existsMediaMetadata(sqlite3* database, wchar_t* MD5, wchar_t* PropertyName);

/** @brief Checks whether a file with the same offset and hash value already exists in the database. */
int checkDuplicateFile(sqlite3* sqlDB, INT64 offset, wchar_t* MD5, wchar_t* currSrcID, long* nItemID, int picture);
/** @brief Updates an existing VICS MediaFile record in the database, used for duplicate detection. */
int updateMediaFileRecord(sqlite3* vicsDB, VICSMediaFile* record, int picture, wchar_t* MD5, LONG dupItemID);

/** @brief Inserts the last-run extraction settings into the lastSettings table. */
int insertExtractionDetails(sqlite3* db, ExtractionDetails* record);
/** @brief Deletes all rows from the lastSettings table. */
int clearExtractionDetails(sqlite3* db);
/** @brief Reads the last-run extraction settings from the lastSettings table. */
int readExtractionSettings(sqlite3* db, ExtractionDetails *record);


#endif // SQLFUNCTIONS_H_INCLUDED
