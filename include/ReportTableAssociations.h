#ifndef REPORTTABLEASSOCIATIONS_H_INCLUDED
#define REPORTTABLEASSOCIATIONS_H_INCLUDED

#include "windows.h"
#include <wchar.h>

struct reportTableEntry{
    LONG reportTableID=-1;
    wchar_t* reportTableName=nullptr;
    bool userCreated=false;
};

//1.50 structure to contain all user created tables and details of key report tables
struct ReportTableDetails{
    LONG thumbnailDiscrepancy = -1;
    LONG thumbnailDamaged = -1;
    LONG maxTables = 1024;
    LONG numTables = 0;
    reportTableEntry entries[1024];
};

struct ReportTableList{
    int numEntries=0;
    wchar_t entries[128][128]={0};
};

int identifyReportTables();
bool isReportTableIDThumbnailMismatch(LONG tblID);
void clearReportTableDetails();
bool containsThumbnailMismatchTable(wchar_t* buffer, int bufferLen);
wchar_t* retrieveUserReportTableAssociations(LONG nItemID);

#endif // REPORTTABLEASSOCIATIONS_H_INCLUDED
