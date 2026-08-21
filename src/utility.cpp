
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cwchar>

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
BOOL dirExists(LPCTSTR szPath)
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
BOOL dirExistsW(LPCWSTR szPath)
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
INT64 filetime2Unix(INT64 fTime)
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
 * @brief Escapes a UTF-8 string for embedding inside a JSON string literal.
 *
 * Escapes backslash, double-quote, and control characters per the JSON spec.
 * Bytes >= 0x80 (UTF-8 continuation/lead bytes) are passed through unchanged.
 *
 * The returned buffer is allocated with new[] and must be freed by the caller
 * using delete[].
 *
 * @param input Null-terminated UTF-8 string to escape.
 * @return      Newly allocated null-terminated escaped string.
 */
char* jsonEscapeString(const char* input)
{
    size_t len = strlen(input);
    char* out = new char[len * 6 + 1]; // worst case: every byte -> \u00XX
    size_t o = 0;
    for (size_t i = 0; i < len; ++i)
    {
        unsigned char c = (unsigned char)input[i];
        switch (c)
        {
            case '\"': out[o++] = '\\'; out[o++] = '\"'; break;
            case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
            case '\b': out[o++] = '\\'; out[o++] = 'b';  break;
            case '\f': out[o++] = '\\'; out[o++] = 'f';  break;
            case '\n': out[o++] = '\\'; out[o++] = 'n';  break;
            case '\r': out[o++] = '\\'; out[o++] = 'r';  break;
            case '\t': out[o++] = '\\'; out[o++] = 't';  break;
            default:
                if (c < 0x20)
                    o += sprintf(out + o, "\\u%04x", c);
                else
                    out[o++] = (char)c;
                break;
        }
    }
    out[o] = '\0';
    return out;
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
INT64 getFileSize(const char* filePath)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(filePath, GetFileExInfoStandard, &fad))
        return -1;
    LARGE_INTEGER size;
    size.HighPart = fad.nFileSizeHigh;
    size.LowPart = fad.nFileSizeLow;
    return size.QuadPart;
}

/**
 * @brief Detects which supported Griffeye CLI executable is present in a folder.
 *
 * @param folder Wide string path to the Griffeye installation directory; may be empty.
 * @return       Filename of the found executable, or NULL if @p folder is empty or
 *               neither supported executable is present.
 */
const char* findGriffeyeExe(const wchar_t* folder)
{
    if (folder == NULL || folder[0] == L'\0') return NULL;
    //stay in wide chars throughout so this works for non-ASCII install paths - a narrow
    //conversion here (via snprintf/fopen) would depend on the current ANSI/OEM codepage
    bool hasSlash = (folder[wcslen(folder)-1] == L'\\');
    wchar_t check[MAX_PATH];
    swprintf(check, MAX_PATH, hasSlash ? L"%lsanalyze-cli.exe" : L"%ls\\analyze-cli.exe", folder);
    if (FILE* f = _wfopen(check, L"r")) { fclose(f); return "analyze-cli.exe"; }
    swprintf(check, MAX_PATH, hasSlash ? L"%lsmagnet-griffeye-cli.exe" : L"%ls\\magnet-griffeye-cli.exe", folder);
    if (FILE* f = _wfopen(check, L"r")) { fclose(f); return "magnet-griffeye-cli.exe"; }
    return NULL;
}
