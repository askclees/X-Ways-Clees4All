#ifndef FILEOUTPUT_H_INCLUDED
#define FILEOUTPUT_H_INCLUDED


//file handling errors
/* Constants: File Handling Errors
    SUCCESS             -   0
    ERROR_FILE_OPEN     -   1
    FILE_ERROR_SIZE     -   2
    FILE_ERROR_FLUSH    -   3
*/
#define SUCCESS             0
#define ERROR_FILE_OPEN     1
#define FILE_ERROR_SIZE     2
#define FILE_ERROR_FLUSH    3

/* Constants: Return Errors
    RETERR_FILE_OPEN        -   100
    RETERR_FILE_READ        -   101
    RETERR_SIZE_MISMATCH    -   102
*/

//possible return values
#define RETERR_FILE_OPEN        100
#define RETERR_FILE_READ        101
#define RETERR_SIZE_MISMATCH    102



int writeOutputFile(LONG nItemID,int picFile,wchar_t* fileName, INT64 fileSize,HANDLE hdlCurrVol);
int generateRelativeFilePathVICS(char*buffer, int sizeBuffer,wchar_t* fileName);
int generateRelativeFilePath(char*buffer, int sizeBuffer,wchar_t* fileName);
#endif // FILEOUTPUT_H_INCLUDED
