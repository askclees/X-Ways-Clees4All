//std libraries
#include <cstdio>
#include <wchar.h>
#include <string.h>
#include <ctime>
#include <string>
#include <windows.h>
#include <shlobj.h>
#include <climits>
#include <map>
#include "sqlite3.h"
#include "shlwapi.h"

#include <commctrl.h>

//other modules
#include "main.h"
#include "SQLFunctions.h"
#include "utility.h"
#include "VisualStyles.h"

//windows form sizes
#define MainWindowWidth 850
#define MainWindowLength 430
#define LeftHandStartX 10
#define FirstLineY 10
#define SecondLineY 40
#define ThirdLineY 70
#define FourthLineY 100
#define FifthLineY 130
#define TypeLineY 160
#define LastLineY 360

//form controls
#define IDC_TEXT_MAXPICSIZE         101
#define IDC_TEXT_MAXVIDSIZE         102
#define IDC_TEXT_CMBOVERWRITE       103
#define IDC_TEXT_REPORTOUTPUT       104
#define IDC_BTN_REPORTOUTPUT        105
#define IDC_BTN_OK                  106
#define IDC_BTN_CANCEL              107
#define IDC_BTN_GRIFFEYE            108
#define IDC_CBO_MAXPIC              109
#define IDC_CBO_MAXVID              110
#define IDC_TEXT_MINPICSIZE         111
#define IDC_TEXT_MINVIDSIZE         112
#define IDC_CBO_MINPIC              113
#define IDC_CBO_MINVID              114
#define IDC_LBX_TYPESTATUS          115
#define IDC_LBX_FILEFORMAT          116
#define IDC_TEXT_GRIFFEYELOCATION   117

HWND optHwnd;
HWND MaxPicSize, txtMaxPicSize, MaxVidSize, txtMaxVidSize, lstOverwrite, lstTxtOverwrite;
HWND ReportOutput, cmdReportOutput, txtReportOutput, cmdOK, cmdCancel, txtGriffeyeLocation, GriffeyeLocation, cmdGriffeyeLocation;
HWND drpMinVid, drpMinPic, drpMaxVid, drpMaxPic;
HWND MinPicSize, txtMinPicSize, MinVidSize, txtMinVidSize;
HWND lstFileStatus, lstFileFormat, txtFileStatus, txtFileFormat;

const char* arrayTypeStatus[] =
{
    "not verified",
    "irrelevant",
    "not in list",
    "confirmed",
    "not confirmed",
    "newly identified",
    "mismatch detected"
};
int numTypeStatus = 7;

const char* arrayFileFormat[] =
{
    "unknown",
    "OK",
    "irregular",
    "corrupt"
};
int numFileFormat = 4;

static HBRUSH hBrush = CreateSolidBrush(RGB(240,240,240));

//globals
char optionsDatabasePath[MAX_PATH];

/** @brief Releases GDI resources allocated during options GUI creation. Call when the X-Tension is unloaded. */
void cleanupOptions()
{
    if (hBrush != NULL) {
        DeleteObject(hBrush);
        hBrush = NULL;
    }
}


//prototyping
void CreateOptionsControls(HWND hwnd);
LRESULT CALLBACK OptionsWindowProc (HWND hwnd, UINT uMsg, WPARAM wparam, LPARAM lParam);
static int CALLBACK BrowserCallbackProc(HWND hwnd,UINT uMsg,LPARAM lParam, LPARAM lpData);
ExtractOptions loadOrCreateOptions(BOOL* success);
void createOptions(char path[]);


/**
 * @brief Creates and runs the options window so that defaults can be changed.
 *
 * @return 0 always.
 *
 * @see OptionsWindowProc
 */
