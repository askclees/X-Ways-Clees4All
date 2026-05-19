//minimum windows versions
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#define WINVER _WIN32_WINNT_WIN7

//std libraries
#include <cstdio>
#include <wchar.h>
#include <string.h>
#include <ctime>
#include <string>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#include <climits>
#include <map>

//other modules
#include "main.h"
#include "VICS.h"
#include "debugMessage.h"

//1.41 added utility function
#include "utility.h"


//winapi controls
#define IDC_TEXT_PICTUREPATH        101
#define IDC_TEXT_VIDEOPATH          102
#define IDC_BTN_PICCHK              103
#define IDC_BTN_VIDCHK              104
#define IDC_BTN_OK                  105
#define IDC_BTN_VID                 106
#define IDC_BTN_PIC                 107
#define IDC_BTN_DBGCHK              108
#define IDC_BTN_VICCHK              109
#define IDC_BTN_C4PCHK              110
#define IDC_TEXT_GRIFFEYECASE       111
#define IDC_TEXT_GRIFFEYEPATH       112
#define IDC_BTN_GRIFFEYEPATH        113
#define IDC_BTN_GRIFFCHK            114
#define IDC_TEXT_GRIFFEYEINVNAME    115
#define IDC_TEXT_GRIFFEYEINVPHONE   116
#define IDC_TEXT_GRIFFEYEINVEMAIL   117
#define IDC_TEXT_GRIFFEYEINVTITLE   118
#define IDC_TEXT_GRIFFEYEINVORG     119
//1.40 added parent checkbox
#define IDC_BTN_PARENTCHK           120

//1.50 added zipped VICS file
#define IDC_BTN_VICSZIP             121
//1.50 added additional options
#define IDC_BTN_RPTCHK              122
#define IDC_BTN_EMBCHK              123
#define IDC_BTN_EMBMISCHK           124
#define IDC_LBL_EXTRACTOPT          125

#define IDC_TEXT_EVNAME 300
#define IDC_TEXT_NEW_NAME 600

//windows form sizes
#define MainWindowWidth 900
//1.50 increased to 450 for additional boxes
//1.51 increased again to 480 to add Griffeye settings option
#define MainWindowLength 480

//1.50 redefining
#define outputStart     10
#define optionsStart    65
#define optionsLine1    90
#define optionsLine2    115
#define optionsLine3    140
#define optionsColumn1  10
#define optionsColumn2  230
#define optionsColumn3  510

#define evidenceStartY  175

#define lblPicStartX    10
#define lblPicStartY    10
#define lblVidStartX    10
#define lblVidStartY    40
#define lblPicWidth     125
#define lblVidWidth     125
#define lblCheckWidth   150
#define boxMultiplier   25

HWND mainHwnd;
//1.40 added parentChkBox //1.50 added compChkBox
HWND PicturePath,VideoPath, btnOK,picChkBox,vidChkBox,parentChkBox,cmdVidSelect,cmdPicSelect,dbgChkBox, compChkBox, vicChkBox, C4PChkBox, GriffeyeCase, GriffeyePath;
HWND* TxtActualName, *TxtNewName, *lblActual, *lblNew;
HWND txtPicOutput,txtVidOutput, txtGriffeyeCaseName, txtGriffeyeCaseLocation, cmdGriffeyePath, griffChkBox;
//griffeye case details
HWND txtGriffeyeInvName, GriffeyeInvName, txtGriffeyeInvPhone, GriffeyeInvPhone, txtGriffeyeInvEmail, GriffeyeInvEmail, txtGriffeyeInvTitle, GriffeyeInvTitle, txtGriffeyeInvOrg, GriffeyeInvOrg;
//1.51 Added Griffeye settings file
HWND txtGriffeyeSettings, GriffeyeSettings;

//1.50 added additional checkboxes
HWND exEmbChkBox, exMisChkBox, rptTblChkBox, txtAddOptions, txtExtractOpts;
HWND toolReportChk;

char* CaseDir;

static HBRUSH hBrush = CreateSolidBrush(RGB(240,240,240));
//1.50 put version number in
WORD versionNumber;
//prototyping
void CreateControls(HWND hwnd);
BOOL DirectoryExists(LPCTSTR szPath);
LRESULT CALLBACK WindowProc (HWND hwnd, UINT uMsg, WPARAM wparam, LPARAM lParam);
static int CALLBACK BrowserCallbackProc(HWND hwnd,UINT uMsg,LPARAM lParam, LPARAM lpData);
LPWSTR GetFolderPath();
int startProcess();
void fillCaseDetails();
BOOL DirExists(LPCTSTR szPath);
int getGriffeyeDetails();
BOOL ifFileExists(char* path);
int saveGriffeyeDetails();

//1.51 String constant for ProgramData folder
wchar_t* GriffeyeConfigPath = L"C:\\ProgramData\\Griffeye Technologies\\Griffeye Analyze\\Data\\Config\\";

// Description:
//   Creates a tooltip for an item in a dialog box.
// Parameters:
//   idTool - identifier of an dialog box item.
//   nDlg - window handle of the dialog box.
//   pszText - string to use as the tooltip text.
// Returns:
//   The handle to the tooltip.
//
HWND CreateToolTip(int toolID, HWND hDlg, PTSTR pszText)
{
    if (!toolID || !hDlg || !pszText)
    {
        return FALSE;
    }
    // Get the window of the tool.
    HWND hwndTool = GetDlgItem(hDlg, toolID);

    // Create the tooltip. g_hInst is the global instance handle.
    HWND hwndTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL,
                              WS_POPUP |TTS_ALWAYSTIP | TTS_BALLOON,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              hDlg, NULL,
                              extractInfo.thisDLL, NULL);

   if (!hwndTool || !hwndTip)
   {
       return (HWND)NULL;
   }

    // Associate the tooltip with the tool.
    TOOLINFO toolInfo = { 0 };
    toolInfo.cbSize = sizeof(toolInfo);
    toolInfo.hwnd = hDlg;
    toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    toolInfo.uId = (UINT_PTR)hwndTool;
    toolInfo.lpszText = pszText;
    SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);

    return hwndTip;
}


