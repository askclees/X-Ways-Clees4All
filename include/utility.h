#ifndef UTILITY_H_INCLUDED
#define UTILITY_H_INCLUDED

BOOL ifFileExists(const char* path);
BOOL DirExists(LPCTSTR szPath);
BOOL ifFileExistsW(const wchar_t* path);
BOOL DirExistsW(LPCWSTR szPath);
char* convertWideToChar(const wchar_t* wString);
INT64 Filetime2INT64(FILETIME &inFT);
wchar_t* extendBuffer(wchar_t* currBuffer, INT64 currSize, INT64 newSize);
INT64 FileSize(const char* filePath);

#endif // UTILITY_H_INCLUDED