int createOptionsWindow()
{
    VisualStylesScope visualStyles;
    const char CLASS_NAME[] = C4A_TITLE " Options";
    WNDCLASSEX wc = {};

    wc.lpfnWndProc = OptionsWindowProc;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.cbClsExtra=0;
    wc.cbWndExtra=0;
    wc.hbrBackground=hBrush;
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hIcon= 0;
    wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpszClassName = CLASS_NAME;
    wc.hInstance = extractInfo.thisDLL;

    RegisterClassEx(&wc);

    optHwnd =CreateWindowEx(
        0, //dwexstyle
        CLASS_NAME, //class name
        CLASS_NAME, //text for window
        WS_OVERLAPPEDWINDOW, //dwstyle
        100,//x
        100,//y
        MainWindowWidth,//width
        MainWindowLength+(extractInfo.noNames * 30),//height
        NULL, //parent
        NULL,//hmenu
        NULL,//hinstance
        NULL// lpparam
    );
    if (optHwnd == NULL)
    {
        return 0;
    }
    ShowWindow(optHwnd, 5);
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0,0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

/**
 * @brief Reads a size value and unit from a combo/edit control pair and returns the size in bytes.
 *
 * @param hwnCmbo Handle to the units combo box (0=b, 1=KB, 2=MB, 3=GB).
 * @param hwndTxt Handle to the edit control containing the numeric value.
 * @return Size in bytes.
 */
INT64 getSizeInBytes(HWND hwnCmbo, HWND hwndTxt)
{
    char numVal[16];
    int units = SendMessage(hwnCmbo, CB_GETCURSEL, 0, 0);
    GetWindowText(hwndTxt,numVal,16);
    INT64 multiplier;
    switch (units){
        case 0:
            multiplier = 1;
        break;
        case 1:
            multiplier = 1024;
        break;
        case 2:
            multiplier = 1024*1024;
        break;
        case 3:
            multiplier = 1024*1024*1024;
        break;
        default:
            multiplier = 1;
        break;
    }
    INT64 numericValue = strtoll(numVal,NULL,10);
    //clamp rather than let the multiplication silently overflow/underflow (UB for a signed type)
    if (numericValue > LLONG_MAX / multiplier)
    {
        numericValue = LLONG_MAX / multiplier;
    }
    else if (numericValue < LLONG_MIN / multiplier)
    {
        numericValue = LLONG_MIN / multiplier;
    }
    return (numericValue*multiplier);

}


/**
 * @brief Validates all option fields before saving.
 *
 * Checks that the report output path exists, the Griffeye folder (if set) contains
 * a recognised CLI executable, and that min sizes do not exceed max sizes.
 *
 * @return true if all fields are valid, false if any validation fails.
 */
bool detailsValid()
{
    char buffer[1024];
    int length = GetWindowTextLength(ReportOutput);
    GetWindowText(ReportOutput,buffer,1024);
    if(!dirExists(buffer)){
        MessageBox(NULL,"Report Output Path does not exist or cannot be accessed","Error ",MB_ICONERROR);
        return false;
    }
    //check valid griffeye location (optional — skip if empty)
    wchar_t griffeyeTemp[MAX_PATH];
    GetWindowTextW(GriffeyeLocation,griffeyeTemp,MAX_PATH);
    if (wcslen(griffeyeTemp) > 0)
    {
        if (findGriffeyeExe(griffeyeTemp) == NULL)
        {
            MessageBox(NULL,"Neither analyze-cli.exe nor magnet-griffeye-cli.exe found in the given folder","Error ",MB_ICONERROR);
            return false;
        }
    }
    //check sizes make sense
    INT64 maxSize = getSizeInBytes(drpMaxPic,MaxPicSize);
    INT64 minSize = getSizeInBytes(drpMinPic,MinPicSize);
    if (maxSize !=0 &&(maxSize < minSize)){
        MessageBox(NULL,"Picture Minimum Size cannot be greater than Maximum Size","Error ",MB_ICONERROR);
        return false;
    }
    maxSize = getSizeInBytes(drpMaxVid,MaxVidSize);
    minSize = getSizeInBytes(drpMinVid,MinVidSize);
    if (maxSize !=0 &&(maxSize < minSize)){
        MessageBox(NULL,"Video Minimum Size cannot be greater than Maximum Size","Error ",MB_ICONERROR);
        return false;
    }
    return true;
}

/**
 * @brief Reads the selected items from the type status list box and returns them as a combined flag.
 *
 * @return Bitmask of selected type status flags.
 */
int getTypeStatus()
{
    int retVal =0;
    int noItems = SendMessage(lstFileStatus, LB_GETSELCOUNT, 0, 0);
    if (noItems == LB_ERR) { return retVal; }
    int* arrItems = new int[noItems];
    SendMessage(lstFileStatus,LB_GETSELITEMS,noItems,(LPARAM)arrItems);
    for (int i=0;i<noItems;i++)
    {
        switch (arrItems[i]){
        case 0:
            retVal = retVal | NOT_VERIFIED;
            break;
        case 1:
            retVal = retVal | IRRELEVANT;
            break;
        case 2:
            retVal = retVal | NOT_IN_LIST;
            break;
        case 3:
            retVal = retVal | CONFIRMED;
            break;
        case 4:
            retVal = retVal | NOT_CONFIRMED;
            break;
        case 5:
            retVal = retVal | NEWLY_IDENTIFIED;
            break;
        case 6:
            retVal = retVal | MISMATCH_DETECTED;
            break;
        }
    }
    delete[] arrItems;
    return retVal;
}

/**
 * @brief Reads the selected items from the file format list box and returns them as a combined flag.
 *
 * @return Bitmask of selected file format flags.
 */
int getFileTypeStatus()
{
    int retVal =0;
    int noItems = SendMessage(lstFileFormat, LB_GETSELCOUNT, 0, 0);
    if (noItems == LB_ERR) { return retVal; }
    int* arrItems = new int[noItems];
    SendMessage(lstFileFormat,LB_GETSELITEMS,noItems,(LPARAM)arrItems);
    for (int i=0;i<noItems;i++)
    {
        switch (arrItems[i]){
        case 0:
            retVal = retVal | UNKNOWN;
            break;
        case 1:
            retVal = retVal | OK;
            break;
        case 2:
            retVal = retVal | IRREGULAR;
            break;
        case 3:
            retVal = retVal | CORRUPT;
            break;
        }
    }
    delete[] arrItems;
    return retVal;
}

/**
 * @brief Handles the OK button click: validates, saves options, and closes the window.
 *
 * @param hwnd Handle to the options window.
 */
void BTN_OK_CLICK(HWND hwnd)
{
    ExtractOptions opt = {0};
    char buffer[1024];
    //validate details first
    bool valid = detailsValid();
    if (!valid) {return;}

    //get min/max file sizes
    opt.maxPictureSize = getSizeInBytes(drpMaxPic,MaxPicSize);
    opt.maxMovieSize = getSizeInBytes(drpMaxVid,MaxVidSize);
    opt.minPictureSize = getSizeInBytes(drpMinPic,MinPicSize);
    opt.minMovieSize = getSizeInBytes(drpMinVid,MinVidSize);

    //matches the 1024-byte buffer detailsValid() used to validate this same control,
    //so a path that passed validation isn't silently truncated here before being saved
    char tempPath[1024], griffeyeTemp[MAX_PATH];
    GetWindowText(ReportOutput,tempPath,1024);
    swprintf(opt.errorReportPath,2048,L"%s",tempPath);
    GetWindowText(GriffeyeLocation,griffeyeTemp,MAX_PATH);
    swprintf(opt.GriffeyePath,2048,L"%s",griffeyeTemp);
    int owrite = SendMessage(lstOverwrite,CB_GETCURSEL, 0, 0);
    if (owrite == 0){
        opt.overwriteFiles = FALSE;
    }
    else{
        opt.overwriteFiles = TRUE;
    }
    opt.TypeStatusFlags = getTypeStatus();
    opt.FileTypeFlag = getFileTypeStatus();
    saveOptions(optionsDatabasePath,opt);
    DestroyWindow(hwnd);
    PostQuitMessage(0);
    //return 0;

}

/**
 * @brief Handles the Griffeye folder browse button click.
 */
void BTN_GRIFFEYE_CLICK()
{
    TCHAR path[2048]={0};
    BROWSEINFO folderDialog = {0};
    folderDialog.lpszTitle = ("Select Griffeye Folder");
    folderDialog.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    folderDialog.lpfn = BrowserCallbackProc;
    LPITEMIDLIST pidl = SHBrowseForFolder(&folderDialog);
    if (pidl != 0)
    {
        if (SHGetPathFromIDList(pidl,path))
        {
            SetWindowText(GriffeyeLocation,path);
        }
        CoTaskMemFree(pidl);
    }
}

/**
 * @brief Handles the report output folder browse button click.
 */
void BTN_REPORTOUTPUT_CLICK()
{
    TCHAR path[2048]={0};
    BROWSEINFO folderDialog = {0};
    folderDialog.lpszTitle = ("Select Report Output Folder");
    folderDialog.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    folderDialog.lpfn = BrowserCallbackProc;
    LPITEMIDLIST pidl = SHBrowseForFolder(&folderDialog);
    if (pidl != 0)
    {
        if (SHGetPathFromIDList(pidl,path))
        {
            SetWindowText(ReportOutput,path);
        }
        CoTaskMemFree(pidl);
    }
}

/**
 * @brief Window procedure for the options window.
 *
 * @see CreateOptionsControls
 */

LRESULT CALLBACK OptionsWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
            CreateOptionsControls(hwnd);
            return 0;
        case WM_COMMAND:
            switch(LOWORD(wParam)){
                //code here for buttons
                case IDC_BTN_REPORTOUTPUT:{
                    switch (HIWORD(wParam)){
                        case BN_CLICKED:{BTN_REPORTOUTPUT_CLICK();}
                            break;
                    }
                }
                break;
                case IDC_BTN_GRIFFEYE:{
                    switch (HIWORD(wParam)){
                        case BN_CLICKED:{BTN_GRIFFEYE_CLICK();}
                            break;
                    }
                }
                break;
                case IDC_BTN_OK:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:{ BTN_OK_CLICK(hwnd);}
                        break;
                    }

                }
                break;
                case IDC_BTN_CANCEL:{
                    switch (HIWORD(wParam)){
                        case BN_CLICKED:{
                                DestroyWindow(hwnd);
                                PostQuitMessage(0);
                                return 0;
                            }
                            break;
                        }
                    }
                break;
            }
            break;
            return 0;
        case WM_DESTROY:
            DestroyWindow(hwnd);
            PostQuitMessage(0);
            return 0;
        case WM_PAINT:{
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                FillRect(hdc, &ps.rcPaint, hBrush);
                EndPaint(hwnd, &ps);
            }
            return 0;
        break;
        case WM_GETMINMAXINFO:{
                MINMAXINFO FAR *mInfo = (MINMAXINFO FAR *)lParam;
            }
        break;
        case WM_SIZE:{
                int newWidth = LOWORD(lParam);
            }
            return 0;
        break;
    }
    return DefWindowProc(hwnd,uMsg,wParam, lParam);
}