int createWindow(WORD version)
{
    versionNumber = version;
    const char CLASS_NAME[] = "CLEES4All 1.60";
    WNDCLASSEX wc = {};

    wc.lpfnWndProc = WindowProc;
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

    mainHwnd =CreateWindowEx(
        0, //dwexstyle
        CLASS_NAME, //class name
        CLASS_NAME, //text for window
        WS_OVERLAPPEDWINDOW, //dwstyle
        100,//x
        100,//y
        MainWindowWidth,//width
        MainWindowLength + (extractInfo.noNames * (boxMultiplier+5)),//height
        NULL, //parent
        NULL,//hmenu
        NULL,//hinstance
        NULL// lpparam
    );

    if (mainHwnd == NULL)
    {
        return 0;
    }
    ShowWindow(mainHwnd, 5);
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0,0))
    {
        if (IsDialogMessage(mainHwnd,&msg)==0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return 0;
}

//button click functions
void btn_griffchk_unselect()
{
    EnableWindow(txtGriffeyeCaseLocation,FALSE);
    EnableWindow(GriffeyePath,FALSE);
    EnableWindow(GriffeyeSettings,FALSE);
    extractInfo.createGriffeye = FALSE;
}



LRESULT CALLBACK WindowProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
            CreateControls(hwnd);
            return 0;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
            case IDC_BTN_OK:
                {
                    if (!extractInfo.extractPictures && !extractInfo.extractVideos)
                    {
                        MessageBox(NULL,"You have to select at least one type of file to export!!","Error!! ",MB_ICONERROR);
                        break;
                    }
                    //1.50 add vics compressed as well
                    if (!extractInfo.C4ALLExport && !extractInfo.VICExport & !extractInfo.VICSCompressed)
                    {
                        MessageBox(NULL,"You have to select at least one output type!!","Error!! ",MB_ICONERROR);
                        break;
                    }
                    if (extractInfo.debugSet) {XWF_OutputMessage(L"CleesForAll Debug Msg: PreStartProcess",0); }
                    int error = startProcess();
                    if (extractInfo.debugSet) {XWF_OutputMessage(L"CleesForAll Debug Msg: PostStartProcess",0); }
                    if (error == 0)
                    {
                        if (extractInfo.debugSet) {XWF_OutputMessage(L"CleesForAll Debug Msg: PreSaveGriffeyeDetails",0); }
                        saveGriffeyeDetails();
                        if (extractInfo.debugSet) {XWF_OutputMessage(L"CleesForAll Debug Msg: PostSaveGriffeyeDetails",0); }
                        extractInfo.processStart = TRUE;

                        DestroyWindow(PicturePath);
                        DestroyWindow(VideoPath);
                        DestroyWindow(btnOK);
                        DestroyWindow(picChkBox);
                        DestroyWindow(vidChkBox);
                        //1.40 added parent check box
                        DestroyWindow(parentChkBox);
                        DestroyWindow(txtPicOutput);
                        DestroyWindow(txtVidOutput);
                        DestroyWindow(hwnd);
                    }
                }
                break;
            case IDC_BTN_PICCHK:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            if (SendDlgItemMessage(hwnd,IDC_BTN_PICCHK,BM_GETCHECK,0,0))
                            {
                                //checked
                                EnableWindow(txtPicOutput,TRUE);
                                EnableWindow(PicturePath,TRUE);
                                extractInfo.extractPictures = TRUE;
                            }
                            else
                            {
                                //not checked
                                EnableWindow(txtPicOutput,FALSE);
                                EnableWindow(PicturePath,FALSE);
                                extractInfo.extractPictures = FALSE;
                            }
                        break;
                    }
                }
                break;
            case IDC_BTN_VIDCHK:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            if (SendDlgItemMessage(hwnd,IDC_BTN_VIDCHK,BM_GETCHECK,0,0))
                            {
                                //checked
                                EnableWindow(txtVidOutput,TRUE);
                                EnableWindow(VideoPath,TRUE);
                                extractInfo.extractVideos = TRUE;
                            }
                            else
                            {
                                //not checked
                                EnableWindow(txtVidOutput,FALSE);
                                EnableWindow(VideoPath,FALSE);
                                extractInfo.extractVideos = FALSE;
                            }
                        break;
                    }
                }
                break;
            //1.40 added for additional checkbox
            case IDC_BTN_PARENTCHK:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            if (SendDlgItemMessage(hwnd,IDC_BTN_VIDCHK,BM_GETCHECK,0,0))
                            {
                                //checked
                                extractInfo.checkParent = TRUE;
                            }
                            else
                            {
                                //not checked
                                extractInfo.checkParent = FALSE;
                            }
                        break;
                    }
                }
                break;
            case IDC_BTN_DBGCHK:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            if (SendDlgItemMessage(hwnd,IDC_BTN_DBGCHK,BM_GETCHECK,0,0))
                            {
                                //checked
                                extractInfo.debugSet = TRUE;
                            }
                            else
                            {
                                //not checked
                                extractInfo.debugSet = FALSE;
                            }
                        break;
                    }
                }
                break;
            case IDC_BTN_C4PCHK:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            if (SendDlgItemMessage(hwnd,IDC_BTN_C4PCHK,BM_GETCHECK,0,0))
                            {
                                //checked
                                extractInfo.C4ALLExport = TRUE;
                                //1.50 change compressed to false
                                extractInfo.VICSCompressed = FALSE;
                                SendDlgItemMessage(hwnd,IDC_BTN_VICSZIP,BM_SETCHECK,0,0);
                            }
                            else
                            {
                                //not checked
                                extractInfo.C4ALLExport = FALSE;
                            }
                        break;
                    }
                }
                break;
            case IDC_BTN_VICCHK:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            if (SendDlgItemMessage(hwnd,IDC_BTN_VICCHK,BM_GETCHECK,0,0))
                            {
                                //checked
                                extractInfo.VICExport = TRUE;
                                //1.50 change compressed to false
                                extractInfo.VICSCompressed = FALSE;
                                SendDlgItemMessage(hwnd,IDC_BTN_VICSZIP,BM_SETCHECK,0,0);
                            }
                            else
                            {
                                //not checked
                                extractInfo.VICExport = FALSE;
                            }
                        break;
                    }
                }
                break;
            //1.50 added handler
            case IDC_BTN_VICSZIP:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            if (SendDlgItemMessage(hwnd,IDC_BTN_VICSZIP,BM_GETCHECK,0,0))
                            {
                                //checked
                                extractInfo.VICSCompressed = TRUE;
                                //1.50 change compressed to false
                                extractInfo.VICExport = FALSE;
                                extractInfo.C4ALLExport = FALSE;
                                SendDlgItemMessage(hwnd,IDC_BTN_VICCHK,BM_SETCHECK,0,0);
                                SendDlgItemMessage(hwnd,IDC_BTN_C4PCHK,BM_SETCHECK,0,0);
                                SendDlgItemMessage(hwnd,IDC_BTN_GRIFFCHK,BM_SETCHECK,0,0);
                                btn_griffchk_unselect();
                            }
                            else
                            {
                                //not checked
                                extractInfo.VICSCompressed = FALSE;
                            }
                        break;
                    }
                }
                break;
            case IDC_BTN_VID:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            {
                                LPWSTR outputFolder = GetFolderPath();
                                SetWindowTextW(VideoPath,outputFolder);
                            }
                            break;
                    }
                }
                break;
            case IDC_BTN_GRIFFEYEPATH:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            {
                                LPWSTR outputFolder = GetFolderPath();
                                SetWindowTextW(GriffeyePath,outputFolder);
                            }
                            break;
                    }
                }
                break;
            case IDC_BTN_GRIFFCHK:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            if (SendDlgItemMessage(hwnd,IDC_BTN_GRIFFCHK,BM_GETCHECK,0,0))
                            {
                                //checked
                                EnableWindow(txtGriffeyeCaseLocation,TRUE);
                                EnableWindow(txtGriffeyeCaseName,TRUE);
                                EnableWindow(GriffeyeCase,TRUE);
                                EnableWindow(GriffeyePath,TRUE);
                                EnableWindow(GriffeyeSettings,TRUE);
                                extractInfo.createGriffeye = TRUE;
                                extractInfo.VICSCompressed = FALSE;
                                SendDlgItemMessage(hwnd,IDC_BTN_VICSZIP,BM_SETCHECK,0,0);
                            }
                            else{
                                //not checked
                                btn_griffchk_unselect();
                            }
                        break;
                    }
                }
                break;
            case IDC_BTN_PIC:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            {
                                LPWSTR outputFolder = GetFolderPath();
                                SetWindowTextW(PicturePath,outputFolder);
                                SetFocus(hwnd);
                            }
                            break;
                    }
                }
                break;
            case IDC_BTN_EMBCHK:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            {
                                if (SendDlgItemMessage(hwnd,IDC_BTN_EMBCHK,BM_GETCHECK,0,0))
                                {
                                    //checked
                                    extractInfo.ignoreThumbs = TRUE;
                                    if (versionNumber >= 2050){
                                        EnableWindow(exMisChkBox, true);
                                    }
                                }
                                else
                                {
                                    //not checked
                                    extractInfo.ignoreThumbs = FALSE;
                                    if (versionNumber >= 2050){
                                        SendDlgItemMessage(hwnd,IDC_BTN_EMBMISCHK,BM_SETCHECK,0,0);
                                        EnableWindow(exMisChkBox, false);
                                    }
                                }
                            }
                            break;
                    }
                }
                break;
            case IDC_BTN_EMBMISCHK:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            {
                                if (SendDlgItemMessage(hwnd,IDC_BTN_EMBMISCHK,BM_GETCHECK,0,0))
                                {
                                    //checked
                                    extractInfo.exceptMismatch = TRUE;
                                }
                                else
                                {
                                    //not checked
                                    extractInfo.exceptMismatch = FALSE;
                                }
                            }
                            break;
                    }
                }
                break;
            case IDC_BTN_RPTCHK:
                {
                    switch (HIWORD(wParam))
                    {
                        case BN_CLICKED:
                            {
                                if (SendDlgItemMessage(hwnd,IDC_BTN_RPTCHK,BM_GETCHECK,0,0))
                                {
                                    //checked
                                    extractInfo.exportReportTables = TRUE;
                                }
                                else
                                {
                                    //not checked
                                    extractInfo.exportReportTables = FALSE;
                                }
                            }
                            break;
                    }
                }
                break;
            }
            return 0;
        case WM_DESTROY:
            DestroyWindow(PicturePath);
            DestroyWindow(VideoPath);
            DestroyWindow(btnOK);
            DestroyWindow(picChkBox);
            DestroyWindow(vidChkBox);
            //1.40 added parent check box
            DestroyWindow(parentChkBox);
            DestroyWindow(txtPicOutput);
            DestroyWindow(txtVidOutput);
            DestroyWindow(hwnd);
            PostQuitMessage(0);
            return 0;
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                FillRect(hdc, &ps.rcPaint, hBrush);
                EndPaint(hwnd, &ps);
            }
            return 0;
        break;
        case WM_GETMINMAXINFO:
            {
                MINMAXINFO FAR *mInfo = (MINMAXINFO FAR *)lParam;
                mInfo->ptMinTrackSize.y = (MainWindowLength+(extractInfo.noNames * boxMultiplier));//+(extractInfo.noNames * 30);
                mInfo->ptMinTrackSize.x = MainWindowWidth;
                mInfo->ptMaxTrackSize.y = (MainWindowLength+(extractInfo.noNames * boxMultiplier));
                mInfo->ptMaxTrackSize.x = MainWindowWidth * 2;
            }
        break;
        case WM_SIZE:
            {
                int newWidth = LOWORD(lParam);
                MoveWindow(PicturePath,lblPicStartX + lblPicWidth + 20,outputStart,newWidth - (lblPicStartX + lblPicWidth) - 60,20, TRUE);
                MoveWindow(cmdPicSelect,newWidth - 40,outputStart,20,20,TRUE);
                MoveWindow(VideoPath,lblVidStartX + lblVidWidth + 20,outputStart+25,newWidth - (lblVidStartX + lblPicWidth) - 60,20, TRUE);
                MoveWindow(cmdVidSelect,newWidth - 40,outputStart+25,20,20,TRUE);
            }
            return 0;
        break;
    }
    return DefWindowProc(hwnd,uMsg,wParam, lParam);
}

