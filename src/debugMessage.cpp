//standard includes
#include <cstdio>
#include <windows.h>
#include <mutex>

//project headers
#include "X-Tension.h"
#include "debugMessage.h"

//globals
std::mutex dbgMessage, errorTotal;
FILE* debugLogFile;

int errorLog[11]={0};
const int numErrorTables = 11;
wchar_t* ReportTableList[][2] =
{
    {L"XT_CLEES4ALL Item Type Error",L"Unknown Item Type"},
    {L"XT_CLEES4ALL Unknown File Size",L"Unknown File Size"},
    {L"XT_CLEES4ALL No Hash Computed",L"No Hash computed"},
    {L"XT_CLEES4ALL File Write Size Mismatch",L"File Write Size Mismatch"},
    {L"XT_CLEES4ALL Unable to open File",L"Unable to Open File"},
    {L"XT_CLEES4ALL Excluded on parent",L"Excluded based on parent"},
    //1.50 added duplicate
    {L"XT_CLEES4ALL Excluded as Duplicate",L"Excluded as duplicate"},
    //1.50 added one for file size exclusion and thumbnail exclusion
    {L"XT_CLEES4ALL Excluded Based on File Size",L"Excluded on Filesize"},
    {L"XT_CLEES4ALL Excluded Thumbnail Embedded in Image",L"Excluded Thumbnail"},
    //1.51 added ones for file type status and consistency checks
    {L"XT_CLEES4ALL Excluded File Type Status",L"Excluded on File Type Status"},
    {L"XT_CLEES4ALL Excluded File Format Consistency ",L"Excluded on File Format Consistency"}
};

//  Section: Debug File Functions

/*Function: startDebugLog

    Function that starts a Debug log

    Parameters:

        char* filePath  -   NULL terminated string with path of desired output file

    Returns:
        0   -   Success
        1   -   Error occurred

    See Also:
        <endDebugLog>

*/

int startDebugLog(char* filePath)
{
    debugLogFile = fopen(filePath,"w");
    if (debugLogFile==NULL)
    {
        return 1;
    }
    return 0;
}


/*Function: endDebugLog

    Function that close the Debug log

    Parameters:

        None

    Returns:
        0   -   Success
        !0   -   Error occurred

    See Also:
        <startDebugLog>

*/
int endDebugLog()
{
    if (debugLogFile !=NULL)
    {
        return fclose(debugLogFile);
    }
    else
    {
        return 1;
    }
}

//  Section: Debug log writing functions

/*Function: debugWriteDetails

    Function that writes a NULL terminated character array to the debug file.

    If no debug file is opened (say there was an error opening the debug file), messages are output to the X-Ways window.

    Parameters:

        None

    Returns:
        0   -   Data written to output window
        1   -   Message written to X-Ways output Window

    See Also:
        <startDebugLog>, <endDebugLog>

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
        //no file yet, output to log window.
        //may be used for debugging case window data.
        XWF_OutputMessage((wchar_t*)message,0x04);
        dbgMessage.unlock();
        return 1;
    }
}

/*Function: debugWriteDetails

    Wrapper for just passing a single parameter version of function with same name

    Parameters:

        const char* message -   Message to be written

    Returns:
        0   -   Data written to output window
        1   -   Message written to X-Ways output Window

    See Also:
        <startDebugLog>, <endDebugLog>, <debugWriteDetails>

*/

int debugWriteDetails(const char* message)
{
    return debugWriteDetails(debugLogFile,message);
}


/*Function: debugWriteDetails

    Function for writing an item ID and module to the error log.

    Provides a wrapper to <debugWriteDetails> function

    Parameters:

        FILE* f                 -   File for message to be written to
        LONG nItemID            -   ItemID that error message relates to
        const wchar_t* module   -   Clees4All module that function relates to

    Returns:
        0   -   Data written to output window
        1   -   Message written to X-Ways output Window

    See Also:
        <startDebugLog>, <endDebugLog>, <debugWriteDetails>

*/

