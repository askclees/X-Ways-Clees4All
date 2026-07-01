//standard includes
#include <cstdio>
#include <windows.h>
#include <mutex>

//project headers
#include "X-Tension.h"
#include "debugMessage.h"

/** @brief Mutex serialising writes to the debug log and X-Ways output window. */
std::mutex dbgMessage;

/** @brief Mutex serialising increments to the per-error-type counters. */
std::mutex errorTotal;

/** @brief Handle to the open debug log file, or NULL if no log is active. */
FILE* debugLogFile;

/** @brief Per-error-type occurrence counters, indexed by REPORT_* constants. */
int errorLog[11]={0};

/** @brief Number of report table entries, must match the size of ReportTableList. */
const int numErrorTables = 11;

/**
 * @brief Report table name pairs, indexed by REPORT_* constants.
 *
 * Each row contains two strings: [0] the full report table name shown in
 * X-Ways, and [1] a short label used in the end-of-run error summary.
 */
const wchar_t* ReportTableList[][2] =
{
    {L"XT_CLEES4ALL Item Type Error",L"Unknown Item Type"},
    {L"XT_CLEES4ALL Unknown File Size",L"Unknown File Size"},
    {L"XT_CLEES4ALL No Hash Computed",L"No Hash computed"},
    {L"XT_CLEES4ALL File Write Size Mismatch",L"File Write Size Mismatch"},
    {L"XT_CLEES4ALL Unable to open File",L"Unable to Open File"},
    {L"XT_CLEES4ALL Excluded on parent",L"Excluded based on parent"},
    {L"XT_CLEES4ALL Excluded as Duplicate",L"Excluded as duplicate"},
    {L"XT_CLEES4ALL Excluded Based on File Size",L"Excluded on Filesize"},
    {L"XT_CLEES4ALL Excluded Thumbnail Embedded in Image",L"Excluded Thumbnail"},
    {L"XT_CLEES4ALL Excluded File Type Status",L"Excluded on File Type Status"},
    {L"XT_CLEES4ALL Excluded File Format Consistency ",L"Excluded on File Format Consistency"}
};

/**
 * @brief Opens a debug log file for writing.
 *
 * @param filePath Null-terminated path for the output log file.
 * @return         0 on success, 1 if the file could not be opened.
 *
 * @see endDebugLog
 */