void setExtractionOptions(ExtractionDetails record)
{
    if (record.extractVideos){SendMessageA(vidChkBox,BM_SETCHECK,BST_CHECKED,0);}
    else {SendMessageA(vidChkBox,BM_SETCHECK,BST_UNCHECKED,0);}
    if (record.extractPictures){SendMessageA(picChkBox,BM_SETCHECK,BST_CHECKED,0);}
    else {SendMessageA(picChkBox,BM_SETCHECK,BST_UNCHECKED,0);}
    if (record.C4ALLExport){SendMessageA(C4PChkBox,BM_SETCHECK,BST_CHECKED,0);}
    else {SendMessageA(C4PChkBox,BM_SETCHECK,BST_UNCHECKED,0);}
    if (record.checkParent){SendMessageA(parentChkBox,BM_SETCHECK,BST_CHECKED,0);}
    else {SendMessageA(parentChkBox,BM_SETCHECK,BST_UNCHECKED,0);}
    if (record.createGriffeye){
            SendMessageA(griffChkBox,BM_SETCHECK,BST_CHECKED,0);
        }
    else {
        SendMessageA(griffChkBox,BM_SETCHECK,BST_UNCHECKED,0);
        EnableWindow(txtGriffeyeCaseLocation,FALSE);
        EnableWindow(GriffeyePath,FALSE);
        EnableWindow(GriffeyeSettings,FALSE);
    }
    if (record.VICExport){SendMessageA(vicChkBox,BM_SETCHECK,BST_CHECKED,0);}
    else {SendMessageA(vicChkBox,BM_SETCHECK,BST_UNCHECKED,0);}
    if (record.VICSCompressed){SendMessageA(compChkBox,BM_SETCHECK,BST_CHECKED,0);}
    else {SendMessageA(compChkBox,BM_SETCHECK,BST_UNCHECKED,0);}
    if (record.ignoreThumbs){
            SendMessageA(exEmbChkBox,BM_SETCHECK,BST_CHECKED,0);
            if (record.exceptMismatch){SendMessageA(exMisChkBox,BM_SETCHECK,BST_CHECKED,0);}
            else {SendMessageA(exMisChkBox,BM_SETCHECK,BST_UNCHECKED,0);}
    }
    else{
        SendMessageA(exEmbChkBox,BM_SETCHECK,BST_UNCHECKED,0);
        SendMessageA(exMisChkBox,BM_SETCHECK,BST_UNCHECKED,0);
    }
    if (record.exportReportTables){SendMessageA(rptTblChkBox,BM_SETCHECK,BST_CHECKED,0);}
    else {SendMessageA(rptTblChkBox,BM_SETCHECK,BST_UNCHECKED,0);}
    if(record.debugSet){SendMessageA(dbgChkBox,BM_SETCHECK,BST_CHECKED,0);}
}