/**
 * @brief Converts a byte size into the most appropriate human-readable unit.
 *
 * @param sizeInBytes Size in bytes to convert.
 * @param unit        Output: 0=bytes, 1=KB, 2=MB, 3=GB.
 * @return The size value expressed in the chosen unit.
 */

INT64 determineSizeLimit(INT64 sizeInBytes, int* unit)
{
    INT64 remainder;
    if (sizeInBytes == 0){
        *unit=0;
        return 0;
    }
    if (sizeInBytes >= (1024*1024*1024)){
        remainder = sizeInBytes % (1024*1024*1024);
        if (remainder == 0){
            *unit=3;
            return (INT64) sizeInBytes / (1024*1024*1024);
        }
    }
    if (sizeInBytes >= (1024*1024)){
        remainder = sizeInBytes % (1024*1024);
        if (remainder == 0){
            *unit=2;
            return (INT64) sizeInBytes / (1024*1024);
        }
    }
    if (sizeInBytes >= 1024){
        remainder = sizeInBytes % (1024);
        if (remainder == 0){
            *unit=1;
            return (INT64) sizeInBytes / (1024);
        }
    }
    *unit=0;
    return sizeInBytes;
}

/**
 * @brief Displays a MessageBox error when a Win32 control could not be created.
 *
 * @param controlName Name of the control that failed, included in the error message.
 */
