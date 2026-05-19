#ifndef UTILITY_H_INCLUDED
#define UTILITY_H_INCLUDED

//existence of files functions
BOOL ifFileExists(char* path);
BOOL DirExists(LPCTSTR szPath);
BOOL ifFileExistsW(wchar_t* path);
BOOL DirExistsW(LPWSTR szPath);
char* convertWideToChar(wchar_t* wString);
INT64 Filetime2INT64(FILETIME &inFT);
wchar_t* extendBuffer(wchar_t* currBuffer, INT64 currSize, INT64 newSize);
INT64 FileSize(const char* filePath);

#endif // UTILITY_H_INCLUDED
