#include "ReportTableAssociations.h"
#include <wchar.h>
#include "X-Tension.h"

//1.50 added report table structure
ReportTableDetails reportEntries;

#define THUMBNAIL_DISCREPANCY   1
#define THUMBNAIL_NOTABLE       2

int addReportTableEntry(wchar_t* tblName, LONG tblID, bool userCreated)
{
    reportEntries.entries[reportEntries.numTables].reportTableName = new wchar_t[wcslen(tblName)+1];
    wcsncpy(reportEntries.entries[reportEntries.numTables].reportTableName, tblName,wcslen(tblName)+1);
    reportEntries.entries[reportEntries.numTables].reportTableID = tblID;
    reportEntries.entries[reportEntries.numTables].userCreated = userCreated;
    reportEntries.numTables++;
    return 0;
}

void setThumbnailReportTableNumber(LONG tblID, int type)
{
    if (type==THUMBNAIL_DISCREPANCY){
        reportEntries.thumbnailDiscrepancy = tblID;
    }
    else if (type == THUMBNAIL_NOTABLE){
        reportEntries.thumbnailDamaged = tblID;
    }
}

void checkReportTableThumbnailMismatch(wchar_t* tblName, LONG tblID, bool userCreated)
{
    if (wcscmp(tblName,L"Thumbnail discrepancy")==0){
        setThumbnailReportTableNumber(tblID,THUMBNAIL_DISCREPANCY);
    }
    else if (wcscmp(tblName,L"Thumbnail notable (data corrupt/incomplete)")==0){
        setThumbnailReportTableNumber(tblID,THUMBNAIL_NOTABLE);
    }
}


void clearReportTableDetails()
{
    for (int i=0;i<reportEntries.numTables;i++){
        if (reportEntries.entries[i].reportTableName != nullptr){
            delete[] reportEntries.entries[i].reportTableName;
        }
    }
    reportEntries.numTables=0;
    reportEntries.thumbnailDamaged = -1;
    reportEntries.thumbnailDiscrepancy = -1;
}

bool isReportTableIDThumbnailMismatch(LONG tblID)
{
    if (tblID == reportEntries.thumbnailDamaged || tblID == reportEntries.thumbnailDiscrepancy){
        return true;
    }
    return false;
}

ReportTableList stringToEntryList(wchar_t* buffer, int bufferLen)
{
    ReportTableList retVal;
    int length = wcsnlen(buffer,bufferLen);
    int start = 0;
    for (int i=0;i<length+1;i++)
    {
        if (buffer[i]==L',' || buffer[i]==L'\0' )
        {
            int itemSize = i - start;
            wcsncpy(retVal.entries[retVal.numEntries],&buffer[start],itemSize);
            retVal.numEntries++;
            start = i+1;
            if (buffer[start]== L' '){
                start++;
            }
        }
    }
    return retVal;
}


bool containsThumbnailMismatchTable(wchar_t* buffer, int bufferLen)
{
    ReportTableList entries = stringToEntryList(buffer, bufferLen);
    for (int i=0;i< entries.numEntries;i++)
    {
        if (wcscmp(entries.entries[i],L"Thumbnail discrepancy")==0 || wcscmp(entries.entries[i],L"Thumbnail notable (data corrupt/incomplete)")==0)
        {
            return true;
        }
    }
    return false;
}

/*Function: identifyReportTables
    Setup case when X-Tension is first run. Only called on first XT_Prepare call of run.

    Created in 1.50, split from <XT_Prepare>

    Returns:
        0 - No Error
        <0 - Error

    See Also:
        Called by   -   <firstRunSetup>
*/

int identifyReportTables()
{
    LONG maxTableNumber=0;
    XWF_GetReportTableInfo(NULL,-1,&maxTableNumber);
    for (LONG i=0;i<maxTableNumber;i++)
    {
        LONG flags = 0;
        wchar_t* tblName = (wchar_t*)XWF_GetReportTableInfo(NULL, i, &flags);
        if (flags & 0x02){
            int result = addReportTableEntry(tblName, i,true);
        }
        else{
            if (tblName != NULL)
            {
                checkReportTableThumbnailMismatch(tblName, i, false);
            }
        }
    }
    return 0;
}


bool isUserCreatedReportTable(wchar_t* tableName)
{
    for (int i=0;i<reportEntries.numTables;i++){
        if(wcscmp(reportEntries.entries[i].reportTableName,tableName)==0){
            return reportEntries.entries[i].userCreated;
        }
    }
    //no found, not user created
    return false;
}

wchar_t* retrieveUserReportTableAssociations(LONG nItemID)
{
    int bufferLen = 1024;
    wchar_t* retVal = new wchar_t[bufferLen];
    retVal[0] = '\0';
    DWORD numTables = XWF_GetReportTableAssocs(nItemID,retVal,bufferLen);
    if (numTables == 0) {
        delete[] retVal;
        return nullptr;
    }
    ReportTableList entries = stringToEntryList(retVal, bufferLen);
    retVal[0] = L'\0';
    for (int i=0;i< entries.numEntries;i++){
        if (isUserCreatedReportTable(entries.entries[i])){
            if (wcscmp(L"",retVal)!=0){
                wcsncat(retVal,L",", bufferLen);
            }
            wcsncat(retVal,entries.entries[i], bufferLen);
        }
    }
    if (wcscmp(L"",retVal)==0) {
        delete[] retVal;
        return nullptr;
    }
    return retVal;
}