void outputControlOutputError(const char* controlName)
{
    int result=GetLastError();
    char message[2048];
    snprintf(message,sizeof(message),"Creation Error: %d for control: %s",result,controlName);
    MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
}

/**
 * @brief Populates a size unit combo box with b/Kb/Mb/Gb entries and sets the current selection.
 *
 * @param hwnCmbo Handle to the combo box.
 * @param units   Index of the unit to select (0=b, 1=Kb, 2=Mb, 3=Gb).
 */
void populateSizeCmbo(HWND hwnCmbo, int units)
{
    SendMessage(hwnCmbo, CB_INSERTSTRING,-1,(LPARAM)"b");
    SendMessage(hwnCmbo, CB_INSERTSTRING,-1,(LPARAM)"Kb");
    SendMessage(hwnCmbo, CB_INSERTSTRING,-1,(LPARAM)"Mb");
    SendMessage(hwnCmbo, CB_INSERTSTRING,-1,(LPARAM)"Gb");
    SendMessage(hwnCmbo, CB_SETCURSEL, (WPARAM)units,(LPARAM)0);

}

/**
 * @brief Creates the max/min picture size controls on the first row of the options window.
 *
 * @param hwnd Handle to the parent options window.
 * @return 0 always.
 */
int drawFirstLine(HWND hwnd)
{
    char tempnum[16];
    INT64 sizeInUnit;
    int units;
    //first line
    txtMaxPicSize = CreateWindowEx(0,"Static","Maximum Picture size to export:",WS_CHILD|WS_VISIBLE|SS_RIGHT,LeftHandStartX,FirstLineY,250,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtMaxPicSize) { outputControlOutputError("txtMaxPicSize"); }

    MaxPicSize = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","0",WS_CHILD|WS_VISIBLE,LeftHandStartX + 260, FirstLineY ,60,24,hwnd,(HMENU)IDC_TEXT_MAXPICSIZE,GetModuleHandle(NULL),NULL);
    if (!MaxPicSize) { outputControlOutputError("MaxPicSize"); }


    drpMaxPic = CreateWindowEx(0,"Combobox",NULL,WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|CBS_DROPDOWNLIST|CBS_HASSTRINGS,
                               LeftHandStartX + 320, FirstLineY ,45,20,hwnd,(HMENU)IDC_CBO_MAXPIC,GetModuleHandle(NULL),0);
    if (!drpMaxPic) {outputControlOutputError("drpMaxPic");}
    sizeInUnit=determineSizeLimit(extractOpt.maxPictureSize,&units);
    populateSizeCmbo(drpMaxPic,units);
    snprintf(tempnum,sizeof(tempnum),"%llu",sizeInUnit);
    SetWindowText(MaxPicSize,tempnum);
    tempnum[0]='\0';

    txtMinPicSize = CreateWindowEx(0,"Static","Minimum Picture size to export:",WS_CHILD|WS_VISIBLE|SS_RIGHT,LeftHandStartX + ((MainWindowWidth-100)/2),FirstLineY,250,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtMinPicSize) { outputControlOutputError("txtMinPicSize"); }

    MinPicSize = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","0",WS_CHILD|WS_VISIBLE,LeftHandStartX + ((MainWindowWidth-100)/2)+  260, FirstLineY ,60,24,hwnd,(HMENU)IDC_TEXT_MINPICSIZE,GetModuleHandle(NULL),NULL);
    if (!MinPicSize) { outputControlOutputError("MinPicSize"); }


    drpMinPic = CreateWindowEx(0,"Combobox",NULL,WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|CBS_DROPDOWNLIST|CBS_HASSTRINGS,
                               LeftHandStartX + ((MainWindowWidth-100)/2)+ 320, FirstLineY ,45,20,hwnd,(HMENU)IDC_CBO_MINPIC,GetModuleHandle(NULL),0);
    if (!drpMinPic) {outputControlOutputError("drpMinPic");}
    sizeInUnit=determineSizeLimit(extractOpt.minPictureSize,&units);
    populateSizeCmbo(drpMinPic,units);
    snprintf(tempnum,sizeof(tempnum),"%llu",sizeInUnit);
    SetWindowText(MinPicSize,tempnum);
    tempnum[0]='\0';
    return 0;
}


