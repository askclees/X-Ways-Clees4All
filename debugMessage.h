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


/* Constants: Item Error Types

    REPORT_ERROR_TYPE           -   0
    REPORT_UNKNOWN_FILESIZE     -   1
    REPORT_NOHASH               -   2
    REPORT_FILESIZE_MISMATCH    -   3
    REPORT_FILEOPEN_ERROR       -   4
    REPORT_EXCLUDED_PARENT      -   5
    REPORT_EXCLUDED_DUPLICATE   -   6
    REPORT_EXCLUDED_FILESIZE    -   7

 */


#define REPORT_ERROR_TYPE               0
#define REPORT_UNKNOWN_FILESIZE         1
#define REPORT_NOHASH                   2
#define REPORT_FILESIZE_MISMATCH        3
#define REPORT_FILEOPEN_ERROR           4
#define REPORT_EXCLUDED_PARENT          5
#define REPORT_EXCLUDED_DUPLICATE       6
#define REPORT_EXCLUDED_FILESIZE        7
#define REPORT_EXCLUDED_THUMBNAIL       8
//1.51 added new errors
#define REPORT_EXCLUDED_TYPESTATUS      9
#define REPORT_EXCLUDED_FILECONSISTENCY 10

//set up functions
int startDebugLog(char* filePath);
int endDebugLog();

//debug log functions
int debugWriteDetails(const char* message);
extern int debugWriteDetails(FILE* f,LONG nItemID, const wchar_t* module);
extern int debugWriteDetails(LONG nItemID, const wchar_t* module);
int debugWriteDetails(LONG nItemID, const wchar_t* module,const wchar_t* message,varList varArgs);


//error message functions
extern void outputErrorMessage(const wchar_t* errMsg, LONG nItemID);
void outputErrorMessage(const wchar_t* errMsg);
void outputErrorMessage(const wchar_t* errMsg, wchar_t* detail);

//functions for adding items to error report tables
void errorRaised(LONG nItemID,int errorCode);
void errorReport();

#endif // DEBUGMESSAGE_H_INCLUDED
