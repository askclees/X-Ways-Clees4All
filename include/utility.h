#ifndef UTILITY_H_INCLUDED
#define UTILITY_H_INCLUDED

/** @brief Returns TRUE if a file exists at the given narrow-character path. */
BOOL ifFileExists(const char* path);
/** @brief Returns TRUE if a directory exists at the given path. */
BOOL dirExists(LPCTSTR szPath);
/** @brief Returns TRUE if a file exists at the given wide-character path. */
BOOL ifFileExistsW(const wchar_t* path);
/** @brief Returns TRUE if a directory exists at the given wide-character path. */
BOOL dirExistsW(LPCWSTR szPath);
/** @brief Converts a wide string to a newly allocated UTF-8 char buffer; caller must delete[]. */
char* convertWideToChar(const wchar_t* wString);
/** @brief Escapes a UTF-8 string for embedding in a JSON string literal; returns a newly allocated buffer the caller must delete[]. */
char* jsonEscapeString(const char* input);
/** @brief Converts a FILETIME to an INT64 (100-nanosecond intervals since 1601-01-01). */
INT64 Filetime2INT64(FILETIME &inFT);
/** @brief Reallocates a wide-character buffer to a larger size, copying existing content. */
wchar_t* extendBuffer(wchar_t* currBuffer, INT64 currSize, INT64 newSize);
/** @brief Returns the size in bytes of the file at the given path, or -1 on error. */
INT64 getFileSize(const char* filePath);
/** @brief Returns the filename of the supported Griffeye CLI executable found in @p folder, or NULL if none is present (or @p folder is empty). */
const char* findGriffeyeExe(const wchar_t* folder);

#endif // UTILITY_H_INCLUDED
