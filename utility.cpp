
#include <windows.h>

//used for converting filetime to UNIX times

/*Constant: Nano2Seconds
    Number of nanoseconds in a second

  Constant: EpochDifference
    Difference between FILETIME and Unix Epoch timestamps

*/
#define Nano2Seconds 10000000LL
#define EpochDifference 11644473600LL

/* Section:
    Utility functions that are called by various modules within the X-Tension
*/

/*Function: DirExists

   Utility function to determine if directory exists

    Parameters:

        LPCTSTR szPath - Path to directory to be checked. Should be NULL terminated

    Returns:
        BOOL    -   True if path exists and is a directory, otherwise false

*/

BOOL DirExists(LPCTSTR szPath)
{
    DWORD dwAttrib = GetFileAttributes(szPath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

/*Function: ifFileExists

   Utility function to determine if file exists

    Parameters:

        LPCTSTR szPath - Path to file to be checked. Should be NULL terminated

    Returns:
        BOOL    -   True if file exists, otherwise false

*/

BOOL ifFileExists(char* path)
{
    DWORD chkAttributes = GetFileAttributes(path);
    return (chkAttributes != INVALID_FILE_ATTRIBUTES);
}

/*Function: DirExistsW

   Utility function to determine if directory exists. Wide character version of <DirExists>

    Parameters:

        LPWSTR szPath - Path to directory to be checked. Should be NULL terminated

    Returns:
        BOOL    -   True if path exists and is a directory, otherwise false

*/

BOOL DirExistsW(LPWSTR szPath)
{
    DWORD dwAttrib = GetFileAttributesW(szPath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

/*Function: ifFileExistsW

   Utility function to determine if file exists. Wide character version of <ifFileExists>

    Parameters:

        wchar_t* path   - Path to file to be checked. Should be NULL terminated

    Returns:
        BOOL    -   True if file exists, otherwise false

*/

BOOL ifFileExistsW(wchar_t* path)
{
    DWORD chkAttributes = GetFileAttributesW(path);
    return (chkAttributes != INVALID_FILE_ATTRIBUTES);
}


/*Function: Filetime2Unix

   Utility Function that converts a FILETIME to a Unix Timestamp

    Parameters:

        INT64 fTime - FILETIME to be converted

    Returns:
        INT64 - Unix timestamp equivalent.

*/

INT64 Filetime2Unix(INT64 fTime)
{
    INT64 temp;
    temp = fTime / Nano2Seconds;
    temp = temp - EpochDifference;
    return temp;
}

/*Function: convertWideToChar
    Utility function to safely convert Wide characters to a UTF-8 string
    Takes a Wide Character parameter - must be NULL terminated
    Implemented in version 1.38

    Parameters:
        wchar_t* wString    -   Wide character string that is to be converted to UTF-8

    Returns:
        char* of UTF-8 interpretation of Wide Character String
*/

char* convertWideToChar(wchar_t* wString)
{
    int bufferSize = WideCharToMultiByte(CP_UTF8,0,wString, -1, NULL, 0, NULL, NULL );
    char* retStr = new char[bufferSize];
    int retVal = WideCharToMultiByte(CP_UTF8,0,wString, -1, retStr, bufferSize, NULL, NULL );
    return retStr;
}

/*Function: Filetime2INT64
    Utility Function to convert a FILETIME to 64 bit Integer

    Returns:
        INT64 - value of FILETIME

*/


INT64 Filetime2INT64(FILETIME &inFT)
{
    return ((INT64(inFT.dwHighDateTime)<<32) | (INT64(inFT.dwLowDateTime)));
}



/*Function: extendBuffer
    Utility Function to convert a FILETIME to 64 bit Integer

    Returns:
        wchar_t* - new buffer of size newSize

*/
wchar_t* extendBuffer(wchar_t* currBuffer, INT64 currSize, INT64 newSize)
{
    wchar_t* retBuffer = new wchar_t[newSize];
    memcpy(retBuffer,currBuffer,currSize* sizeof(wchar_t));
    delete[] currBuffer;
    return retBuffer;
}

/*Function: FileSize
    Utility function to return size of a file given its path

    Added in 1.50

    Parameters:
        const char* filePath    -   Pointer to Null terminated path

    Returns:
        int64_t -   Size of file
        -1      -   Error
*/

INT64 FileSize(const char* filePath)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(filePath, GetFileExInfoStandard,&fad))
        return -1;
    LARGE_INTEGER size;
    size.HighPart = fad.nFileSizeHigh;
    size.LowPart = fad.nFileSizeLow;
    return size.QuadPart;
}
