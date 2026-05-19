#include <cwchar>
#include <cstdio>

#include "accessible.h"

FilterInfo fInfo;

/* Section Accessiblity:

    Currently unused, possibly to be used to provide information that particular files are accessible/not.
*/

int AddPath(LPWSTR pathStr)
{
    int pathLen = wcslen(pathStr);
    fInfo.FilterPaths[fInfo.noPathFilters] = new wchar_t[pathLen + 1];
    swprintf(fInfo.FilterPaths[fInfo.noPathFilters],L"%ls",pathStr);
    fInfo.noPathFilters++;
    return 0;
}

int AddPathList(FILE* filterFile)
{
    wchar_t line[128] = {0};
    char buffer;
    bool endOfFile = FALSE;
    int i = 0;
    do
    {
        int result = fread(&buffer,1,1,filterFile);
        if (result != 1)
        {
            endOfFile = true;
        }
        else
        {
            i++;
            swprintf(line,L"%ls%c",line,buffer);
            if (buffer == '\n')
            {
                line[i-1] = L'\0';
                AddPath((wchar_t*)&line);
                for (int j=0;j<i;j++)
                {
                    line[j] = L'\0';
                    i = 0;
                }
            }
        }
    } while (endOfFile == FALSE);
    return 0;
}


BOOL checkPathName(LPWSTR name)
{
    for (int i=0;i<fInfo.noPathFilters;i++)
    {
        int check = wcscmp(name,fInfo.FilterPaths[i]);
        if (check == 0)
        {
            //matches
            return true;
        }
    }
    return false;
}

BOOL checkPath(LONG nItemID)
{
    //find parent until no parent, check folder name against list
    LONG currItem = nItemID;
    int checkEnd;
    do
    {
        checkEnd = XWF_GetItemParent(currItem);
        if (checkEnd != -1)
        {
            LPWSTR parentName = (LPWSTR)XWF_GetItemName(checkEnd);
            if (checkPathName(parentName))
            {
                return true;
            }
            currItem = checkEnd;
        }
    } while (checkEnd != -1);
    return false;
}


BOOL checkFileName(LONG nItemID)
{
    //check if filename is a special file type
    LPWSTR fileName = (LPWSTR)XWF_GetItemName(nItemID);
    int check = wcscmp(fileName,L"Thumbnail.jpg");
    if (check == 0)
    {
        return true;
    }
    check = wcsncmp(fileName, L"Thumbnail (", wcslen(L"Thumbnail ("));
    if (check == 0)
    {
        int nameLen = wcslen(fileName);
        check = wcsncmp(&fileName[nameLen - 5], L").jpg", 5);
        if (check == 0)
        {
            return true;
        }
    }
    //nothing here yet
    return false;
}

int checkAccessible(LONG nItemID)
{
    int result = ACCESSIBLE;
    BOOL check;
    if (XWF_GetItemInformation(nItemID,XWF_ITEM_INFO_DELETION,&check)!=0)
    {
        result = result | INACCESSIBLE_DELETED;
    }
    if (checkFileName(nItemID))
    {
        result = result | INACCESSIBLE_FILENAME;
    }
    if (checkPath(nItemID))
    {
        result = result | INACCESSIBLE_PATH;
    }
    return result;
}