int startDebugLog(const char* filePath)
{
    debugLogFile = fopen(filePath,"w");
    if (debugLogFile==NULL)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief Closes the debug log file.
 *
 * @return 0 on success, non-zero if the file could not be closed or was not open.
 *
 * @see startDebugLog
 */
int endDebugLog()
{
    if (debugLogFile != NULL)
    {
        int rc = fclose(debugLogFile);
        debugLogFile = NULL;
        return rc;
    }
    else
    {
        return 1;
    }
}

/**
 * @brief Writes a message to a debug log file.
 *
 * If @p f is NULL the message is silently discarded and 1 is returned.
 *
 * @param f       File to write to, or NULL.
 * @param message Null-terminated message string.
 * @return        0 if written to the file, 1 if no file handle is available (message not written).
 *
 * @see startDebugLog, endDebugLog
 */
int debugWriteDetails(FILE* f, const char* message)
{
    dbgMessage.lock();
    if (f !=NULL)
    {
        int check  = fprintf(f, "%s",message);
        if (check < 0)
        {
            XWF_OutputMessage(L"Fprintf error",0);
        }
        fflush(f);
        dbgMessage.unlock();
        return 0;
    }
    else
    {
        dbgMessage.unlock();
        return 1;
    }
}

/**
 * @brief Writes a message to the default debug log file.
 *
 * Convenience overload that uses the global debugLogFile handle.
 *
 * @param message Null-terminated message string.
 * @return        0 if written to the file, 1 if no file handle is available (message not written).
 *
 * @see startDebugLog, endDebugLog
 */
int debugWriteDetails(const char* message)
{
    return debugWriteDetails(debugLogFile,message);
}

/**
 * @brief Writes item ID, item name and module name to a debug log file.
 *
 * If @p nItemID is 0 only the module name is written (XWF_GetItemName is
 * not called, as it is invalid outside an active case context).
 *
 * @param f       File to write to.
 * @param nItemID X-Ways item ID the message relates to, or 0 to omit item details.
 * @param module  Name of the Clees4All module generating the message.
 * @return        0 if written to the file, 1 if no file handle is available (message not written).
 *
 * @see startDebugLog, endDebugLog
 */
int debugWriteDetails(FILE* f,LONG nItemID, const wchar_t* module)
{
    char* buffer = new char[1024];
    if (nItemID == 0)
    {
        snprintf(buffer, 1024, "Module: %ls\r\n", module);
    }
    else
    {
        LPWSTR ItemName = (LPWSTR)XWF_GetItemName(nItemID);
        if (ItemName != NULL)
            snprintf(buffer, 1024, "ItemID: %ld ItemName: %ls Module: %ls\r\n", nItemID, ItemName, module);
        else
            snprintf(buffer, 1024, "ItemID: %ld ItemName: (null) Module: %ls\r\n", nItemID, module);
    }
    int result = debugWriteDetails(f,buffer);
    delete[] buffer;
    return result;
}

/**
 * @brief Writes item ID, item name and module name to the default debug log file.
 *
 * Convenience overload that uses the global debugLogFile handle.
 * If @p nItemID is 0 only the module name is written.
 *
 * @param nItemID X-Ways item ID the message relates to, or 0 to omit item details.
 * @param module  Name of the Clees4All module generating the message.
 * @return        0 if written to the file, 1 if no file handle is available (message not written).
 *
 * @see startDebugLog, endDebugLog
 */
int debugWriteDetails(LONG nItemID, const wchar_t* module)
{
    if (debugLogFile!=NULL)
    {
        return debugWriteDetails(debugLogFile, nItemID, module);
    }
    else
    {
        return debugWriteDetails(NULL, nItemID, module);
    }
}

/**
 * @brief Writes item ID, module, message and a variable list to the debug log.
 *
 * Formats a header line followed by one line per variable in @p varArgs.
 * Supported variable types are 'c' (char*), 'i' (int32 or int64) and 'w' (wchar_t*).
 *
 * @param nItemID X-Ways item ID the message relates to.
 * @param module  Name of the Clees4All module generating the message.
 * @param message Null-terminated descriptive message string.
 * @param varArgs List of variables to append to the output.
 * @return        0 if written to the file, 1 if written to the X-Ways output window.
 *
 * @see startDebugLog, endDebugLog
 */
int debugWriteDetails(LONG nItemID, const wchar_t* module,const wchar_t* message,varList varArgs)
{
    char buffer[1024];
    int pos = snprintf(buffer,sizeof(buffer),"ItemID: %ld, Module: %ls, Message: %ls\r\n",nItemID,module,message);
    if (pos < 0) pos = 0;
    for (int i=0;i<varArgs.noVars && pos < (int)sizeof(buffer)-1;i++)
    {
        char tempBuffer[256]={0};
        switch(varArgs.entries[i].type)
        {
        case 'c':
            //ascii character
            snprintf(tempBuffer,sizeof(tempBuffer),"Variable: %i, Value: %s \r\n",i,(char*)varArgs.entries[i].varData);
            break;
        case 'i':
            //integer variable
            if (varArgs.entries[i].varLen==4){
                snprintf(tempBuffer,sizeof(tempBuffer),"Variable: %i, Value: %ld \r\n",i,*(long*)varArgs.entries[i].varData);
            }
            else if (varArgs.entries[i].varLen==8){
                snprintf(tempBuffer,sizeof(tempBuffer),"Variable: %i, Value: %lld\r\n",i,*(long long*)varArgs.entries[i].varData);
            }
            break;
        case 'w':
            snprintf(tempBuffer,sizeof(tempBuffer),"Variable: %i, Value: %ls \r\n",i,(wchar_t*)varArgs.entries[i].varData);
            break;
        default:
            break;
        }
        int written = snprintf(buffer+pos, sizeof(buffer)-pos, "%s", tempBuffer);
        if (written > 0) pos += written;
    }
    if (debugLogFile!=NULL)
    {
        return debugWriteDetails(debugLogFile,buffer);
    }
    else
    {
        return debugWriteDetails(NULL, buffer);
    }
}

/**
 * @brief Outputs an error message and item ID to the X-Ways output window.
 *
 * @param errMsg  Message to display.
 * @param nItemID Item ID appended to the message.
 */
void outputErrorMessage(const wchar_t* errMsg, LONG nItemID)
{
    wchar_t errorMessage[2048];
    swprintf(errorMessage,2048,L"%ls %lu",errMsg, nItemID);
    dbgMessage.lock();
    XWF_OutputMessage(errorMessage,0);
    dbgMessage.unlock();
}

/**
 * @brief Outputs an error message to the X-Ways output window.
 *
 * @param errMsg Message to display.
 */
void outputErrorMessage(const wchar_t* errMsg)
{
    dbgMessage.lock();
    XWF_OutputMessage((wchar_t*)errMsg,0);
    dbgMessage.unlock();
}

/**
 * @brief Outputs an error message with additional detail to the X-Ways output window.
 *
 * @param errMsg Primary message string.
 * @param detail Additional detail string appended directly after @p errMsg.
 */
void outputErrorMessage(const wchar_t* errMsg, wchar_t* detail)
{
    wchar_t errorMessage[2048];
    swprintf(errorMessage,2048,L"%ls%ls",errMsg,detail);
    dbgMessage.lock();
    XWF_OutputMessage(errorMessage,0);
    dbgMessage.unlock();
}

/**
 * @brief Records an error by adding the item to an X-Ways report table and
 *        incrementing the per-error-type counter.
 *
 * @param nItemID   X-Ways item ID the error relates to.
 * @param errorCode Error type, must be one of the REPORT_* constants defined
 *                  in debugMessage.h.
 */
void errorRaised(LONG nItemID,int errorCode)
{
    //add file to report table for easier finding.
    const wchar_t* messagePtr = ReportTableList[errorCode][0];
    LONG result = XWF_AddToReportTable(nItemID, const_cast<wchar_t*>(messagePtr), 1);
    if (result == 0)
    {
        outputErrorMessage(L"Unable to set report table for itemID: ",nItemID);
    }
    errorTotal.lock();
    errorLog[errorCode]++;
    errorTotal.unlock();
}

/**
 * @brief Outputs a summary of all errors encountered to the X-Ways output window and case log.
 *
 * Prints a count for each error type, then a reminder that affected files have
 * been added to report table associations.
 */
void errorReport()
{
    //output starting line
    XWF_OutputMessage(L"Number of errors encountered:",0);
    XWF_OutputMessage(L"Number of errors encountered:",0x10);
    for (int i=0;i<numErrorTables;i++)
    {
        wchar_t messageBuffer[1024]={0};
        swprintf(messageBuffer,L"%ls:\t\t%i",ReportTableList[i][1],errorLog[i]);
        //output message to window and print to log
        XWF_OutputMessage(messageBuffer,0);
        XWF_OutputMessage(messageBuffer,0x10);
    }
    XWF_OutputMessage(L"All error files added to relevant report table association. Please review these files, as they will not have been extracted",0);
}