bool createOuputControls(HWND hwnd, int startOffset)
{
    CaseDir = new char[512];
    wchar_t* CaseDirWide = new wchar_t[512];
    INT64 lenCaseDir = XWF_GetCaseProp(NULL,5,(PVOID)CaseDirWide,512);
    if (lenCaseDir < 0)
    {
        XWF_OutputMessage(L"Error retrieving case directory",0);
    }
    for (int i = lenCaseDir;i>0;i--)
    {
        if (CaseDirWide[i]==L'\\')
        {
            CaseDirWide[i] = L'\0';
            break;
        }
    }
    sprintf(CaseDir,"%ls\\Clees4All\\",CaseDirWide);
    delete[] CaseDirWide;
    PicturePath = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT",CaseDir,WS_CHILD|WS_VISIBLE,lblPicStartX + lblPicWidth + 20,startOffset,MainWindowWidth - (lblPicStartX + lblPicWidth) - 80,20,hwnd,(HMENU)IDC_TEXT_PICTUREPATH,GetModuleHandle(NULL),NULL);
    if (!PicturePath)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    VideoPath = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT",CaseDir,WS_CHILD|WS_VISIBLE,lblVidStartX + lblVidWidth + 20,startOffset+25,MainWindowWidth - (lblVidStartX + lblVidWidth) - 80,20,hwnd,(HMENU)IDC_TEXT_VIDEOPATH,GetModuleHandle(NULL),NULL);
    if (!VideoPath)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    txtPicOutput = CreateWindowEx(0,"Static","Picture Output Path",WS_CHILD|WS_VISIBLE|SS_RIGHT,lblPicStartX,startOffset,lblPicWidth,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtPicOutput)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Static Text Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    txtVidOutput = CreateWindowEx(0,"Static","Video Output Path",WS_CHILD|WS_VISIBLE|SS_RIGHT,lblVidStartX,startOffset+25,lblVidWidth,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtVidOutput)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Static Text Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    cmdPicSelect = CreateWindowEx(0,"BUTTON","...",WS_CHILD|WS_VISIBLE,MainWindowWidth - 40,startOffset,20,20,hwnd,(HMENU)IDC_BTN_PIC,GetModuleHandle(NULL),0);
    if (!cmdPicSelect)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"cmdVidSelect Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create button",MB_ICONERROR);
    }
    cmdVidSelect = CreateWindowEx(0,"BUTTON","...",WS_CHILD|WS_VISIBLE,MainWindowWidth - 60,startOffset,20,20,hwnd,(HMENU)IDC_BTN_VID,GetModuleHandle(NULL),NULL);
    if (!cmdVidSelect)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"cmdVidSelect Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create button",MB_ICONERROR);
    }
    return true;
}