/**
 * @brief Creates the max/min video size controls on the second row of the options window.
 *
 * @param hwnd Handle to the parent options window.
 * @return 0 always.
 */
int drawSecondLine(HWND hwnd)
{
    char tempnum[16];
    INT64 sizeInUnit;
    int units;

    txtMaxVidSize = CreateWindowEx(0,"Static","Maximum Video size to export:",WS_CHILD|WS_VISIBLE|SS_RIGHT,LeftHandStartX,SecondLineY,250,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtMaxVidSize) {outputControlOutputError("txtMaxVidSize");}

    MaxVidSize = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","0",WS_CHILD|WS_VISIBLE,LeftHandStartX + 260, SecondLineY ,60,24,hwnd,(HMENU)IDC_TEXT_MAXVIDSIZE,GetModuleHandle(NULL),NULL);
    if (!MaxVidSize) {outputControlOutputError("MaxVidSize");}

    drpMaxVid = CreateWindowEx(0,"Combobox",NULL,WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|CBS_DROPDOWNLIST|CBS_HASSTRINGS,
                               LeftHandStartX + 320, SecondLineY ,45,20,hwnd,(HMENU)IDC_CBO_MAXVID,GetModuleHandle(NULL),0);
    if (!drpMaxVid) {outputControlOutputError("drpMaxVid");}
    sizeInUnit=determineSizeLimit(extractOpt.maxMovieSize,&units);
    populateSizeCmbo(drpMaxVid,units);
    snprintf(tempnum,sizeof(tempnum),"%llu",sizeInUnit);
    SetWindowText(MaxVidSize,tempnum);
    tempnum[0]='\0';

    txtMinVidSize = CreateWindowEx(0,"Static","Minimum Video size to export:",WS_CHILD|WS_VISIBLE|SS_RIGHT,LeftHandStartX + ((MainWindowWidth-100)/2),SecondLineY,250,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtMinVidSize) { outputControlOutputError("txtMinVidSize"); }

    MinVidSize = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","0",WS_CHILD|WS_VISIBLE,LeftHandStartX + ((MainWindowWidth-100)/2)+  260, SecondLineY ,60,24,hwnd,(HMENU)IDC_TEXT_MINVIDSIZE,GetModuleHandle(NULL),NULL);
    if (!MinVidSize) { outputControlOutputError("MinVidSize"); }


    drpMinVid = CreateWindowEx(0,"Combobox",NULL,WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|CBS_DROPDOWNLIST|CBS_HASSTRINGS,
                               LeftHandStartX + ((MainWindowWidth-100)/2)+ 320, SecondLineY ,45,20,hwnd,(HMENU)IDC_CBO_MINVID,GetModuleHandle(NULL),0);
    if (!drpMinVid) {outputControlOutputError("drpMinVid");}
    sizeInUnit=determineSizeLimit(extractOpt.minMovieSize,&units);
    populateSizeCmbo(drpMinVid,units);
    snprintf(tempnum,sizeof(tempnum),"%llu",sizeInUnit);
    SetWindowText(MinVidSize,tempnum);
    tempnum[0]='\0';

    return 0;
}

/**
 * @brief Creates the overwrite-files combo box on the third row of the options window.
 *
 * @param hwnd Handle to the parent options window.
 * @return 0 always.
 */
