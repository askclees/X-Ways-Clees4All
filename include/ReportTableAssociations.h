#ifndef REPORTTABLEASSOCIATIONS_H_INCLUDED
#define REPORTTABLEASSOCIATIONS_H_INCLUDED

#include "windows.h"
#include <wchar.h>

/**
 * @brief Stores the ID, name, and user-created flag for a single X-Ways report table.
 */
struct reportTableEntry{
    /** @brief X-Ways report table ID, or -1 if unset. */
    LONG reportTableID=-1;
    /** @brief Name of the report table. */
    wchar_t* reportTableName=nullptr;
    /** @brief True if the table was created by the user. */
    bool userCreated=false;
};

/**
 * @brief Holds all discovered report tables and the IDs of thumbnail-related system tables.
 */
struct ReportTableDetails{
    /** @brief X-Ways ID of the "Thumbnail discrepancy" report table, or -1 if not found. */
    LONG thumbnailDiscrepancy = -1;
    /** @brief X-Ways ID of the "Thumbnail notable (data corrupt/incomplete)" report table, or -1 if not found. */
    LONG thumbnailDamaged = -1;
    /** @brief Maximum number of entries in the entries array. */
    LONG maxTables = 1024;
    /** @brief Current number of populated entries. */
    LONG numTables = 0;
    /** @brief Array of report table entries. */
    reportTableEntry entries[1024];
};

/**
 * @brief A flat list of report table name strings parsed from a comma-separated buffer.
 */
struct ReportTableList{
    /** @brief Number of valid entries. */
    int numEntries=0;
    /** @brief Array of null-terminated table name strings. */
    wchar_t entries[128][128]={0};
};

/** @brief Enumerates all X-Ways report tables and records user-created and thumbnail-mismatch tables. */
int identifyReportTables();
/** @brief Returns true if the given report table ID corresponds to a thumbnail mismatch table. */
bool isReportTableIDThumbnailMismatch(LONG tblID);
/** @brief Frees all report table name strings and resets the global reportEntries list. */
void clearReportTableDetails();
/** @brief Returns true if the comma-separated buffer contains a thumbnail mismatch table name. */
bool containsThumbnailMismatchTable(wchar_t* buffer, int bufferLen);
/** @brief Returns a comma-separated list of user-created report table associations for an item, or nullptr. */
wchar_t* retrieveUserReportTableAssociations(LONG nItemID);

#endif // REPORTTABLEASSOCIATIONS_H_INCLUDED