bool createOptionsControls(HWND hwnd)
{
    txtExtractOpts = CreateWindowEx(0,"Static","Extraction Options",WS_CHILD|WS_VISIBLE,10,optionsStart,150,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtExtractOpts)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Static Text Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    picChkBox = CreateWindowEx(0,"BUTTON","Extract Pictures?",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,optionsColumn1,optionsLine1,lblCheckWidth,20,hwnd,(HMENU)IDC_BTN_PICCHK,GetModuleHandle(NULL),NULL);
    if (!picChkBox)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    vidChkBox = CreateWindowEx(0,"BUTTON","Extract Videos?",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,optionsColumn2,optionsLine1,lblCheckWidth,20,hwnd,(HMENU)IDC_BTN_VIDCHK,GetModuleHandle(NULL),NULL);
    if (!vidChkBox)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    //1.40 add parent checkbox
    //1.41 changed wording
    parentChkBox = CreateWindowEx(0,"BUTTON","Ignore media extracted from within live videos?",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,optionsColumn3,optionsLine1,lblCheckWidth+180,20,hwnd,(HMENU)IDC_BTN_PARENTCHK,GetModuleHandle(NULL),NULL);
    if (!parentChkBox)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    //line 2
    //HWND thmbChkBox, thmbMisChk, rptTblChkBox, txtAddOptions;
    rptTblChkBox = CreateWindowEx(0,"BUTTON","Export RTA as Metadata",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,optionsColumn1,optionsLine2,200,20,hwnd,(HMENU)IDC_BTN_RPTCHK,GetModuleHandle(NULL),NULL);
    if (!rptTblChkBox)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create debug checkbox",MB_ICONERROR);
    }
    //HWND thmbChkBox, thmbMisChk, rptTblChkBox, txtAddOptions;
    exEmbChkBox = CreateWindowEx(0,"BUTTON","Exclude embedded thumbnails",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,optionsColumn2,optionsLine2,250,20,hwnd,(HMENU)IDC_BTN_EMBCHK,GetModuleHandle(NULL),NULL);
    if (!exEmbChkBox)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create debug checkbox",MB_ICONERROR);
    }
    exMisChkBox = CreateWindowEx(0,"BUTTON","Except mismatches",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,optionsColumn3,optionsLine2,200,20,hwnd,(HMENU)IDC_BTN_EMBMISCHK,GetModuleHandle(NULL),NULL);
    if (!exMisChkBox)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create debug checkbox",MB_ICONERROR);
    }
    if (versionNumber < 2050){ EnableWindow(exMisChkBox, false);}
    //line 3
    dbgChkBox = CreateWindowEx(0,"BUTTON","Debug Mode",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,10,optionsLine3,120,20,hwnd,(HMENU)IDC_BTN_DBGCHK,GetModuleHandle(NULL),NULL);
    if (!dbgChkBox)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create debug checkbox",MB_ICONERROR);
    }
    return true;
}

bool createEvidenceEntries(HWND hwnd, int start)
{
    return true;
}


bool createGriffeyeEntries(HWND hwnd, int start)
{
    txtGriffeyeCaseName = CreateWindowEx(0,"Static","Griffeye Case Name:",WS_CHILD|WS_VISIBLE|SS_RIGHT,lblVidStartX,start,140,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtGriffeyeCaseName)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Static Text Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    wchar_t caseNameW[128] = {0};
    char caseName[128]={0};
    INT64 result = XWF_GetCaseProp(NULL,1,&caseNameW,128);
    sprintf(caseName,"%ls",caseNameW);
    GriffeyeCase = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT",caseName,WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_GROUP,lblVidStartX + 160,start,150,20,hwnd,(HMENU)IDC_TEXT_GRIFFEYECASE,GetModuleHandle(NULL),NULL);
    if (!GriffeyeCase)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    //start of griffeye options
    griffChkBox = CreateWindowEx(0,"BUTTON","Create Griffeye Case",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,MainWindowWidth - 200,start-5,200,30,hwnd,(HMENU)IDC_BTN_GRIFFCHK,GetModuleHandle(NULL),NULL);
    if (!griffChkBox)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create debug checkbox",MB_ICONERROR);
    }
    txtGriffeyeCaseLocation = CreateWindowEx(0,"Static","Griffeye Case Path:",WS_CHILD|WS_VISIBLE|SS_RIGHT,lblVidStartX,start + 25,140,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtGriffeyeCaseLocation)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Static Text Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    GriffeyePath = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE,lblVidStartX + 160,start + 25,MainWindowWidth - (lblVidStartX + 140) - 80,20,hwnd,(HMENU)IDC_TEXT_GRIFFEYEPATH,GetModuleHandle(NULL),NULL);
    if (!GriffeyePath)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    cmdGriffeyePath = CreateWindowEx(0,"BUTTON","...",WS_CHILD|WS_VISIBLE,MainWindowWidth - 60,start + 25,20,20,hwnd,(HMENU)IDC_BTN_GRIFFEYEPATH,GetModuleHandle(NULL),NULL);
    if (!cmdGriffeyePath)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"cmdVidSelect Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create button",MB_ICONERROR);
    }
    //griffeye case details
    txtGriffeyeInvName = CreateWindowEx(0,"Static","Investigator Name:",WS_CHILD|WS_VISIBLE|SS_RIGHT,lblVidStartX,start+50,140,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtGriffeyeInvName)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Static Text Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    wchar_t NameW[128] = {0};
    char Name[128]={0};
    result = XWF_GetCaseProp(NULL,3,&NameW,128);
    sprintf(Name,"%ls",NameW);
    GriffeyeInvName = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT",Name,WS_CHILD|WS_VISIBLE|WS_TABSTOP,lblVidStartX + 160,start+50,MainWindowWidth - (lblVidStartX + 140) - 350,20,hwnd,(HMENU)IDC_TEXT_GRIFFEYEINVNAME,GetModuleHandle(NULL),NULL);
    if (!GriffeyeInvName)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    txtGriffeyeInvTitle = CreateWindowEx(0,"Static","Investigator Title:",WS_CHILD|WS_VISIBLE|SS_RIGHT,lblVidStartX,start+75,140,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtGriffeyeInvTitle)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Static Text Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    GriffeyeInvTitle = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|WS_TABSTOP,lblVidStartX + 160,start+75,MainWindowWidth - (lblVidStartX + 140) - 350,20,hwnd,(HMENU)IDC_TEXT_GRIFFEYEINVTITLE,GetModuleHandle(NULL),NULL);
    if (!GriffeyeInvTitle)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    txtGriffeyeInvEmail = CreateWindowEx(0,"Static","Contact Email:",WS_CHILD|WS_VISIBLE|SS_RIGHT,lblVidStartX,start+100,140,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtGriffeyeInvEmail)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Static Text Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    GriffeyeInvEmail = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|WS_TABSTOP,lblVidStartX + 160,start+100,MainWindowWidth - (lblVidStartX + 140) - 350,20,hwnd,(HMENU)IDC_TEXT_GRIFFEYEINVEMAIL,GetModuleHandle(NULL),NULL);
    if (!GriffeyeInvEmail)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    txtGriffeyeInvPhone = CreateWindowEx(0,"Static","Contact Number:",WS_CHILD|WS_VISIBLE|SS_RIGHT,lblVidStartX,start+125,140,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtGriffeyeInvPhone)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Static Text Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    GriffeyeInvPhone = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|WS_TABSTOP,lblVidStartX + 160,start+125,MainWindowWidth - (lblVidStartX + 140) - 350,20,hwnd,(HMENU)IDC_TEXT_GRIFFEYEINVPHONE,GetModuleHandle(NULL),NULL);
    if (!GriffeyeInvPhone)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    txtGriffeyeInvOrg = CreateWindowEx(0,"Static","Organisation:",WS_CHILD|WS_VISIBLE|SS_RIGHT,lblVidStartX,start+150,140,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtGriffeyeInvOrg)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Static Text Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    GriffeyeInvOrg = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|WS_TABSTOP,lblVidStartX + 160,start+150,MainWindowWidth - (lblVidStartX + 140) - 350,20,hwnd,(HMENU)IDC_TEXT_GRIFFEYEINVORG,GetModuleHandle(NULL),NULL);
    if (!GriffeyeInvOrg)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    txtGriffeyeSettings = CreateWindowEx(0,"Static","Griffeye Settings File:",WS_CHILD|WS_VISIBLE|SS_RIGHT,lblVidStartX,start+175,140,20,hwnd,0,GetModuleHandle(NULL),0);
    if (!txtGriffeyeSettings)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Static Text Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    GriffeyeSettings = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|WS_TABSTOP,lblVidStartX + 160,start+175,MainWindowWidth - (lblVidStartX + 140) - 350,20,hwnd,(HMENU)IDC_TEXT_GRIFFEYEINVORG,GetModuleHandle(NULL),NULL);
    if (!GriffeyeSettings)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    return true;
}