int drawThirdLine(HWND hwnd)
{
    lstTxtOverwrite = CreateWindowEx(0,"Static","Overwrite previously exported files?",WS_CHILD|WS_VISIBLE|SS_RIGHT,LeftHandStartX,ThirdLineY,250,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!lstTxtOverwrite) {outputControlOutputError("lstTxtOverwrite");}

    lstOverwrite = CreateWindowEx(WS_EX_CLIENTEDGE,"COMBOBOX","No",WS_CHILD|WS_VISIBLE|CBS_DROPDOWN,LeftHandStartX + 260, ThirdLineY ,60,20,hwnd,(HMENU)IDC_TEXT_CMBOVERWRITE,GetModuleHandle(NULL),NULL);
    if (!lstOverwrite) {outputControlOutputError("lstOverwrite");}

    SendMessage(lstOverwrite, CB_ADDSTRING, 0, (LPARAM)"No");
    SendMessage(lstOverwrite, CB_ADDSTRING, 0, (LPARAM)"Yes");
    int pos = 0;
    if (extractOpt.overwriteFiles) { pos = 1;}
    SendMessage(lstOverwrite,CB_SETCURSEL, pos, 0);

    return 0;
}

/**
 * @brief Creates the error report output path controls on the fourth row of the options window.
 *
 * @param hwnd Handle to the parent options window.
 * @return 0 always.
 */
int drawFourthLine(HWND hwnd)
{
    txtReportOutput = CreateWindowEx(0,"Static","Error Report Output Path",WS_CHILD|WS_VISIBLE|SS_RIGHT,0,FourthLineY,170,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtReportOutput) {outputControlOutputError("txtReportOutput");}

    ReportOutput = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE,LeftHandStartX+170, FourthLineY ,MainWindowWidth - 230,20,hwnd,(HMENU)IDC_TEXT_REPORTOUTPUT,GetModuleHandle(NULL),NULL);
    if (!ReportOutput) {outputControlOutputError("ReportOutput");}

    char tempPath[2048];
    snprintf(tempPath, sizeof(tempPath), "%ls",extractOpt.errorReportPath);
    SetWindowText(ReportOutput,tempPath);
    cmdReportOutput = CreateWindowEx(0,"BUTTON","...",WS_CHILD|WS_VISIBLE,MainWindowWidth - 50,FourthLineY,20,20,hwnd,(HMENU)IDC_BTN_REPORTOUTPUT,GetModuleHandle(NULL),0);
    if (!cmdReportOutput) {outputControlOutputError("cmdReportOutput");}

    return 0;
}

/**
 * @brief Creates the Griffeye CLI folder controls on the fifth row of the options window.
 *
 * @param hwnd Handle to the parent options window.
 * @return 0 always.
 */
int drawFifthLine(HWND hwnd)
{
    txtGriffeyeLocation = CreateWindowEx(0,"Static","Griffeye CLI Folder (opt.)",WS_CHILD|WS_VISIBLE|SS_RIGHT,0,FifthLineY,170,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtGriffeyeLocation) {outputControlOutputError("txtGriffeyeLocation");}

    char path[2048]={0};
    snprintf(path,sizeof(path),"%ls",extractOpt.GriffeyePath);
    GriffeyeLocation = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT",path,WS_CHILD|WS_VISIBLE,LeftHandStartX+170, FifthLineY ,MainWindowWidth - 230,20,hwnd,(HMENU)IDC_TEXT_GRIFFEYELOCATION,GetModuleHandle(NULL),NULL);
    if (!GriffeyeLocation) {outputControlOutputError("GriffeyeLocation");}

    cmdGriffeyeLocation = CreateWindowEx(0,"BUTTON","...",WS_CHILD|WS_VISIBLE,MainWindowWidth - 50,FifthLineY,20,20,hwnd,(HMENU)IDC_BTN_GRIFFEYE,GetModuleHandle(NULL),0);
    if (!cmdGriffeyeLocation) {outputControlOutputError("cmdGriffeyeLocation");}

    return 0;
}


/**
 * @brief Pre-selects items in the file format list box based on the saved flag value.
 *
 * @param fileTypeFlag Bitmask of file format flags to select.
 * @return 0 always.
 */
int selectFileType(int fileTypeFlag)
{
    if (fileTypeFlag & UNKNOWN){
        SendMessage(lstFileFormat,LB_SETSEL,TRUE,(LPARAM)0);
    }
    if (fileTypeFlag & OK){
        SendMessage(lstFileFormat,LB_SETSEL,TRUE,(LPARAM)1);
    }
    if (fileTypeFlag & IRREGULAR){
        SendMessage(lstFileFormat,LB_SETSEL,TRUE,(LPARAM)2);
    }
    if (fileTypeFlag & CORRUPT){
        SendMessage(lstFileFormat,LB_SETSEL,TRUE,(LPARAM)3);
    }
    return 0;
}

/**
 * @brief Pre-selects items in the type status list box based on the saved flag value.
 *
 * @param typeStatusFlag Bitmask of type status flags to select.
 * @return 0 always.
 */