int debugWriteDetails(FILE* f,LONG nItemID, const wchar_t* module)
{
    LPWSTR ItemName;
    ItemName = (LPWSTR)XWF_GetItemName(nItemID);
    char* buffer = new char[1024];
    sprintf(buffer, "ItemID: %ld ItemName: %ls Module: %ls\r\n",nItemID,ItemName,module);
    int result = debugWriteDetails(f,buffer);
    delete[] buffer;
    return result;
}


/*Function: debugWriteDetails

    Function for writing an item ID and module to the error log.

    Provides a wrapper to <debugWriteDetails> function

    Parameters:

        LONG nItemID            -   ItemID that error message relates to
        const wchar_t* module   -   Clees4All module that function relates to

    Returns:
        0   -   Data written to output window
        1   -   Message written to X-Ways output Window

    See Also:
        <startDebugLog>, <endDebugLog>, <debugWriteDetails>

*/

//when used without a FILE object, use standard
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


/*Function: debugWriteDetails

    Function for writing an item ID and module to the error log.

    Allows a function to provide a variable length list of details to be written to the log for debug purposes.

    Provides a wrapper to <debugWriteDetails> function

    Parameters:

        LONG nItemID            -   ItemID that error relates to
        const wchar_t* module   -   Clees4All module that function relates to
        const wchar_t* message  -   NULL terminated Error message
        varList varArgs         -   List of arguments to be written to error log

    Returns:
        0   -   Data written to output window
        1   -   Message written to X-Ways output Window

    See Also:
        <startDebugLog>, <endDebugLog>, <debugWriteDetails>

*/

//function to output var list
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

//  Section: General Error Messages

/*Function: outputErrorMessage

    Function for outputting an error message to the X-Ways Output Window

    Parameters:

        const wchar_t* errMsg   -   Message to be output.
        LONG nItemID            -   ItemID that error relates to

    Returns:
        void

*/
void outputErrorMessage(const wchar_t* errMsg, LONG nItemID)
{
    wchar_t errorMessage[2048];
    swprintf(errorMessage,2048,L"%ls %lu",errMsg, nItemID);
    dbgMessage.lock();
    XWF_OutputMessage(errorMessage,0);
    dbgMessage.unlock();
}

/*Function: outputErrorMessage

    Function for outputting an error message to the X-Ways Output Window

    Parameters:

        const wchar_t* errMsg   -   Message to be output.

    Returns:
        void

*/
void outputErrorMessage(const wchar_t* errMsg)
{
    dbgMessage.lock();
    XWF_OutputMessage((wchar_t*)errMsg,0);
    dbgMessage.unlock();
}

/*Function: outputErrorMessage

    Function for outputting an error message to the X-Ways Output Window

    Parameters:

        const wchar_t* errMsg   -   Message to be output.
        wchar_t* detail         -   Second string with further error details.

    Returns:
        void

*/

void outputErrorMessage(const wchar_t* errMsg, wchar_t* detail)
{
    wchar_t errorMessage[2048];
    swprintf(errorMessage,2048,L"%ls%ls",errMsg,detail);
    dbgMessage.lock();
    XWF_OutputMessage(errorMessage,0);
    dbgMessage.unlock();
}

/*  Section: Report Table Error Functions

        This section contains functions that are used to add items to a report table when an error is recorded.

        Also includes functions for subsequent reporting of these errors.
*/


/*Function: errorRaised

    Function for outputting an error message to the X-Ways Output Window

    Parameters:

        LONG nItemID    -   ItemID error relates to
        int errorCode   -   Error code that corresponds to error codes in header file

    Returns:
        void

*/
void errorRaised(LONG nItemID,int errorCode)
{
    //add file to report table for easier finding.
    wchar_t* messagePtr = ReportTableList[errorCode][0];
    LONG result = XWF_AddToReportTable(nItemID,messagePtr,1);
    if (result == 0)
    {
        outputErrorMessage(L"Unable to set report table for itemID: ",nItemID);
    }
    errorTotal.lock();
    errorLog[errorCode]++;
    errorTotal.unlock();
}


/*Function: errorReport

    Function for outputting numbers of each type of error encountered.

    Output is to X-Ways Output Window and stamped to X-Ways log

    Returns:
        void

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