void CreateControls(HWND hwnd)
{
    createOuputControls(hwnd,outputStart);
    createOptionsControls(hwnd);
    createGriffeyeEntries(hwnd,MainWindowLength - 280 + (extractInfo.noNames*boxMultiplier));

    btnOK= CreateWindowEx(0,"BUTTON","&OK",WS_CHILD|WS_VISIBLE|WS_TABSTOP,(MainWindowWidth / 2)-20,MainWindowLength - 80 + (extractInfo.noNames*boxMultiplier),40,40,hwnd,(HMENU)IDC_BTN_OK,GetModuleHandle(NULL),NULL);
    if (!btnOK)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"OK Button Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
    }
    vicChkBox = CreateWindowEx(0,"BUTTON","Export VICS Format",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,MainWindowWidth - 300,MainWindowLength - 200 + (extractInfo.noNames*boxMultiplier),200,40,hwnd,(HMENU)IDC_BTN_VICCHK,GetModuleHandle(NULL),NULL);
    if (!vicChkBox)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create debug checkbox",MB_ICONERROR);
    }
    compChkBox = CreateWindowEx(0,"BUTTON","Export VICS Format (Compressed)",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,MainWindowWidth - 300,MainWindowLength - 170 + (extractInfo.noNames*boxMultiplier),300,40,hwnd,(HMENU)IDC_BTN_VICSZIP,GetModuleHandle(NULL),NULL);
    if (!compChkBox)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create debug checkbox",MB_ICONERROR);
    }
    C4PChkBox = CreateWindowEx(0,"BUTTON","Export C4P XML",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,MainWindowWidth - 300,MainWindowLength - 140 + (extractInfo.noNames*boxMultiplier),200,40,hwnd,(HMENU)IDC_BTN_C4PCHK,GetModuleHandle(NULL),NULL);
    if (!dbgChkBox)
    {
        int result=GetLastError();
        char message[2048];
        sprintf(message,"Textbox Creation Error: %d",result);
        MessageBox(NULL,message,"Failed to create debug checkbox",MB_ICONERROR);
    }
    TxtActualName = new HWND[extractInfo.noNames];
    TxtNewName = new HWND[extractInfo.noNames];
    lblActual = new HWND[extractInfo.noNames];
    lblNew = new HWND[extractInfo.noNames];
    for (int i=0; i< extractInfo.noNames;i++)
    {
        char* txtEvCurr;
        txtEvCurr = new char[wcslen(extractInfo.nameList[i].actualName)+2];
        txtEvCurr[0] = '\0';
        sprintf(txtEvCurr,"%ls",extractInfo.nameList[i].actualName);
        lblActual[i] = CreateWindowEx(0,"Static","Evidence Name:",WS_CHILD|WS_VISIBLE|SS_RIGHT,lblVidStartX - 10,evidenceStartY + (i*boxMultiplier),140,30,hwnd,0,GetModuleHandle(NULL),0);
        if (!txtVidOutput)
        {
            int result=GetLastError();
            char message[2048];
            sprintf(message,"Static Text Creation Error: %d",result);
            MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
        }
        TxtActualName[i] = CreateWindowEx(WS_EX_CLIENTEDGE,"Static",txtEvCurr,WS_CHILD|WS_VISIBLE,lblVidStartX + 140,evidenceStartY + (i*boxMultiplier),210,20,hwnd,(HMENU)(IDC_TEXT_NEW_NAME+i),GetModuleHandle(NULL),NULL);
        if (!TxtActualName[i])
        {
            int result=GetLastError();
            char message[2048];
            sprintf(message,"Textbox Creation Error: %d",result);
            MessageBox(NULL,message,"Failed to create debug checkbox",MB_ICONERROR);
        }
        lblNew[i] = CreateWindowEx(0,"Static","Source ID Name:",WS_CHILD|WS_VISIBLE|SS_RIGHT,(MainWindowWidth/2) - 20,evidenceStartY + (i*boxMultiplier),130,20,hwnd,0,GetModuleHandle(NULL),0);
        if (!txtVidOutput)
        {
            int result=GetLastError();
            char message[2048];
            sprintf(message,"Static Text Creation Error: %d",result);
            MessageBox(NULL,message,"Failed to create textbox",MB_ICONERROR);
        }
        TxtNewName[i] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT",txtEvCurr,WS_CHILD|WS_VISIBLE,(MainWindowWidth/2)+120,evidenceStartY + (i*boxMultiplier),190,20,hwnd,(HMENU)(IDC_TEXT_NEW_NAME+i),GetModuleHandle(NULL),NULL);
        if (!TxtActualName[i])
        {
            int result=GetLastError();
            char message[2048];
            sprintf(message,"Textbox Creation Error: %d",result);
            MessageBox(NULL,message,"Failed to create debug checkbox",MB_ICONERROR);
        }
        delete[] txtEvCurr;
    }
    getGriffeyeDetails();
    setExtractionOptions(extractInfo);
}