int selectTypeStatus(int typeStatusFlag)
{
    if (typeStatusFlag & NOT_VERIFIED){
        SendMessage(lstFileStatus,LB_SETSEL,TRUE,(LPARAM)0);
    }
    if (typeStatusFlag & IRRELEVANT){
        SendMessage(lstFileStatus,LB_SETSEL,TRUE,(LPARAM)1);
    }
    if (typeStatusFlag & NOT_IN_LIST){
        SendMessage(lstFileStatus,LB_SETSEL,TRUE,(LPARAM)2);
    }
    if (typeStatusFlag & CONFIRMED){
        SendMessage(lstFileStatus,LB_SETSEL,TRUE,(LPARAM)3);
    }
    if (typeStatusFlag & NOT_CONFIRMED){
        SendMessage(lstFileStatus,LB_SETSEL,TRUE,(LPARAM)4);
    }
    if (typeStatusFlag & NEWLY_IDENTIFIED){
        SendMessage(lstFileStatus,LB_SETSEL,TRUE,(LPARAM)5);
    }
    if (typeStatusFlag & MISMATCH_DETECTED){
        SendMessage(lstFileStatus,LB_SETSEL,TRUE,(LPARAM)6);
    }
    return 0;
}

/**
 * @brief Creates the type status and file format list boxes and their labels.
 *
 * @param hwnd Handle to the parent options window.
 * @return 0 always.
 */
int drawTypeStatusWindows(HWND hwnd)
{
    txtFileStatus = CreateWindowEx(0,"Static","Type Status to be exported:",WS_CHILD|WS_VISIBLE|SS_CENTER,LeftHandStartX,TypeLineY,200,40,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtFileStatus) { outputControlOutputError("txtFileStatus"); }
    txtFileFormat = CreateWindowEx(0,"Static","File Format Consistency to be exported:",WS_CHILD|WS_VISIBLE|SS_CENTER,LeftHandStartX+225,TypeLineY,200,40,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtFileFormat) { outputControlOutputError("txtFileFormat"); }
    lstFileStatus = CreateWindowEx(0,"ListBox","",WS_CHILD|WS_VISIBLE|LBS_NOTIFY|WS_BORDER|LBS_EXTENDEDSEL,LeftHandStartX+30,TypeLineY+30,130,170,hwnd,(HMENU)IDC_LBX_TYPESTATUS,GetModuleHandle(NULL),0);
    if (!lstFileStatus) {outputControlOutputError("lstFileStatus");}
    for (int i=0;i<numTypeStatus;i++)
    {
        SendMessage(lstFileStatus,LB_ADDSTRING,0,(LPARAM)arrayTypeStatus[i]);
    }
    selectTypeStatus(extractOpt.TypeStatusFlags);
    lstFileFormat = CreateWindowEx(0,"ListBox","",WS_CHILD|WS_VISIBLE|LBS_NOTIFY|WS_BORDER|LBS_EXTENDEDSEL,LeftHandStartX + 250,TypeLineY+40,130,100,hwnd,(HMENU)IDC_LBX_FILEFORMAT,GetModuleHandle(NULL),0);
    if (!lstFileFormat) {outputControlOutputError("lstFileFormat");}
    for (int i=0;i<numFileFormat;i++)
    {
        SendMessage(lstFileFormat,LB_ADDSTRING,0,(LPARAM)arrayFileFormat[i]);
    }
    selectFileType(extractOpt.FileTypeFlag);
    return 0;
}

/**
 * @brief Creates the OK and Cancel buttons on the last row of the options window.
 *
 * @param hwnd Handle to the parent options window.
 * @return 0 always.
 */
int drawLastLine(HWND hwnd)
{
    //last line always contains the buttons
    cmdOK = CreateWindowEx(0,"BUTTON","&Ok",WS_CHILD|WS_VISIBLE,(((MainWindowWidth)/2)-70) ,LastLineY,50,20,hwnd,(HMENU)IDC_BTN_OK,GetModuleHandle(NULL),0);
    if (!cmdOK) {outputControlOutputError("cmdOK");}

    cmdCancel = CreateWindowEx(0,"BUTTON","&Cancel",WS_CHILD|WS_VISIBLE,(((MainWindowWidth)/2)+30) ,LastLineY,50,20,hwnd,(HMENU)IDC_BTN_CANCEL,GetModuleHandle(NULL),0);
    if (!cmdCancel) {outputControlOutputError("cmdCancel");}

    return 0;
}


/**
 * @brief Creates all child controls for the options window.
 *
 * @param hwnd Handle to the parent options window.
 *
 * @see createOptionsWindow
 */
void CreateOptionsControls(HWND hwnd)
{
    drawFirstLine(hwnd);
    drawSecondLine(hwnd);
    drawThirdLine(hwnd);
    drawFourthLine(hwnd);
    drawFifthLine(hwnd);
    drawTypeStatusWindows(hwnd);
    drawLastLine(hwnd);
}


/**
 * @brief Folder browser callback that sets the initial selection to the current path.
 */
static int CALLBACK BrowserCallbackProc(HWND hwnd,UINT uMsg,LPARAM lParam, LPARAM lpData)
{
    if (uMsg== BFFM_INITIALIZED)
    {
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
    }
    return 0;
}

