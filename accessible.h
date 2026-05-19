#ifndef ACCESSIBLE_H_INCLUDED
#define ACCESSIBLE_H_INCLUDED

#include "X-Tension.h"

#define ACCESSIBLE              0x00
#define INACCESSIBLE_DELETED    0x01
#define INACCESSIBLE_PATH       0x02
#define INACCESSIBLE_FILENAME   0x04


struct FilterInfo
{
    int noPathFilters = 0;
    wchar_t* FilterPaths[128];
    int noNameFilters = 0;
    wchar_t* FilterNames[128];
};

extern int checkAccessible(LONG nItemID);
extern int AddPath(LPWSTR pathStr);
extern int AddPathList(FILE* filterFile);


#endif // ACCESSIBLE_H_INCLUDED
