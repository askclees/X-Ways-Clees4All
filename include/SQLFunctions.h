#ifndef SQLFUNCTIONS_H_INCLUDED
#define SQLFUNCTIONS_H_INCLUDED

#include "VICS.h"

extern int setupVics(sqlite3** sqlDB);
extern INT64 getVicsRecord(sqlite3* sqlDB, wchar_t* MD5, int picture);
extern int insertEvObjRecord(sqlite3* sqlDB, EvidenceProps &record);
extern void createSQLNameList(sqlite3* sqlDB, HANDLE evObj);
extern DWORD getEvObjParent(sqlite3* sqlDB, DWORD evObj);
extern int updateEvObjFILE(sqlite3* sqlDB, DWORD evObj, int fileNo);
extern ObjectNames* retrieveEvidenceNames(sqlite3* sqlDB,int *retCounter);
extern int updateEvidenceNames(sqlite3* sqlDB,ObjectNames* listEvObj, int noObjs);
extern void checkParentObjectsSelected(sqlite3* sqlDB);
extern wchar_t* getSourceIDName(sqlite3* sqlDB, DWORD evID);
extern int updateFileNumber(sqlite3* sqlDB,DWORD objID,int fileNo);
extern int getFileNumber(sqlite3* sqlDB,DWORD objID);
extern DWORD getRootObj(sqlite3* sqlDB,DWORD objID);
extern int recordError(sqlite3* sqlDB,int errorCode, LONG objID, LPWSTR srcText);
extern int sqlCreateOptions(char path[]);
extern BOOL SQLDatabaseExists(char path[]);
extern ExtractOptions loadOptions(char path[]);
int saveOptions(char path[], ExtractOptions opt);
void outputErrorStats(sqlite3* sqlDB,WORD versionNo);
int loadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave);
int SQLInit();

//inserting records
int insertMediaFileRecord(sqlite3* vicsDB, VICSMediaFile &record, int picture);
int insertMediaRecord(sqlite3* vicsDB, VICSMedia &record, int picture);
//1.41 added media metadata
int insertMediaMetadataRecord(sqlite3* vicsDB, VICSMediaMetadata record);

//1.41 new functions
int returnMediaFileRecords(sqlite3* database, sqlite3_stmt** statement, int picture, wchar_t* hashValue);
int returnMediaRecords(sqlite3* database, sqlite3_stmt** statement, int picture);
int returnMediaMetadataRecords(sqlite3* database, sqlite3_stmt** statement, wchar_t* hashValue);
int existsMediaMetadata(sqlite3* database, wchar_t* MD5, wchar_t* PropertyName);

//1.50 added check for duplicate function
int checkDuplicateFile(sqlite3* sqlDB, INT64 offset, wchar_t* MD5, wchar_t* currSrcID, long* nItemID, int picture);
int updateMediaFileRecord(sqlite3* vicsDB, VICSMediaFile* record, int picture, wchar_t* MD5, LONG dupItemID);

//1.50 - new functions to insert last run settings
int insertExtractionDetails(sqlite3* db, ExtractionDetails* record);
int clearExtractionDetails(sqlite3* db);
int readExtractionSettings(sqlite3* db, ExtractionDetails *record);


#endif // SQLFUNCTIONS_H_INCLUDED
