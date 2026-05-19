#ifndef DEBUGMESSAGE_H_INCLUDED
#define DEBUGMESSAGE_H_INCLUDED

struct varEntry
{
    char type;
    int varLen;
    void* varData;
};

struct varList
{
    int noVars=0;
    varEntry entries[16];
};


/** @brief Item was of an unrecognised type. */
#define REPORT_ERROR_TYPE               0
/** @brief Item file size could not be determined. */
#define REPORT_UNKNOWN_FILESIZE         1
/** @brief No hash was computed for the item. */
#define REPORT_NOHASH                   2
/** @brief Bytes written to archive did not match the expected file size. */
#define REPORT_FILESIZE_MISMATCH        3
/** @brief Item handle could not be opened. */
#define REPORT_FILEOPEN_ERROR           4
/** @brief Item excluded because a parent item was excluded. */
#define REPORT_EXCLUDED_PARENT          5
/** @brief Item excluded as a duplicate (same hash already written). */
#define REPORT_EXCLUDED_DUPLICATE       6
/** @brief Item excluded because its file size is outside the configured limits. */
#define REPORT_EXCLUDED_FILESIZE        7
/** @brief Item excluded because it is a thumbnail embedded in an image. */
#define REPORT_EXCLUDED_THUMBNAIL       8
/** @brief Item excluded due to its file type status. */
#define REPORT_EXCLUDED_TYPESTATUS      9
/** @brief Item excluded due to file format consistency check failure. */
#define REPORT_EXCLUDED_FILECONSISTENCY 10

//set up functions
int startDebugLog(const char* filePath);
int endDebugLog();

//debug log functions
int debugWriteDetails(const char* message);
int debugWriteDetails(FILE* f,LONG nItemID, const wchar_t* module);
int debugWriteDetails(LONG nItemID, const wchar_t* module);
int debugWriteDetails(LONG nItemID, const wchar_t* module,const wchar_t* message,varList varArgs);


//error message functions
void outputErrorMessage(const wchar_t* errMsg, LONG nItemID);
void outputErrorMessage(const wchar_t* errMsg);
void outputErrorMessage(const wchar_t* errMsg, wchar_t* detail);

//functions for adding items to error report tables
void errorRaised(LONG nItemID,int errorCode);
void errorReport();

#endif // DEBUGMESSAGE_H_INCLUDED