BOOL DirectoryExists(LPCTSTR szPath)
{
    DWORD dwAttrib = GetFileAttributes(szPath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}


static int CALLBACK BrowserCallbackProc(HWND hwnd,UINT uMsg,LPARAM lParam, LPARAM lpData)
{
    if (uMsg== BFFM_INITIALIZED)
    {
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
    }
    return 0;
}

LPWSTR GetFolderPath()
{
    // CoCreate the File Open Dialog object.
    IFileDialog *pfd = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog,
                      NULL,
                      CLSCTX_INPROC_SERVER,
                      IID_PPV_ARGS(&pfd));
    LPWSTR pszFilePath = NULL;
    if (SUCCEEDED(hr))
    {
        if (SUCCEEDED(hr))
        {
            // Set the options on the dialog.
            DWORD dwFlags;

            // Before setting, always get the options first in order
            // not to override existing options.
            hr = pfd->GetOptions(&dwFlags);
            if (SUCCEEDED(hr))
            {
                // In this case, get shell items only for file system items.
                hr = pfd->SetOptions(dwFlags | FOS_PICKFOLDERS);
                if (SUCCEEDED(hr))
                {
                    // Set the file types to display only.
                    // Notice that this is a 1-based array.
                    // Show the dialog
                    hr = pfd->Show(NULL);
                    if (SUCCEEDED(hr))
                    {
                        // Obtain the result once the user clicks
                        // the 'Open' button.
                        // The result is an IShellItem object.
                        IShellItem *psiResult;
                        hr = pfd->GetResult(&psiResult);
                        if (SUCCEEDED(hr))
                        {
                            // We are just going to print out the
                            // name of the file for sample sake.
                            hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH,
                                               &pszFilePath);
                            psiResult->Release();
                        }
                    }
                }
            }
        }
    }
    return pszFilePath;
}


int startProcess()
{
    char buffer[1024];
    if(extractInfo.createGriffeye)
    {
        int length = GetWindowTextLength(GriffeyeCase);
        if (length == 0)
        {
            MessageBox(NULL,"Griffeye case must be filled in","Error!! ",MB_ICONERROR);
            return 1;
        }
        length = GetWindowTextLength(GriffeyePath);
        GetWindowText(GriffeyePath,buffer,1024);
        if(!DirectoryExists(buffer))
        {
            MessageBox(NULL,"Griffeye Case Location does not exist","Error!! ",MB_ICONERROR);
            return 1;
        }
        extractInfo.GriffeyeCaseLocation = new wchar_t[length + 40];
        swprintf(extractInfo.GriffeyeCaseLocation,L"%s",buffer);
        buffer[0]='\0';
        //1.51 get settings file
        length = GetWindowTextLength(GriffeyeSettings);
        GetWindowText(GriffeyeSettings,buffer,1024);
        if (length != 0){
            wchar_t pathBuffer[2048];
            swprintf(pathBuffer,L"%ls%s",GriffeyeConfigPath,buffer);
            if (ifFileExistsW(pathBuffer)){
                extractInfo.GriffeyeSettingsName = new wchar_t[length + 10];
                swprintf(extractInfo.GriffeyeSettingsName,L"%s",buffer);
                buffer[0]='\0';
            }
            else{
                MessageBox(NULL,"Griffeye Settings File does not exist","Error!! ",MB_ICONERROR);
                return 1;
            }
        }
        //extract Griffeye case name
        length = GetWindowTextLength(GriffeyeCase);
        GetWindowText(GriffeyeCase,buffer,1024);
        extractInfo.GriffeyeCaseName = new wchar_t[length + 10];
        swprintf(extractInfo.GriffeyeCaseName,L"%s",buffer);
        buffer[0]='\0';
    }
    if (extractInfo.extractPictures)
    {
        int length = GetWindowTextLength(PicturePath);
        GetWindowText(PicturePath,buffer,1024);
        if(!DirectoryExists(buffer))
        {
            if (strcmp(buffer,CaseDir)==0)
            {
                int res = MessageBox(NULL,"Do you want to create the output in the X-Ways case folder","Create Folder?",MB_YESNO);
                if (res == IDNO)
                {
                    return 1;
                }
                else if (res == IDYES)
                {
                    CreateDirectoryA(buffer,NULL);
                }
            }
            else
            {
                MessageBox(NULL,"Picture Path does not exist or cannot be accessed","Dumbass!! ",MB_ICONERROR);
                return 1;
            }
        }
        if (buffer[length-1]!= '\\')
        {
            strcat(buffer,"\\");
        }
        swprintf(extractInfo.C4PPath,L"%s",buffer);
        //1.50 only create if not just ZIP
        if (extractInfo.C4ALLExport || extractInfo.VICExport)
        {
            strcat(buffer,"Files");
            CreateDirectoryA(buffer,NULL);
        }
        buffer[0]='\0';
    }
    if (extractInfo.extractVideos)
    {
        int length = GetWindowTextLength(VideoPath);
        GetWindowText(VideoPath,buffer,1024);
        if(!DirectoryExists(buffer))
        {
            if (strcmp(buffer,CaseDir)==0)
            {
                int res = MessageBox(NULL,"Do you want to create the output in the X-Ways case folder","Create Folder?",MB_YESNO);
                if (res == IDNO)
                {
                    return 1;
                }
                else if (res == IDYES)
                {
                    CreateDirectoryA(buffer,NULL);
                }
            }
            else
            {
                MessageBox(NULL,"Video Path does not exist or cannot be accessed","Error",MB_ICONERROR);
                return 1;
            }
        }
        if (buffer[length-1]!= '\\')
        {
            strcat(buffer,"\\");
        }
        swprintf(extractInfo.C4MPath,L"%s",buffer);
        //1.50 only create if not just ZIP
        if (extractInfo.C4ALLExport || extractInfo.VICExport)
        {
            strcat(buffer,"Files");
            CreateDirectoryA(buffer,NULL);

        }
        buffer[0]='\0';
    }
    //set prefnames
    for (int i = 0;i<extractInfo.noNames;i++)
    {
        GetWindowText(TxtNewName[i],buffer,1024);
        swprintf(extractInfo.nameList[i].prefName,L"%s",buffer);
        buffer[0]='\0';
    }
    HRESULT error = CoCreateGuid(&vCaseData.caseGuid);
    if (extractInfo.debugSet) {debugWriteDetails("CleesForAll Debug Msg: PreFillCaseDetails"); }
    fillCaseDetails();
    if (extractInfo.debugSet) {debugWriteDetails("CleesForAll Debug Msg: PostFillCaseDetails"); }
    return 0;
}

