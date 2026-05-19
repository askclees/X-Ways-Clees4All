
#include <windows.h>

/** @brief Number of nanoseconds in a second. */
#define Nano2Seconds 10000000LL

/** @brief Difference between Windows FILETIME epoch and Unix epoch in seconds. */
#define EpochDifference 11644473600LL

// Utility functions called by various modules within the X-Tension

/**
 * @brief Checks whether a path exists and refers to a directory.
 *
 * @param szPath Null-terminated path to check.
 * @return       TRUE if the path exists and is a directory, FALSE otherwise.
 */
BOOL DirExists(LPCTSTR szPath)
{
    DWORD dwAttrib = GetFileAttributes(szPath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

/**
 * @brief Checks whether a file exists at the given path.
 *
 * @param path Null-terminated path to the file.
 * @return     TRUE if the file exists, FALSE otherwise.
 */
BOOL ifFileExists(const char* path)
{
    DWORD chkAttributes = GetFileAttributes(path);
    return (chkAttributes != INVALID_FILE_ATTRIBUTES);
}

/**
 * @brief Checks whether a path exists and refers to a directory (wide-character version).
 *
 * @param szPath Null-terminated wide path to check.
 * @return       TRUE if the path exists and is a directory, FALSE otherwise.
 */
BOOL DirExistsW(LPCWSTR szPath)
{
    DWORD dwAttrib = GetFileAttributesW(szPath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

/**
 * @brief Checks whether a file exists at the given path (wide-character version).
 *
 * @param path Null-terminated wide path to the file.
 * @return     TRUE if the file exists, FALSE otherwise.
 */
BOOL ifFileExistsW(const wchar_t* path)
{
    DWORD chkAttributes = GetFileAttributesW(path);
    return (chkAttributes != INVALID_FILE_ATTRIBUTES);
}

/**
 * @brief Converts a Windows FILETIME integer to a Unix timestamp.
 *
 * @param fTime FILETIME value expressed as a 64-bit integer.
 * @return      Equivalent Unix timestamp in seconds since the Unix epoch.
 */
INT64 Filetime2Unix(INT64 fTime)
{
    INT64 temp;
    temp = fTime / Nano2Seconds;
    temp = temp - EpochDifference;
    return temp;
}

/**
 * @brief Converts a wide-character string to a UTF-8 char string.
 *
 * The returned buffer is allocated with new[] and must be freed by the caller
 * using delete[].
 *
 * @param wString Null-terminated wide-character string to convert.
 * @return        Newly allocated null-terminated UTF-8 string.
 */
char* convertWideToChar(const wchar_t* wString)
{
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wString, -1, NULL, 0, NULL, NULL);
    char* retStr = new char[bufferSize];
    WideCharToMultiByte(CP_UTF8, 0, wString, -1, retStr, bufferSize, NULL, NULL);
    return retStr;
}

/**
 * @brief Converts a FILETIME structure to a 64-bit integer.
 *
 * @param inFT FILETIME structure to convert.
 * @return     64-bit integer representation of the FILETIME value.
 */
INT64 Filetime2INT64(FILETIME &inFT)
{
    return ((INT64(inFT.dwHighDateTime)<<32) | (INT64(inFT.dwLowDateTime)));
}

/**
 * @brief Grows a wide-character buffer, copying existing content into the new allocation.
 *
 * The old buffer is freed. The caller takes ownership of the returned buffer and
 * must free it with delete[].
 *
 * @param currBuffer Existing buffer to copy from; freed by this function.
 * @param currSize   Number of wide characters currently in @p currBuffer.
 * @param newSize    Number of wide characters to allocate for the new buffer.
 * @return           Newly allocated buffer of @p newSize wide characters.
 */
wchar_t* extendBuffer(wchar_t* currBuffer, INT64 currSize, INT64 newSize)
{
    wchar_t* retBuffer = new wchar_t[newSize];
    memcpy(retBuffer, currBuffer, currSize * sizeof(wchar_t));
    delete[] currBuffer;
    return retBuffer;
}

/**
 * @brief Returns the size of a file in bytes.
 *
 * @param filePath Null-terminated path to the file.
 * @return         File size in bytes, or -1 if the attributes could not be retrieved.
 */
INT64 FileSize(const char* filePath)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(filePath, GetFileExInfoStandard, &fad))
        return -1;
    LARGE_INTEGER size;
    size.HighPart = fad.nFileSizeHigh;
    size.LowPart = fad.nFileSizeLow;
    return size.QuadPart;
}
