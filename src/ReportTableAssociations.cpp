#include "ReportTableAssociations.h"
#include <wchar.h>
#include "X-Tension.h"
#include "debugMessage.h"

//1.50 added report table structure
ReportTableDetails reportEntries;

#define THUMBNAIL_DISCREPANCY   1
#define THUMBNAIL_NOTABLE       2

/**
 * @brief Adds a report table entry to the global reportEntries list.
 *
 * @param tblName    Wide string containing the name of the report table.
 * @param tblID      X-Ways report table ID.
 * @param userCreated True if the table was created by the user, false for system tables.
 * @return 0 on success, -1 if the entry list is full.
 */
int addReportTableEntry(wchar_t* tblName, LONG tblID, bool userCreated)
{
    if (reportEntries.numTables >= reportEntries.maxTables)
        return -1;
    reportEntries.entries[reportEntries.numTables].reportTableName = new wchar_t[wcslen(tblName)+1];
    wcsncpy(reportEntries.entries[reportEntries.numTables].reportTableName, tblName,wcslen(tblName)+1);
    reportEntries.entries[reportEntries.numTables].reportTableID = tblID;
    reportEntries.entries[reportEntries.numTables].userCreated = userCreated;
    reportEntries.numTables++;
    return 0;
}

/**
 * @brief Stores the table ID for a thumbnail-related report table by type.
 *
 * @param tblID X-Ways report table ID to store.
 * @param type  THUMBNAIL_DISCREPANCY or THUMBNAIL_NOTABLE indicating which slot to populate.
 */
void setThumbnailReportTableNumber(LONG tblID, int type)
{
    if (type==THUMBNAIL_DISCREPANCY){
        reportEntries.thumbnailDiscrepancy = tblID;
    }
    else if (type == THUMBNAIL_NOTABLE){
        reportEntries.thumbnailDamaged = tblID;
    }
}

/**
 * @brief Checks whether a report table name matches a known thumbnail mismatch table and records its ID.
 *
 * @param tblName    Wide string containing the report table name to test.
 * @param tblID      X-Ways report table ID.
 * @param userCreated Whether the table was user-created (unused; retained for API symmetry).
 *
 * @see setThumbnailReportTableNumber
 */
void checkReportTableThumbnailMismatch(wchar_t* tblName, LONG tblID, bool userCreated)
{
    if (wcscmp(tblName,L"Thumbnail discrepancy")==0){
        setThumbnailReportTableNumber(tblID,THUMBNAIL_DISCREPANCY);
    }
    else if (wcscmp(tblName,L"Thumbnail notable (data corrupt/incomplete)")==0){
        setThumbnailReportTableNumber(tblID,THUMBNAIL_NOTABLE);
    }
}


/**
 * @brief Frees all dynamically allocated report table name strings and resets the global reportEntries list.
 */
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

/**
 * @brief Checks whether a given report table ID matches one of the known thumbnail mismatch tables.
 *
 * @param tblID X-Ways report table ID to test.
 * @return true if the ID matches a thumbnail mismatch table, false otherwise.
 */
bool isReportTableIDThumbnailMismatch(LONG tblID)
{
    if (tblID == reportEntries.thumbnailDamaged || tblID == reportEntries.thumbnailDiscrepancy){
        return true;
    }
    return false;
}

/**
 * @brief Parses a comma-separated wide string of report table names into a ReportTableList.
 *
 * @param buffer    Wide character buffer containing the comma-separated table names.
 * @param bufferLen Length of the buffer in characters.
 * @return ReportTableList populated with the individual table name entries.
 */
ReportTableList stringToEntryList(wchar_t* buffer, int bufferLen)
{
    ReportTableList retVal;
    int length = wcsnlen(buffer,bufferLen);
    int start = 0;
    for (int i=0;i<length+1 && retVal.numEntries<128;i++)
    {
        if (buffer[i]==L',' || buffer[i]==L'\0' )
        {
            int itemSize = i - start;
            if (itemSize > 127) itemSize = 127;
            wcsncpy(retVal.entries[retVal.numEntries],&buffer[start],itemSize);
            retVal.entries[retVal.numEntries][itemSize] = L'\0';
            retVal.numEntries++;
            start = i+1;
            if (start < length && buffer[start]== L' '){
                start++;
            }
        }
    }
    return retVal;
}


/**
 * @brief Returns true if the comma-separated report table string contains a thumbnail mismatch table name.
 *
 * @param buffer    Wide character buffer containing the comma-separated report table names.
 * @param bufferLen Length of the buffer in characters.
 * @return true if a thumbnail mismatch table is present, false otherwise.
 *
 * @see stringToEntryList
 */
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

/**
 * @brief Enumerates all X-Ways report tables, records user-created tables, and identifies thumbnail mismatch tables.
 *
 * @return 0 on success, negative on error.
 *
 * @see addReportTableEntry
 * @see checkReportTableThumbnailMismatch
 * @see firstRunSetup
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
            if (tblName != NULL)
            {
                int result = addReportTableEntry(tblName, i,true);
            }
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


/**
 * @brief Returns whether a report table with the given name was user-created.
 *
 * Searches the global reportEntries list for a name match.
 *
 * @param tableName Wide string containing the report table name to look up.
 * @return true if a matching user-created table is found, false otherwise.
 */
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

/**
 * @brief Retrieves the user-created report table associations for a given X-Ways item.
 *
 * Queries X-Ways for all report table associations of the item, then filters to only those
 * tables that are user-created. Returns NULL if no user-created associations exist.
 *
 * The returned buffer is allocated with new[] and must be freed by the caller using delete[].
 *
 * @param nItemID X-Ways item ID to query.
 * @return Newly allocated wide string containing comma-separated user-created table names,
 *         or NULL if the item has no user-created report table associations.
 *
 * @see stringToEntryList
 * @see isUserCreatedReportTable
 */
wchar_t* retrieveUserReportTableAssociations(LONG nItemID)
{
    int bufferLen = 1024;
    wchar_t* retVal = new wchar_t[bufferLen];
    retVal[0] = '\0';
    LONG numTables = XWF_GetReportTableAssocs(nItemID,retVal,bufferLen);
    if (numTables < 0)
    {
        outputErrorMessage(L"XWF_GetReportTableAssocs failed in retrieveUserReportTableAssociations for itemID: ", nItemID);
        delete[] retVal;
        return nullptr;
    }
    if (numTables == 0) {
        delete[] retVal;
        return nullptr;
    }
    ReportTableList entries = stringToEntryList(retVal, bufferLen);
    retVal[0] = L'\0';
    for (int i=0;i< entries.numEntries;i++){
        if (isUserCreatedReportTable(entries.entries[i])){
            if (wcscmp(L"",retVal)!=0){
                wcsncat(retVal,L",", bufferLen - wcslen(retVal) - 1);
            }
            wcsncat(retVal,entries.entries[i], bufferLen - wcslen(retVal) - 1);
        }
    }
    if (wcscmp(L"",retVal)==0) {
        delete[] retVal;
        return nullptr;
    }
    return retVal;
}