void fillCaseDetails()
{
    int fieldLength = GetWindowTextLength(GriffeyeInvName);
    if (fieldLength > 0)
    {
        vCaseData.ContactName = new wchar_t[fieldLength + 2];
        GetWindowTextW(GriffeyeInvName,vCaseData.ContactName,fieldLength+1);
    }
    fieldLength = GetWindowTextLength(GriffeyeInvEmail);
    if (fieldLength > 0)
    {
        vCaseData.ContactEmail = new wchar_t[fieldLength + 2];
        GetWindowTextW(GriffeyeInvEmail,vCaseData.ContactEmail,fieldLength+1);
    }
    fieldLength = GetWindowTextLength(GriffeyeInvOrg);
    if (fieldLength > 0)
    {
        vCaseData.ContactOrg = new wchar_t[fieldLength + 2];
        GetWindowTextW(GriffeyeInvOrg,vCaseData.ContactOrg,fieldLength+1);
    }
    fieldLength = GetWindowTextLength(GriffeyeInvPhone);
    if (fieldLength > 0)
    {
        vCaseData.ContactPhone = new wchar_t[fieldLength + 2];
        GetWindowTextW(GriffeyeInvPhone,vCaseData.ContactPhone,fieldLength+1);
    }
    fieldLength = GetWindowTextLength(GriffeyeInvTitle);
    if (fieldLength > 0)
    {
        vCaseData.ContactTitle = new wchar_t[fieldLength + 2];
        GetWindowTextW(GriffeyeInvTitle,vCaseData.ContactTitle,fieldLength+1);
    }
    fieldLength = GetWindowTextLength(GriffeyeCase);
    if (fieldLength > 0)
    {
        vCaseData.CaseNumber = new wchar_t[fieldLength + 2];
        GetWindowTextW(GriffeyeCase,vCaseData.CaseNumber,fieldLength+1);
    }
}

int getGriffeyeDetails()
{
    char appdataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA,NULL,0,appdataPath)))
    {
        strcat(appdataPath,"\\X-Ways\\");
        if (!DirectoryExists(appdataPath))
        {
            CreateDirectoryA(appdataPath, NULL);
        }
        strcat(appdataPath,"Clees4All\\");
        if (DirectoryExists(appdataPath))
        {
            //folder exists, check if options database does!
            char optPath[MAX_PATH];
            sprintf(optPath,"%s\\%s",appdataPath,"griffeyeDetails.txt");
            if (ifFileExists(optPath))
            {
                //file exists, read lines
                FILE* detailsFile = fopen(optPath,"r");
                char line[256];
                int len;
                //currently working on 4 lines
                if (fgets(line,sizeof(line),detailsFile)!=NULL)
                {
                    //line 1 - inv title
                    len = strlen(line);
                    if (line[len-1] == '\n')
                    {
                        line[len-1] = '\0';
                    }
                    SetWindowText(GriffeyeInvTitle,line);
                }
                if (fgets(line,sizeof(line),detailsFile)!=NULL)
                {
                    //line 2 - contact email
                    len = strlen(line);
                    if (line[len-1] == '\n')
                    {
                        line[len-1] = '\0';
                    }
                    SetWindowText(GriffeyeInvEmail,line);
                }
                if (fgets(line,sizeof(line),detailsFile)!=NULL)
                {
                    //line 3 - phone number
                    len = strlen(line);
                    if (line[len-1] == '\n')
                    {
                        line[len-1] = '\0';
                    }
                    SetWindowText(GriffeyeInvPhone,line);
                }
                if (fgets(line,sizeof(line),detailsFile)!=NULL)
                {
                    //line 2 - organisation
                    len = strlen(line);
                    if (line[len-1] == '\n')
                    {
                        line[len-1] = '\0';
                    }
                    SetWindowText(GriffeyeInvOrg,line);
                }
            }
        }
    }
    return 0;
}

int saveGriffeyeDetails()
{
    char appdataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA,NULL,0,appdataPath)))
    {
        strcat(appdataPath,"\\X-Ways\\");
        if (!DirExists(appdataPath))
        {
            CreateDirectoryA(appdataPath, NULL);
        }
        strcat(appdataPath,"Clees4All\\");
        if (DirectoryExists(appdataPath))
        {
            //folder exists, check if options database does!
            char optPath[MAX_PATH];
            sprintf(optPath,"%s\\%s",appdataPath,"griffeyeDetails.txt");
            FILE* details = fopen(optPath,"w");
            char item[256];
            GetWindowText(GriffeyeInvTitle,item,256);
            fprintf(details,"%s\n",item);
            GetWindowText(GriffeyeInvEmail,item,256);
            fprintf(details,"%s\n",item);
            GetWindowText(GriffeyeInvPhone,item,256);
            fprintf(details,"%s\n",item);
            GetWindowText(GriffeyeInvOrg,item,256);
            fprintf(details,"%s\n",item);
            fclose(details);
        }
    }
    return 0;
}