/**
 * @brief Returns the path to the Clees4All AppData folder, creating it if it does not exist.
 *
 * The returned buffer is allocated with new[] and must be freed by the caller using delete[].
 *
 * @return Newly allocated path string, or nullptr if SHGetFolderPath fails.
 */
char* createOptionsFolderString()
{
    char* appdataPath = new char[MAX_PATH + 32];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA,NULL,0,appdataPath)))
    {
        strncat(appdataPath,"\\X-Ways\\",31);
        if (!dirExists(appdataPath))
        {
            CreateDirectory(appdataPath, NULL);
        }
        strncat(appdataPath,"Clees4All\\",31);
        if (!dirExists(appdataPath))
        {
            CreateDirectory(appdataPath, NULL);
        }
    }
    else{
        delete[] appdataPath;
        return nullptr;
    }
    return appdataPath;
}

/**
 * @brief Returns the path to the Clees4All AppData folder without creating missing directories.
 *
 * The returned buffer is allocated with new[] and must be freed by the caller using delete[].
 *
 * @return Newly allocated path string, or nullptr if SHGetFolderPath fails.
 */
char* generateOptionsFolderString()
{
    char* appdataPath = new char[MAX_PATH + 32];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA,NULL,0,appdataPath)))
    {
        strncat(appdataPath,"\\X-Ways\\Clees4All\\",31);
    }
    else{
        delete[] appdataPath;
        return nullptr;
    }
    return appdataPath;
}

/**
 * @brief Loads options from the SQLite database, creating it with defaults if it does not exist.
 *
 * @param success Output BOOL (unused, reserved for future error reporting).
 * @return Populated ExtractOptions struct.
 */
ExtractOptions loadOrCreateOptions(BOOL* success)
{
    ExtractOptions retOpt ={0};
    char* path = createOptionsFolderString();
    if (path == nullptr) {return retOpt;}
    //folder exists, check if options database does!
    char optPath[MAX_PATH];
    snprintf(optPath,sizeof(optPath),"%s\\%s",path,"opt.sqlite");
    strcpy(optionsDatabasePath, optPath);
    if (sqlDatabaseExists(optPath))
    {
        //database there
        retOpt = loadOptions(optPath);
    }
    else
    {
        createOptions(path);
        retOpt = loadOptions(optPath);
    }
    delete[] path;
    return retOpt;
}

/**
 * @brief Creates the options SQLite database at the given path.
 *
 * @param path Null-terminated path to the AppData folder where the database will be created.
 */
void createOptions(char* path)
{
    CreateDirectory(path, NULL);
    int rc = sqlCreateOptions(path);
    if (rc !=0)
    {
        wchar_t message[128];
        swprintf(message,128,L"Error creating SQL Options: %i",rc);
        MessageBoxW(NULL,message,L"Error",MB_ICONERROR);
    }
}

/**
 * @brief Saves the current extraction settings to the options database as the last-used record.
 *
 * @param record ExtractionDetails struct containing the settings to persist.
 * @return SQLite result code; SQLITE_OK (0) on success.
 */
int writeExtractionDetails(ExtractionDetails record)
{
    sqlite3 *sqlDB;
    char* path = createOptionsFolderString();
    if (path == nullptr) {return SQLITE_ERROR;}
    char optPath[MAX_PATH];
    snprintf(optPath,sizeof(optPath),"%s\\%s",path,"opt.sqlite");
    int rc = sqlite3_open_v2(optPath,&sqlDB,SQLITE_OPEN_FULLMUTEX|SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK){
        delete[] path;
        sqlite3_close(sqlDB);
        return rc;
    }
    //clear previous settings
    rc = clearExtractionDetails(sqlDB);
    //insert new 'last run' settings
    rc = insertExtractionDetails(sqlDB, &record);
    delete[] path;
    sqlite3_close(sqlDB);
    return rc;
}

/**
 * @brief Loads the last-used extraction settings from the options database into @p record.
 *
 * @param record Output struct to populate with the stored settings.
 * @return SQLite result code; SQLITE_OK (0) on success.
 */
int loadLastExtractionSettings(ExtractionDetails* record)
{
    sqlite3 *sqlDB;
    char* path = createOptionsFolderString();
    if (path == nullptr) {return SQLITE_ERROR;}
    char optPath[MAX_PATH];
    snprintf(optPath,sizeof(optPath),"%s\\%s",path,"opt.sqlite");
    int rc = sqlite3_open_v2(optPath,&sqlDB,SQLITE_OPEN_FULLMUTEX|SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK){
        delete[] path;
        sqlite3_close(sqlDB);
        return rc;
    }
    //load last settings
    rc = readExtractionSettings(sqlDB, record);
    delete[] path;
    sqlite3_close(sqlDB);
    return rc;
}


