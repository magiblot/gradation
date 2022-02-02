/*
    Gradation Curves Filter v1.46 for VirtualDub -- a wide range of color
    manipulation through gradation curves.
    Copyright (C) 2008 Alexander Nagiller
    Speed optimizations for HSV and CMYK by Achim Stahlberger.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "gradation.h"
#include "resource.h"

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include <string>
#include <sstream>

#include "ScriptInterpreter.h"
#include "ScriptError.h"
#include "ScriptValue.h"
#include "Filter.h"

///////////////////////////////////////////////////////////////////////////

static int RunProc(const FilterActivation *fa, const FilterFunctions *ff);
static int StartProc(FilterActivation *fa, const FilterFunctions *ff);
static int EndProc(FilterActivation *fa, const FilterFunctions *ff);
static int InitProc(FilterActivation *fa, const FilterFunctions *ff);
static void DeinitProc(FilterActivation *fa, const FilterFunctions *ff);
static int ConfigProc(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd);
static void StringProc(const FilterActivation *fa, const FilterFunctions *ff, char *str);
static void ScriptConfig(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc);
static bool FssProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen);

///////////////////////////////////////////////////////////////////////////

static HINSTANCE hInst;

static LRESULT CALLBACK FiWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HINSTANCE g_hInst;

static const char BBFilterWindowName[]="BBFilterWindow";

bool WINAPI DllMain(HINSTANCE hInst, ULONG ulReason, LPVOID lpReserved) {
    g_hInst = hInst;
    return TRUE;
}

static ATOM RegisterFilterControl() {
    WNDCLASS wc;

    wc.style        = CS_DBLCLKS;
    wc.lpfnWndProc  = FiWndProc;
    wc.cbClsExtra   = 0;
    wc.cbWndExtra   = 0;
    wc.hInstance    = g_hInst;
    wc.hIcon        = NULL;
    wc.hCursor      = LoadCursor(NULL, IDC_CROSS);
    wc.hbrBackground= (HBRUSH)(COLOR_3DFACE+1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName= BBFilterWindowName;

    return RegisterClass(&wc);
};

static LRESULT CALLBACK FiWndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){ //curve box
    switch(message){
    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON)
        {SendMessage((GetParent (hwnd)),WM_USER + 100,(LOWORD (lParam)),(HIWORD (lParam)));}
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture (hwnd);
        SendMessage((GetParent (hwnd)),WM_USER + 101,(LOWORD (lParam)),(HIWORD (lParam)));
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture ();
        SendMessage((GetParent (hwnd)),WM_USER + 102,0,0);
        return 0;
    case WM_RBUTTONDOWN:
        SendMessage((GetParent (hwnd)),WM_USER + 103,(LOWORD (lParam)),(HIWORD (lParam)));
        return 0;
    }
    return DefWindowProc (hwnd, message, wParam, lParam) ;
}

struct MyFilterData : Gradation {
    IFilterPreview *ifp;
    int value;
};

static ScriptFunctionDef func_defs[]={
    { (ScriptFunctionPtr)ScriptConfig, "Config", "0is" },
    { (ScriptFunctionPtr)ScriptConfig, NULL,     "0iss" },
    { NULL },
};

static CScriptObject script_obj={
    NULL, func_defs
};

static FilterDefinition filterDef = {

    NULL, NULL, NULL,       // next, prev, module
    "gradation curves",     // name
    "Version 1.46 Beta Adjustment of contrast, brightness, gamma and a wide range of color manipulations through gradation curves is possible. Speed optimizations for HSV and CMYK by Achim Stahlberger.",
                            // desc
    "Alexander Nagiller",   // maker
    NULL,                   // private_data
    sizeof(MyFilterData),   // inst_data_size

    InitProc,               // initProc
    DeinitProc,             // deinitProc
    RunProc,                // runProc
    NULL,                   // paramProc
    ConfigProc,             // configProc
    StringProc,             // stringProc
    StartProc,              // startProc
    EndProc,                // endProc

    &script_obj,            // script_obj
    FssProc,                // fssProc

};

extern "C" int __declspec(dllexport) __cdecl VirtualdubFilterModuleInit2(FilterModule *fm, const FilterFunctions *ff, int& vdfd_ver, int& vdfd_compat);
extern "C" void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(FilterModule *fm, const FilterFunctions *ff);

static FilterDefinition *fd;

int __declspec(dllexport) __cdecl VirtualdubFilterModuleInit2(FilterModule *fm, const FilterFunctions *ff, int& vdfd_ver, int& vdfd_compat) {
    InitCommonControls();
    RegisterFilterControl();

    if (!(fd = ff->addFilter(fm, &filterDef, sizeof(FilterDefinition))))
    return 1;

    hInst = fm->hInstModule;
    vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
    vdfd_compat = VIRTUALDUB_FILTERDEF_COMPATIBLE;

    return 0;
}

void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(FilterModule *fm, const FilterFunctions *ff) {
    ff->removeFilter(fd);
}

///////////////////////////////////////////////////////////////////////////

static void GrdDrawGradTable(HWND hWnd, int table[], int laboff, DrawMode dmode, int dp[16][2], int pc, int ap);
static void GrdDrawBorder(HWND hWnd, HWND hWnd2, MyFilterData *mfd);

///////////////////////////////////////////////////////////////////////////

static int StartProc(FilterActivation *fa, const FilterFunctions *) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    return StartProcImpl(*mfd);
}

static int RunProc(const FilterActivation *fa, const FilterFunctions *) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    PixDim width = fa->src.w;
    PixDim height = fa->src.h;
    uint32_t *src = (uint32_t *)fa->src.data;
    uint32_t *dst = (uint32_t *)fa->dst.data;
    PixOffset src_modulo = fa->src.modulo;
    PixOffset dst_modulo = fa->dst.modulo;
    return RunProcImpl(*mfd, width, height, src, dst, src_modulo, dst_modulo);
}

static int EndProc(FilterActivation *, const FilterFunctions *) {
    return 0;
}

static void DeinitProc(FilterActivation *, const FilterFunctions *) {
    UnregisterClass("FiWndProc", g_hInst);
}

static int InitProc(FilterActivation *fa, const FilterFunctions *) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    mfd->value = 0;
    return InitProcImpl(*mfd);
}

#ifdef _WIN64
typedef INT_PTR DLGPROC_RET;
#define GetWindowUserData(h) GetWindowLongPtr((h), DWLP_USER)
#define SetWindowUserData(h, p) SetWindowLongPtr((h), DWLP_USER, (p))
#else
typedef BOOL DLGPROC_RET;
#define GetWindowUserData(h) GetWindowLong((h), DWL_USER)
#define SetWindowUserData(h, p) SetWindowLong((h), DWL_USER, (p))
#endif

static DLGPROC_RET CALLBACK ConfigDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    MyFilterData *mfd = (MyFilterData *)GetWindowUserData(hdlg);
    int inv[256];
    int invp[16][2];
    int cx;
    int cy;
    int ax;
    int ay;
    int delta[256];
    int a;
    int i;
    int j;
    int tmpx=0;
    int tmpy=0;
    bool stp;
    bool ptp;
    int b;
    int max;
    int min;
    int mode;
    int spacemode;
    RECT rect;
    HWND hWnd;
    HCURSOR hCursor;
    HWND hWndBtn1 = NULL;
    HWND hWndBtn2 = NULL;
    HWND hWndBtn3 = NULL;
    HWND hWndBtn4 = NULL;
    HICON hIco1;
    HICON hIco2;
    HICON hIco3;
    HICON hIco4;

    switch(msg) {
        case WM_INITDIALOG:
            SetWindowUserData(hdlg, lParam);
            mfd = (MyFilterData *)lParam;

            hWndBtn1 = GetDlgItem(hdlg, IDC_RADIOPM);
            hIco1 = LoadIcon(hInst, (LPCSTR)IDI_ICON1);
            SendMessage(hWndBtn1, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIco1);
            hWndBtn2 = GetDlgItem(hdlg, IDC_RADIOLM);
            hIco2 = LoadIcon(hInst, (LPCSTR)IDI_ICON2);
            SendMessage(hWndBtn2, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIco2);
            hWndBtn3 = GetDlgItem(hdlg, IDC_RADIOSM);
            hIco3 = LoadIcon(hInst, (LPCSTR)IDI_ICON3);
            SendMessage(hWndBtn3, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIco3);
            hWndBtn4 = GetDlgItem(hdlg, IDC_RADIOGM);
            hIco4 = LoadIcon(hInst, (LPCSTR)IDI_ICON4);
            SendMessage(hWndBtn4, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIco4);

            hWnd = GetDlgItem(hdlg, IDC_SPACE);
            for(i=0; i<5; i++)
            {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)space_names[i]);}
            if (mfd->process > 4) mfd->space_mode = Space(mfd->process - 4);
            SendMessage(hWnd, CB_SETCURSEL, mfd->space_mode, 0);
            hWnd = GetDlgItem(hdlg, IDC_CHANNEL);
            switch (mfd->space_mode){
                case SPACE_RGB:
                    for(i=0; i<4; i++)
                    {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)RGBchannel_names[i]);}
                    mfd->channel_mode = Channel(0);
                    mfd->offset = 0;
                    mfd->laboff = 0;
                    hWnd = GetDlgItem(hdlg, IDC_RGB);
                    SetWindowText (hWnd,"RGB only");
                    hWnd = GetDlgItem(hdlg, IDC_FULL);
                    SetWindowText (hWnd,"RGB + R/G/B");
                    SendMessage (hWnd,BM_SETCHECK,0,0L);
                    hWnd = GetDlgItem(hdlg, IDC_RGBW);
                    SetWindowText (hWnd,"RGB weighted");
                    ShowWindow (hWnd, SW_SHOWNORMAL);
                    SendMessage (hWnd,BM_SETCHECK,0,0L);
                    hWnd = GetDlgItem(hdlg, IDC_FULLW);
                    SetWindowText (hWnd,"RGB weighted + R/G/B");
                    ShowWindow (hWnd, SW_SHOWNORMAL);
                    SendMessage (hWnd,BM_SETCHECK,0,0L);
                    hWnd = GetDlgItem(hdlg, IDC_OFF);
                    SetWindowText (hWnd,"no processing");
                    ShowWindow (hWnd, SW_SHOWNORMAL);
                    SendMessage (hWnd,BM_SETCHECK,0,0L);
                break;
                case SPACE_YUV:
                    for(i=0; i<3; i++)
                    {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)YUVchannel_names[i]);}
                    mfd->channel_mode = Channel(0);
                    mfd->offset = 1;
                    hWnd = GetDlgItem(hdlg, IDC_RGB);
                    SetWindowText (hWnd,"Y/U/V");
                break;
                case SPACE_CMYK:
                    for(i=0; i<4; i++)
                    {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)CMYKchannel_names[i]);}
                    mfd->channel_mode = Channel(0);
                    mfd->offset = 1;
                    hWnd = GetDlgItem(hdlg, IDC_RGB);
                    SetWindowText (hWnd,"C/M/Y/K");
                break;
                case SPACE_HSV:
                    for(i=0; i<3; i++)
                    {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)HSVchannel_names[i]);}
                    mfd->channel_mode = Channel(0);
                    mfd->offset = 1;
                    hWnd = GetDlgItem(hdlg, IDC_RGB);
                    SetWindowText (hWnd,"H/S/V");
                break;
                case SPACE_LAB:
                    for(i=0; i<3; i++)
                    {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)LABchannel_names[i]);}
                    mfd->channel_mode = Channel(0);
                    mfd->offset = 1;
                    hWnd = GetDlgItem(hdlg, IDC_RGB);
                    SetWindowText (hWnd,"L/a/b");
                break;
            }
            if (mfd->space_mode != SPACE_RGB) {
                hWnd = GetDlgItem(hdlg, IDC_FULL);
                SetWindowText (hWnd,"no processing");
                SendMessage (hWnd,BM_SETCHECK,0,0L);
                hWnd = GetDlgItem(hdlg, IDC_RGBW);
                ShowWindow (hWnd, SW_HIDE);
                SendMessage (hWnd,BM_SETCHECK,0,0L);
                hWnd = GetDlgItem(hdlg, IDC_FULLW);
                ShowWindow (hWnd, SW_HIDE);
                SendMessage (hWnd,BM_SETCHECK,0,0L);
                hWnd = GetDlgItem(hdlg, IDC_OFF);
                ShowWindow (hWnd, SW_HIDE);
                SendMessage (hWnd,BM_SETCHECK,0,0L);
            }
            hWnd = GetDlgItem(hdlg, IDC_CHANNEL);
            SendMessage(hWnd, CB_SETCURSEL, mfd->channel_mode, 0);
            mfd->channel_mode = Channel(SendDlgItemMessage(hdlg, IDC_CHANNEL, CB_GETCURSEL, 0, 0) + mfd->offset);
            if (mfd->drwmode[mfd->channel_mode]!=DRAWMODE_PEN){
                SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_GAMMA){
                    hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                    ShowWindow(hWnd, SW_SHOW);
                    SetWindowText(hWnd, mfd->gamma);
                    hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                    ShowWindow(hWnd, SW_SHOW);}
            }
            else {
                SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[0][mfd->value], FALSE);
                hWnd = GetDlgItem(hdlg, IDC_POINTMINUS);
                ShowWindow(hWnd, SW_HIDE);
                hWnd = GetDlgItem(hdlg, IDC_POINTPLUS);
                ShowWindow(hWnd, SW_HIDE);
                hWnd = GetDlgItem(hdlg, IDC_POINT);
                ShowWindow(hWnd, SW_HIDE);
                hWnd = GetDlgItem(hdlg, IDC_POINTNO);
                ShowWindow(hWnd, SW_HIDE);}
            switch (mfd->process)
            {
                case PROCESS_RGB: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); break;
                case PROCESS_FULL: CheckDlgButton(hdlg, IDC_FULL,BST_CHECKED); break;
                case PROCESS_RGBW: CheckDlgButton(hdlg, IDC_RGBW,BST_CHECKED); break;
                case PROCESS_FULLW: CheckDlgButton(hdlg, IDC_FULLW,BST_CHECKED); break;
                case PROCESS_OFF:
                    if (mfd->space_mode != SPACE_RGB) CheckDlgButton(hdlg, IDC_FULL,BST_CHECKED);
                    else CheckDlgButton(hdlg, IDC_OFF,BST_CHECKED); break;
                case PROCESS_YUV: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); mfd->space_mode = SPACE_YUV; break;
                case PROCESS_CMYK: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); mfd->space_mode = SPACE_CMYK; break;
                case PROCESS_HSV: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); mfd->space_mode = SPACE_HSV; break;
                case PROCESS_LAB: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); mfd->space_mode = SPACE_LAB; break;
            }
            switch  (mfd->drwmode[mfd->channel_mode]) {
                case DRAWMODE_PEN:
                    CheckDlgButton(hdlg, IDC_RADIOPM, BST_CHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                    break;
                case DRAWMODE_LINEAR:
                    CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOLM, BST_CHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                    break;
                case DRAWMODE_SPLINE:
                    CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOSM, BST_CHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                    break;
                case DRAWMODE_GAMMA:
                    CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOGM, BST_CHECKED);
                    break;
            }
            GetClientRect(GetDlgItem(hdlg, IDC_GRADCURVE), &rect);
            mfd->boxx=rect.right-rect.left-1;
            mfd->boxy=rect.bottom-rect.top;
            mfd->scalex=(256/double(mfd->boxx));
            mfd->scaley=(256/double(mfd->boxy));
            mfd->ifp->InitButton(GetDlgItem(hdlg, IDPREVIEW));
            return TRUE;
        case WM_PAINT:
            {   if (mfd->Labprecalc==0 && mfd->process==PROCESS_LAB) { // build up the LUT for the Lab process if it is not precalculated already
                    hCursor = LoadCursor(NULL, IDC_WAIT);
                    SetCursor (hCursor);
                    PreCalcLut();
                    mfd->Labprecalc = 1;
                    hCursor = LoadCursor(NULL, IDC_ARROW);
                    SetCursor (hCursor);}

                PAINTSTRUCT ps;

                BeginPaint(hdlg, &ps);
                EndPaint(hdlg, &ps);
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                GrdDrawBorder(GetDlgItem(hdlg, IDC_HBORDER), GetDlgItem(hdlg, IDC_VBORDER), mfd);
            }
            return TRUE;

        case WM_USER + 100:
            if (wParam > 32768) {ax = 0;}
            else if (wParam >= mfd->boxx) {ax = 255;}
            else {ax = int(wParam*mfd->scalex+0.5);}
            if (lParam > 32768) {ay = 255;}
            else if (lParam >= mfd->boxy) {ay = 0;}
            else {ay = 255-int(lParam*mfd->scaley+0.5);}
            if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN){
                mfd->ovalue[mfd->channel_mode][ax]=(ay);
                switch (mfd->channel_mode) { //for faster RGB modes
                    case 0:
                            mfd->rvalue[0][ax]=(mfd->ovalue[0][ax]<<16);
                            mfd->rvalue[2][ax]=(mfd->ovalue[0][ax]-ax)<<16;
                            mfd->gvalue[0][ax]=(mfd->ovalue[0][ax]<<8);
                            mfd->gvalue[2][ax]=(mfd->ovalue[0][ax]-ax)<<8;
                            mfd->bvalue[ax]=mfd->ovalue[0][ax]-ax;
                    break;
                    case 1:
                        mfd->rvalue[1][ax]=(mfd->ovalue[1][ax]<<16);
                    break;
                    case 2:
                        mfd->gvalue[1][ax]=(mfd->ovalue[2][ax]<<8);
                    break;
                }
                if (ax > mfd->xl)
                {
                    if ((ax-(mfd->xl))>1 && (ax-(mfd->xl))<256 && (mfd->xl)<256 && (mfd->yl)<256)
                    {
                        if (ay > (mfd->yl))
                        {
                            cy=(((ay-(mfd->yl))<<8)/(ax-(mfd->xl)));
                            for (cx=((mfd->xl)+1);cx<ax;cx++)
                            {
                                mfd->ovalue[mfd->channel_mode][cx]=((mfd->ovalue[mfd->channel_mode][mfd->xl])+(((cx-(mfd->xl))*cy)>>8));
                                switch (mfd->channel_mode) { //for faster RGB modes
                                    case 0:
                                            mfd->rvalue[0][cx]=(mfd->ovalue[0][cx]<<16);
                                            mfd->rvalue[2][cx]=(mfd->ovalue[0][cx]-cx)<<16;
                                            mfd->gvalue[0][cx]=(mfd->ovalue[0][cx]<<8);
                                            mfd->gvalue[2][cx]=(mfd->ovalue[0][cx]-cx)<<8;
                                            mfd->bvalue[cx]=mfd->ovalue[0][cx]-cx;
                                    break;
                                    case 1:
                                        mfd->rvalue[1][cx]=(mfd->ovalue[1][cx]<<16);
                                    break;
                                    case 2:
                                        mfd->gvalue[1][cx]=(mfd->ovalue[2][cx]<<8);
                                    break;
                                }
                            }
                        }
                        else
                        {
                            cy=(((mfd->yl)-ay)<<8)/(ax-(mfd->xl));
                            for (cx=((mfd->xl)+1);cx<ax;cx++)
                            {
                                mfd->ovalue[mfd->channel_mode][cx]=((mfd->ovalue[mfd->channel_mode][mfd->xl])-(((cx-(mfd->xl))*cy)>>8));
                                switch (mfd->channel_mode) { //for faster RGB modes
                                    case 0:
                                            mfd->rvalue[0][cx]=(mfd->ovalue[0][cx]<<16);
                                            mfd->rvalue[2][cx]=(mfd->ovalue[0][cx]-cx)<<16;
                                            mfd->gvalue[0][cx]=(mfd->ovalue[0][cx]<<8);
                                            mfd->gvalue[2][cx]=(mfd->ovalue[0][cx]-cx)<<8;
                                            mfd->bvalue[cx]=mfd->ovalue[0][cx]-cx;
                                    break;
                                    case 1:
                                        mfd->rvalue[1][cx]=(mfd->ovalue[1][cx]<<16);
                                    break;
                                    case 2:
                                        mfd->gvalue[1][cx]=(mfd->ovalue[2][cx]<<8);
                                    break;
                                }
                            }
                        }
                    }
                }
                else
                {
                    if (((mfd->xl)-ax)>1 && ((mfd->xl)-ax)<256 && (mfd->xl)<256 && (mfd->yl)<256)
                    {
                        if (ay >= mfd->yl)
                        {
                            cy=((ay-(mfd->yl))<<8)/((mfd->xl)-ax);
                            for (cx=((mfd->xl)-1);cx>ax;cx--)
                            {
                                mfd->ovalue[mfd->channel_mode][cx]=((mfd->ovalue[mfd->channel_mode][mfd->xl])+((((mfd->xl)-cx)*cy)>>8));
                                switch (mfd->channel_mode) { //for faster RGB modes
                                    case 0:
                                            mfd->rvalue[0][cx]=(mfd->ovalue[0][cx]<<16);
                                            mfd->rvalue[2][cx]=(mfd->ovalue[0][cx]-cx)<<16;
                                            mfd->gvalue[0][cx]=(mfd->ovalue[0][cx]<<8);
                                            mfd->gvalue[2][cx]=(mfd->ovalue[0][cx]-cx)<<8;
                                            mfd->bvalue[cx]=mfd->ovalue[0][cx]-cx;
                                    break;
                                    case 1:
                                        mfd->rvalue[1][cx]=(mfd->ovalue[1][cx]<<16);
                                    break;
                                    case 2:
                                        mfd->gvalue[1][cx]=(mfd->ovalue[2][cx]<<8);
                                    break;
                                }
                            }
                        }
                        else
                        {
                            cy=(((mfd->yl)-ay)<<8)/((mfd->xl)-ax);
                            for (cx=((mfd->xl)-1);cx>ax;cx--)
                            {
                                mfd->ovalue[mfd->channel_mode][cx]=((mfd->ovalue[mfd->channel_mode][mfd->xl])-((((mfd->xl)-cx)*cy)>>8));
                                switch (mfd->channel_mode) { //for faster RGB modes
                                    case 0:
                                            mfd->rvalue[0][cx]=(mfd->ovalue[0][cx]<<16);
                                            mfd->rvalue[2][cx]=(mfd->ovalue[0][cx]-cx)<<16;
                                            mfd->gvalue[0][cx]=(mfd->ovalue[0][cx]<<8);
                                            mfd->gvalue[2][cx]=(mfd->ovalue[0][cx]-cx)<<8;
                                            mfd->bvalue[cx]=mfd->ovalue[0][cx]-cx;
                                    break;
                                    case 1:
                                        mfd->rvalue[1][cx]=(mfd->ovalue[1][cx]<<16);
                                    break;
                                    case 2:
                                        mfd->gvalue[1][cx]=(mfd->ovalue[2][cx]<<8);
                                    break;
                                }
                            }
                        }
                    }
                }
                mfd->xl = ax;
                mfd->yl = ay;
                mfd->value=ax;
            }
            else {
                if (mfd->psel==true){
                    if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_GAMMA){
                        if (mfd->drwpoint[mfd->channel_mode][0][1]<mfd->drwpoint[mfd->channel_mode][2][1]){
                            max=mfd->drwpoint[mfd->channel_mode][2][1];
                            min=mfd->drwpoint[mfd->channel_mode][0][1];}
                        else {
                            max=mfd->drwpoint[mfd->channel_mode][0][1];
                            min=mfd->drwpoint[mfd->channel_mode][2][1];}
                        switch (mfd->cp){
                            case 0:
                                if (mfd->drwpoint[mfd->channel_mode][0][1]<mfd->drwpoint[mfd->channel_mode][2][1]){
                                    if (ay>mfd->drwpoint[mfd->channel_mode][1][1]){ay=mfd->drwpoint[mfd->channel_mode][1][1]-1;}}
                                else {if (ay<mfd->drwpoint[mfd->channel_mode][1][1]){ay=mfd->drwpoint[mfd->channel_mode][1][1]+1;}}
                            break;
                            case 1:
                                if (ay>=max){ay=max-1;}
                                if (ay<=min){ay=min+1;}
                            break;
                            case 2:
                                if (mfd->drwpoint[mfd->channel_mode][0][1]<mfd->drwpoint[mfd->channel_mode][2][1]){
                                    if (ay<mfd->drwpoint[mfd->channel_mode][1][1]){ay=mfd->drwpoint[mfd->channel_mode][1][1]+1;}}
                                else {if (ay>mfd->drwpoint[mfd->channel_mode][1][1]){ay=mfd->drwpoint[mfd->channel_mode][1][1]-1;}}
                            break;
                        }
                        mfd->drwpoint[mfd->channel_mode][mfd->cp][1]=ay;
                    }
                    else {mfd->drwpoint[mfd->channel_mode][mfd->cp][1]=ay;}
                    if (mfd->cp==0){
                        if (ax>=mfd->drwpoint[mfd->channel_mode][(mfd->cp)+1][0]){mfd->drwpoint[mfd->channel_mode][mfd->cp][0]=(mfd->drwpoint[mfd->channel_mode][(mfd->cp)+1][0])-1;}
                        else {mfd->drwpoint[mfd->channel_mode][mfd->cp][0]=ax;}}
                    else if (mfd->cp==(mfd->poic[mfd->channel_mode]-1)){
                        if (ax<=mfd->drwpoint[mfd->channel_mode][(mfd->cp)-1][0]){mfd->drwpoint[mfd->channel_mode][mfd->cp][0]=(mfd->drwpoint[mfd->channel_mode][(mfd->cp)-1][0])+1;}
                        else {mfd->drwpoint[mfd->channel_mode][mfd->cp][0]=ax;}}
                    else{
                        if (ax>=mfd->drwpoint[mfd->channel_mode][(mfd->cp)+1][0]){mfd->drwpoint[mfd->channel_mode][mfd->cp][0]=(mfd->drwpoint[mfd->channel_mode][(mfd->cp)+1][0])-1;}
                        else if (ax<=mfd->drwpoint[mfd->channel_mode][(mfd->cp)-1][0]){mfd->drwpoint[mfd->channel_mode][mfd->cp][0]=(mfd->drwpoint[mfd->channel_mode][(mfd->cp)-1][0])+1;}
                        else {mfd->drwpoint[mfd->channel_mode][mfd->cp][0]=ax;}
                    }
                    }
                CalcCurve(*mfd);
                if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_GAMMA){
                    hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                    SetWindowText(hWnd, mfd->gamma);}
                mfd->value=mfd->drwpoint[mfd->channel_mode][mfd->cp][0];
            }
            SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
            SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, ay, FALSE);
            GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
            mfd->ifp->RedoFrame();
            break;

        case WM_USER + 101: //left mouse button pressed
            if (wParam >= mfd->boxx) {ax = 255;}
            else if (wParam < mfd->boxx) {ax = int(wParam*mfd->scalex+0.5);}
            if (lParam >= mfd->boxy) {ay = 0;}
            else if (lParam < mfd->boxy) {ay = 255-int(lParam*mfd->scaley+0.5);}
            if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN){
                mfd->xl = 300;
                mfd->yl = 300;
                mfd->value=ax;
                SetDlgItemInt(hdlg, IDC_VALUE, ax, FALSE);
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, ay, FALSE);}
            else {
                mfd->psel=false;
                stp=false;
                for (i=0; i<(mfd->poic[mfd->channel_mode]);i++){  // select point
                    if (abs(mfd->drwpoint[mfd->channel_mode][i][0]-ax)<11 && abs(mfd->drwpoint[mfd->channel_mode][i][1]-ay)<31 && stp==false){
                        if (i<mfd->poic[mfd->channel_mode]-1){
                            if (abs(mfd->drwpoint[mfd->channel_mode][i][0]-ax)+abs(mfd->drwpoint[mfd->channel_mode][i][1]-ay)<abs(mfd->drwpoint[mfd->channel_mode][i+1][0]-ax)+abs(mfd->drwpoint[mfd->channel_mode][i+1][1]-ay)){
                            mfd->cp=i;
                            mfd->psel=true;
                            stp=true;
                            SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                            GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);}
                        }
                        else {
                            mfd->cp=i;
                            mfd->psel=true;
                            stp=true;
                            SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                            GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);}
                    }
                }
                if (mfd->drwmode[mfd->channel_mode]!=DRAWMODE_GAMMA){ // add point
                    stp=false;
                    ptp=false;
                    if (mfd->psel==false && mfd->poic[mfd->channel_mode]<16)
                        for (i=1; i<(mfd->poic[mfd->channel_mode]);i++){
                            if (mfd->drwpoint[mfd->channel_mode][i][0]>ax && ptp==false && mfd->drwpoint[mfd->channel_mode][0][0]<ax){
                                ptp=true;
                                if (ax>mfd->drwpoint[mfd->channel_mode][i-1][0]+11 && ax<mfd->drwpoint[mfd->channel_mode][i][0]-11){
                                stp=true;
                                for (j=mfd->poic[mfd->channel_mode];j>i;j--){
                                    mfd->drwpoint[mfd->channel_mode][j][0]=mfd->drwpoint[mfd->channel_mode][j-1][0];
                                    mfd->drwpoint[mfd->channel_mode][j][1]=mfd->drwpoint[mfd->channel_mode][j-1][1];}
                                mfd->drwpoint[mfd->channel_mode][i][0]=ax;
                                mfd->drwpoint[mfd->channel_mode][i][1]=ay;
                                mfd->cp=i;}}
                        }
                        if (stp==true){
                            mfd->poic[mfd->channel_mode]=mfd->poic[mfd->channel_mode]++;
                            mfd->psel=true;
                            SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                            CalcCurve(*mfd);
                            GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                            mfd->ifp->RedoFrame();
                            }
                    }
                SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                }
            break;

        case WM_USER + 102: //left mouse button releases
            mfd->psel=false;
            break;

        case WM_USER + 103: //right mouse button pressed
            if (wParam >= mfd->boxx) {ax = 255;}
            else if (wParam < mfd->boxx) {ax = int(wParam*mfd->scalex+0.5);}
            if (lParam >= mfd->boxy) {ay = 0;}
            else if (lParam < mfd->boxy) {ay = 255-int(lParam*mfd->scaley+0.5);}
            if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN){
                mfd->value=ax;
                SetDlgItemInt(hdlg, IDC_VALUE, ax, FALSE);
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][ax], FALSE);}
            else if ((mfd->drwmode[mfd->channel_mode]==DRAWMODE_LINEAR || mfd->drwmode[mfd->channel_mode]==DRAWMODE_SPLINE) && mfd->poic[mfd->channel_mode]>2){ // delete point
                stp=false;
                ptp=false;
                for (i=1; i<(mfd->poic[mfd->channel_mode])-1;i++){
                    if (abs(mfd->drwpoint[mfd->channel_mode][i][0]-ax)<11 && abs(mfd->drwpoint[mfd->channel_mode][i][1]-ay)<11 && stp==false){
                        if (abs(mfd->drwpoint[mfd->channel_mode][i][0]-ax)+abs(mfd->drwpoint[mfd->channel_mode][i][1]-ay)<abs(mfd->drwpoint[mfd->channel_mode][i+1][0]-ax)+abs(mfd->drwpoint[mfd->channel_mode][i+1][1]-ay)){
                            mfd->cp=i;
                            for (j=i; j<=(mfd->poic[mfd->channel_mode]-1);j++){
                                mfd->drwpoint[mfd->channel_mode][j][0]=mfd->drwpoint[mfd->channel_mode][j+1][0];
                                mfd->drwpoint[mfd->channel_mode][j][1]=mfd->drwpoint[mfd->channel_mode][j+1][1];}
                            mfd->poic[mfd->channel_mode]=mfd->poic[mfd->channel_mode]--;
                            mfd->cp--;
                            stp=true;
                            CalcCurve(*mfd);
                            GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                            SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                            SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                            SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                            mfd->ifp->RedoFrame();
                        }
                    }
                }
            }
            break;

        case WM_COMMAND:
            switch(LOWORD(wParam)) {
            case IDPREVIEW: // open preview
                mfd->ifp->Toggle(hdlg);
                break;
            case IDCANCEL:
                EndDialog(hdlg, 1);
                return TRUE;
            case IDOK:
                EndDialog(hdlg, 0);
                return TRUE;
            case IDHELP: // open helpfile
                {
                char prog[256];
                char path[256];
                LPTSTR ptr;
                GetModuleFileName(NULL, prog, 255);
                GetFullPathName(prog, 255, path, &ptr);
                *ptr = 0;
                strcat(path, "plugins\\gradation.html");
                ShellExecute(hdlg, "open", path, NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
                }
            case IDIMPORT: // import curves
                {
                OPENFILENAME ofn;
                mfd->filename[0] = NULL;
                ofn.lStructSize = sizeof(OPENFILENAME);
                ofn.hwndOwner = hdlg;
                ofn.hInstance = NULL;
                ofn.lpTemplateName = NULL;
                ofn.lpstrFilter = "Map Settings (*.amp)\0*.amp\0Adjustment Curves (*.acv)\0*.acv\0Comma Separated Values (*.csv)\0*.csv\0Tone Curve File (*.crv)\0*.crv\0Tone Map File (*.map)\0*.map\0SmartCurve HSV (*.amp)\0*.amp\0All Files\0*.*\0\0";
                ofn.lpstrCustomFilter = NULL;
                ofn.nMaxCustFilter = 0;
                ofn.nFilterIndex = 1;
                ofn.lpstrFile = mfd->filename;
                ofn.nMaxFile = 1024;
                ofn.lpstrFileTitle = NULL;
                ofn.nMaxFileTitle = 0;
                ofn.lpstrInitialDir = NULL;
                ofn.lpstrTitle = "Choose Import File";
                ofn.Flags = OFN_EXPLORER | OFN_CREATEPROMPT | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                ofn.nFileOffset = 0;
                ofn.nFileExtension = 0;
                ofn.lpstrDefExt = NULL;
                ofn.lCustData = 0;
                ofn.lpfnHook = NULL;
                GetOpenFileName(&ofn);
                mfd->filter = ofn.nFilterIndex;
                if (mfd->filename[0] != 0)
                {
                    if (!ImportCurve (*mfd))
                    {
                        MessageBox (NULL, TEXT ("Error"), TEXT ("Error opening file"),0);
                    }
                    if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN) {
                        hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                        ShowWindow(hWnd, SW_HIDE);
                        hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                        ShowWindow(hWnd, SW_HIDE);
                        hWnd = GetDlgItem(hdlg, IDC_POINTMINUS);
                        ShowWindow(hWnd, SW_HIDE);
                        hWnd = GetDlgItem(hdlg, IDC_POINTPLUS);
                        ShowWindow(hWnd, SW_HIDE);
                        hWnd = GetDlgItem(hdlg, IDC_POINT);
                        ShowWindow(hWnd, SW_HIDE);
                        hWnd = GetDlgItem(hdlg, IDC_POINTNO);
                        ShowWindow(hWnd, SW_HIDE);
                        mfd->value=0;
                        SetDlgItemInt(hdlg, IDC_VALUE, (mfd->value), FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);}
                    else {
                        mfd->cp=0;
                        SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                        SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                        if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_GAMMA) {
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            ShowWindow(hWnd, SW_SHOW);
                            SetWindowText(hWnd, mfd->gamma);
                            hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                            ShowWindow(hWnd, SW_SHOW);}
                        else {
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            ShowWindow(hWnd, SW_HIDE);
                            hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                            ShowWindow(hWnd, SW_HIDE);}
                        hWnd = GetDlgItem(hdlg, IDC_POINTMINUS);
                        ShowWindow(hWnd, SW_SHOW);
                        hWnd = GetDlgItem(hdlg, IDC_POINTPLUS);
                        ShowWindow(hWnd, SW_SHOW);
                        hWnd = GetDlgItem(hdlg, IDC_POINT);
                        ShowWindow(hWnd, SW_SHOW);
                        hWnd = GetDlgItem(hdlg, IDC_POINTNO);
                        ShowWindow(hWnd, SW_SHOW);
                    }
                    switch  (mfd->drwmode[mfd->channel_mode]) {
                        case DRAWMODE_PEN:
                            CheckDlgButton(hdlg, IDC_RADIOPM, BST_CHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                            break;
                        case DRAWMODE_LINEAR:
                            CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOLM, BST_CHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                            break;
                        case DRAWMODE_SPLINE:
                            CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOSM, BST_CHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                            break;
                        case DRAWMODE_GAMMA:
                            CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOGM, BST_CHECKED);
                            break;
                    }
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                    mfd->ifp->RedoFrame();
                }
                break;
                }
                return TRUE;
            case IDEXPORT: // export curves
                {
                OPENFILENAME ofn;
                mfd->filename[0] = NULL;
                ofn.lStructSize = sizeof(OPENFILENAME);
                ofn.hwndOwner = hdlg;
                ofn.hInstance = NULL;
                ofn.lpTemplateName = NULL;
                ofn.lpstrFilter = "Map Settings (*.amp)\0*.amp\0Adjustment Curves (*.acv)\0*.acv\0Comma Separated Values (*.csv)\0*.csv\0All Files\0*.*\0\0";
                ofn.lpstrCustomFilter = NULL;
                ofn.nMaxCustFilter = 0;
                ofn.nFilterIndex = 1;
                ofn.lpstrFile = mfd->filename;
                ofn.nMaxFile = 1024;
                ofn.lpstrFileTitle = NULL;
                ofn.nMaxFileTitle = 0;
                ofn.lpstrInitialDir = NULL;
                ofn.lpstrTitle = "Choose Import File";
                ofn.Flags = OFN_EXPLORER | OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT;
                ofn.nFileOffset = 0;
                ofn.nFileExtension = 0;
                ofn.lpstrDefExt = ("amp");
                ofn.lCustData = 0;
                ofn.lpfnHook = NULL;
                GetSaveFileName(&ofn);
                mfd->filter = ofn.nFilterIndex;
                if (mfd->filename[0] != 0)
                {
                    ExportCurve (*mfd);
                }
                break;
                }
                return TRUE;
            case IDC_RGB:
                switch (mfd->space_mode){
                    case SPACE_RGB:
                    mfd->process = PROCESS_RGB;
                    break;
                    case SPACE_YUV:
                    mfd->process = PROCESS_YUV;
                    break;
                    case SPACE_CMYK:
                    mfd->process = PROCESS_CMYK;
                    break;
                    case SPACE_HSV:
                    mfd->process = PROCESS_HSV;
                    break;
                    case SPACE_LAB:
                    mfd->process = PROCESS_LAB;
                    break;
                }
                mfd->ifp->RedoFrame();
            break;
            case IDC_FULL:
                switch (mfd->space_mode){
                    case SPACE_RGB:
                    mfd->process = PROCESS_FULL;
                    break;
                    case SPACE_YUV:
                    mfd->process = PROCESS_OFF;
                    break;
                    case SPACE_CMYK:
                    mfd->process = PROCESS_OFF;
                    break;
                    case SPACE_HSV:
                    mfd->process = PROCESS_OFF;
                    break;
                    case SPACE_LAB:
                    mfd->process = PROCESS_OFF;
                    break;
                }
                mfd->ifp->RedoFrame();
            break;
            case IDC_RGBW:
                mfd->process = PROCESS_RGBW;
                mfd->ifp->RedoFrame();
            break;
            case IDC_FULLW:
                mfd->process = PROCESS_FULLW;
                mfd->ifp->RedoFrame();
            break;
            case IDC_OFF:
                mfd->process = PROCESS_OFF;
                mfd->ifp->RedoFrame();
            break;
            case IDC_INPUTPLUS:
                if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN){
                    if (mfd->value < 255) {
                        mfd->value++;
                        SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);}
                }
                else {
                    if (mfd->cp==mfd->poic[mfd->channel_mode]-1) {i=255;}
                    else {i=mfd->drwpoint[mfd->channel_mode][(mfd->cp+1)][0]-1;}
                    if (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]<i){
                        mfd->drwpoint[mfd->channel_mode][mfd->cp][0]++;
                        SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                        CalcCurve(*mfd);
                        if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_GAMMA){
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            SetWindowText(hWnd, mfd->gamma);}
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                        mfd->ifp->RedoFrame();
                    }
                }
                break;
            case IDC_INPUTMINUS:
                if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN){
                    if (mfd->value > 0) {
                        mfd->value--;
                        SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);}
                }
                else {
                    if (mfd->cp==0) {i=0;}
                    else {i=mfd->drwpoint[mfd->channel_mode][(mfd->cp-1)][0]+1;}
                    if (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]>i){
                        mfd->drwpoint[mfd->channel_mode][mfd->cp][0]--;
                        SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                        CalcCurve(*mfd);
                        if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_GAMMA){
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            SetWindowText(hWnd, mfd->gamma);}
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                        mfd->ifp->RedoFrame();
                    }
                }

                break;
            case IDC_OUTPUTPLUS:
                if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN){
                    if (mfd->ovalue[mfd->channel_mode][mfd->value] < 255) {
                        mfd->ovalue[mfd->channel_mode][mfd->value]++;
                        switch (mfd->channel_mode) { //for faster RGB modes
                            case 0:
                                    mfd->rvalue[0][mfd->value]=(mfd->ovalue[0][mfd->value]<<16);
                                    mfd->rvalue[2][mfd->value]=(mfd->ovalue[0][mfd->value]-mfd->value)<<16;
                                    mfd->gvalue[0][mfd->value]=(mfd->ovalue[0][mfd->value]<<8);
                                    mfd->gvalue[2][mfd->value]=(mfd->ovalue[0][mfd->value]-mfd->value)<<8;
                                    mfd->bvalue[mfd->value]=mfd->ovalue[0][mfd->value]-mfd->value;
                            break;
                            case 1:
                                mfd->rvalue[1][mfd->value]=(mfd->ovalue[1][mfd->value]<<16);
                            break;
                            case 2:
                                mfd->gvalue[1][mfd->value]=(mfd->ovalue[2][mfd->value]<<8);
                            break;}
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);}
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                    mfd->ifp->RedoFrame();
                }
                else {
                    if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_GAMMA) {
                        if (mfd->drwpoint[mfd->channel_mode][0][1]<mfd->drwpoint[mfd->channel_mode][2][1]){
                            if (mfd->cp<2) {i=mfd->drwpoint[mfd->channel_mode][mfd->cp+1][1]-1;}
                            else {i=255;}
                        }
                        else {
                            if (mfd->cp>0) {i=mfd->drwpoint[mfd->channel_mode][mfd->cp-1][1]-1;}
                            else {i=255;}
                        }
                    }
                    else {i=255;}
                    if (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]<i){
                        mfd->drwpoint[mfd->channel_mode][mfd->cp][1]++;
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                        CalcCurve(*mfd);
                        if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_GAMMA){
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            SetWindowText(hWnd, mfd->gamma);}
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                        mfd->ifp->RedoFrame();
                    }
                }
                break;
            case IDC_OUTPUTMINUS:
                if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN){
                    if (mfd->ovalue[mfd->channel_mode][mfd->value] > 0) {
                        mfd->ovalue[mfd->channel_mode][mfd->value]--;
                        switch (mfd->channel_mode) { //for faster RGB modes
                            case 0:
                                    mfd->rvalue[0][mfd->value]=(mfd->ovalue[0][mfd->value]<<16);
                                    mfd->rvalue[2][mfd->value]=(mfd->ovalue[0][mfd->value]-mfd->value)<<16;
                                    mfd->gvalue[0][mfd->value]=(mfd->ovalue[0][mfd->value]<<8);
                                    mfd->gvalue[2][mfd->value]=(mfd->ovalue[0][mfd->value]-mfd->value)<<8;
                                    mfd->bvalue[mfd->value]=mfd->ovalue[0][mfd->value]-mfd->value;
                            break;
                            case 1:
                                mfd->rvalue[1][mfd->value]=(mfd->ovalue[1][mfd->value]<<16);
                            break;
                            case 2:
                                mfd->gvalue[1][mfd->value]=(mfd->ovalue[2][mfd->value]<<8);
                            break;}
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);}
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                    mfd->ifp->RedoFrame();
                }
                else {
                    if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_GAMMA) {
                        if (mfd->drwpoint[mfd->channel_mode][0][1]<mfd->drwpoint[mfd->channel_mode][2][1]){
                            if (mfd->cp>0) {i=mfd->drwpoint[mfd->channel_mode][mfd->cp-1][1]+1;}
                            else {i=0;}
                        }
                        else {
                            if (mfd->cp<2) {i=mfd->drwpoint[mfd->channel_mode][mfd->cp+1][1]+1;}
                            else {i=0;}
                        }
                    }
                    else {i=0;}
                    if (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]>i){
                        mfd->drwpoint[mfd->channel_mode][mfd->cp][1]--;
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                        CalcCurve(*mfd);
                        if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_GAMMA){
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            SetWindowText(hWnd, mfd->gamma);}
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                        mfd->ifp->RedoFrame();
                    }
                }
                break;
            case IDC_POINTPLUS:
                if (mfd->drwmode[mfd->channel_mode]!=DRAWMODE_PEN){
                    if (mfd->cp<mfd->poic[mfd->channel_mode]-1){
                        mfd->cp++;
                        SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                        SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);}
                }
                break;
            case IDC_POINTMINUS:
                if (mfd->drwmode[mfd->channel_mode]!=DRAWMODE_PEN){
                    if (mfd->cp>0){
                        mfd->cp--;
                        SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                        SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);}
                }
                break;
            case IDC_RESET: // reset the curve
                switch (mfd->drwmode[mfd->channel_mode]){
                    case DRAWMODE_PEN:
                        for (i=0; i<256; i++) {
                            mfd->ovalue[mfd->channel_mode][i] = i;
                            switch (mfd->channel_mode) { //for faster RGB modes
                            case 0:
                                    mfd->rvalue[0][i]=(mfd->ovalue[0][i]<<16);
                                    mfd->rvalue[2][i]=(mfd->ovalue[0][i]-i)<<16;
                                    mfd->gvalue[0][i]=(mfd->ovalue[0][i]<<8);
                                    mfd->gvalue[2][i]=(mfd->ovalue[0][i]-i)<<8;
                                    mfd->bvalue[i]=mfd->ovalue[0][i]-i;
                            break;
                            case 1:
                                mfd->rvalue[1][i]=(mfd->ovalue[1][i]<<16);
                            break;
                            case 2:
                                mfd->gvalue[1][i]=(mfd->ovalue[2][i]<<8);
                            break;}
                        }
                        mfd->value=0;
                        SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                        break;
                    case DRAWMODE_LINEAR:
                        mfd->poic[mfd->channel_mode]=2;
                        mfd->drwpoint[mfd->channel_mode][0][0]=0;
                        mfd->drwpoint[mfd->channel_mode][0][1]=0;
                        mfd->drwpoint[mfd->channel_mode][1][0]=255;
                        mfd->drwpoint[mfd->channel_mode][1][1]=255;
                        CalcCurve(*mfd);
                        mfd->cp=0;
                        SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                        SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                        break;
                    case DRAWMODE_SPLINE:
                        mfd->poic[mfd->channel_mode]=2;
                        mfd->drwpoint[mfd->channel_mode][0][0]=0;
                        mfd->drwpoint[mfd->channel_mode][0][1]=0;
                        mfd->drwpoint[mfd->channel_mode][1][0]=255;
                        mfd->drwpoint[mfd->channel_mode][1][1]=255;
                        CalcCurve(*mfd);
                        mfd->cp=0;
                        SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                        SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                        break;
                    case DRAWMODE_GAMMA:
                        mfd->poic[mfd->channel_mode]=3;
                        mfd->drwpoint[mfd->channel_mode][0][0]=0;
                        mfd->drwpoint[mfd->channel_mode][0][1]=0;
                        mfd->drwpoint[mfd->channel_mode][1][0]=128;
                        mfd->drwpoint[mfd->channel_mode][1][1]=128;
                        mfd->drwpoint[mfd->channel_mode][2][0]=255;
                        mfd->drwpoint[mfd->channel_mode][2][1]=255;
                        CalcCurve(*mfd);
                        mfd->cp=0;
                        SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                        SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                        hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                        SetWindowText(hWnd, mfd->gamma);
                        break;
                    }
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                mfd->ifp->RedoFrame();
                break;
            case IDC_SMOOTH:  // smooth the curve
                if (mfd->drwmode[mfd->channel_mode] == DRAWMODE_PEN){
                    if (mfd->ovalue[mfd->channel_mode][0]<=mfd->ovalue[mfd->channel_mode][255]) {
                        a = mfd->ovalue[mfd->channel_mode][0];
                        b = mfd->ovalue[mfd->channel_mode][255]-a;}
                    else if (mfd->ovalue[mfd->channel_mode][0]>mfd->ovalue[mfd->channel_mode][255]) {
                        a = mfd->ovalue[mfd->channel_mode][0];
                        b = -a+mfd->ovalue[mfd->channel_mode][255];}
                    for (i=0; i<256; i++) {delta[i] = mfd->ovalue[mfd->channel_mode][i]-(((i*b)/255)+a);}
                    for (i=0; i<255; i++) {
                        if (i < 2)
                        {
                            if (i==0) {inv[i]=(delta[i]*25)/25;}
                            else {inv[i]=(delta[i]*25+delta[i-1]*50)/75;}
                        }
                        else {inv[i]=(delta[i]*25+delta[i-1]*50+delta[i-2]*25)/100;}
                    }
                    for (i=0; i<255; i++) {delta[i]=inv[i];}
                    for (i=255; i>0; i--) {
                        if (i > 253)
                        {
                            if (i==255) {inv[i]=(delta[i]*25)/25;}
                            else {inv[i]=(delta[i]*25+delta[i+1]*50)/75;}
                        }
                        else {inv[i]=(delta[i]*25+delta[i+1]*50+delta[i+2]*25)/100;}
                    }
                    for (i=255; i>0; i--) {delta[i]=inv[i];}
                    for (i=0; i<256; i++) {
                        mfd->ovalue[mfd->channel_mode][i] = delta[i]+((i*b)/255)+a;
                        switch (mfd->channel_mode) { //for faster RGB modes
                            case 0:
                                    mfd->rvalue[0][i]=(mfd->ovalue[0][i]<<16);
                                    mfd->rvalue[2][i]=(mfd->ovalue[0][i]-i)<<16;
                                    mfd->gvalue[0][i]=(mfd->ovalue[0][i]<<8);
                                    mfd->gvalue[2][i]=(mfd->ovalue[0][i]-i)<<8;
                                    mfd->bvalue[i]=mfd->ovalue[0][i]-i;
                            break;
                            case 1:
                                mfd->rvalue[1][i]=(mfd->ovalue[1][i]<<16);
                            break;
                            case 2:
                                mfd->gvalue[1][i]=(mfd->ovalue[2][i]<<8);
                            break;}
                    }
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                    mfd->ifp->RedoFrame();
                }
                break;
            case IDC_INVERTX:
                if (mfd->drwmode[mfd->channel_mode] == DRAWMODE_PEN){
                    for (i=0; i<256; i++) {inv[255-i] = mfd->ovalue[mfd->channel_mode][i];}
                    for (i=0; i<256; i++) {mfd->ovalue[mfd->channel_mode][i] = inv[i];
                    switch (mfd->channel_mode) { //for faster RGB modes
                            case 0:
                                    mfd->rvalue[0][i]=(mfd->ovalue[0][i]<<16);
                                    mfd->rvalue[2][i]=(mfd->ovalue[0][i]-i)<<16;
                                    mfd->gvalue[0][i]=(mfd->ovalue[0][i]<<8);
                                    mfd->gvalue[2][i]=(mfd->ovalue[0][i]-i)<<8;
                                    mfd->bvalue[i]=mfd->ovalue[0][i]-i;
                            break;
                            case 1:
                                mfd->rvalue[1][i]=(mfd->ovalue[1][i]<<16);
                            break;
                            case 2:
                                mfd->gvalue[1][i]=(mfd->ovalue[2][i]<<8);
                            break;}
                    }
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                }
                else {
                    for (i=0;i<mfd->poic[mfd->channel_mode];i++){
                        invp[i][0]=255-mfd->drwpoint[mfd->channel_mode][mfd->poic[mfd->channel_mode]-1-i][0];
                        invp[i][1]=mfd->drwpoint[mfd->channel_mode][mfd->poic[mfd->channel_mode]-1-i][1];}
                    for (i=0;i<mfd->poic[mfd->channel_mode];i++){
                        mfd->drwpoint[mfd->channel_mode][i][0]=invp[i][0];
                        mfd->drwpoint[mfd->channel_mode][i][1]=invp[i][1];}
                    CalcCurve(*mfd);
                    SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                    SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                    if (mfd->drwmode[mfd->channel_mode] == DRAWMODE_GAMMA){
                        hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                        SetWindowText(hWnd, mfd->gamma);}
                }
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                mfd->ifp->RedoFrame();
                break;
            case IDC_RADIOPM:
                if (mfd->drwmode[mfd->channel_mode]!=DRAWMODE_PEN){
                    mfd->drwmode[mfd->channel_mode]=DRAWMODE_PEN;
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)],mfd->cp);
                    hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                    ShowWindow(hWnd, SW_HIDE);
                    hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                    ShowWindow(hWnd, SW_HIDE);
                    hWnd = GetDlgItem(hdlg, IDC_POINTMINUS);
                    ShowWindow(hWnd, SW_HIDE);
                    hWnd = GetDlgItem(hdlg, IDC_POINTPLUS);
                    ShowWindow(hWnd, SW_HIDE);
                    hWnd = GetDlgItem(hdlg, IDC_POINT);
                    ShowWindow(hWnd, SW_HIDE);
                    hWnd = GetDlgItem(hdlg, IDC_POINTNO);
                    ShowWindow(hWnd, SW_HIDE);
                    mfd->value=0;
                    SetDlgItemInt(hdlg, IDC_VALUE, (mfd->value), FALSE);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                    mfd->ifp->RedoFrame();
                }
                break;
            case IDC_RADIOLM:
                if (mfd->drwmode[mfd->channel_mode]!=DRAWMODE_LINEAR){
                    if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN) {
                        mfd->poic[mfd->channel_mode]=2;
                        mfd->drwpoint[mfd->channel_mode][0][0]=0;
                        mfd->drwpoint[mfd->channel_mode][0][1]=0;
                        mfd->drwpoint[mfd->channel_mode][1][0]=255;
                        mfd->drwpoint[mfd->channel_mode][1][1]=255;
                        mfd->cp=0;}
                    mfd->drwmode[mfd->channel_mode]=DRAWMODE_LINEAR;
                    CalcCurve(*mfd);
                    SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                    SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                    hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                    ShowWindow(hWnd, SW_HIDE);
                    hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                    ShowWindow(hWnd, SW_HIDE);
                    hWnd = GetDlgItem(hdlg, IDC_POINTMINUS);
                    ShowWindow(hWnd, SW_SHOW);
                    hWnd = GetDlgItem(hdlg, IDC_POINTPLUS);
                    ShowWindow(hWnd, SW_SHOW);
                    hWnd = GetDlgItem(hdlg, IDC_POINT);
                    ShowWindow(hWnd, SW_SHOW);
                    hWnd = GetDlgItem(hdlg, IDC_POINTNO);
                    ShowWindow(hWnd, SW_SHOW);
                    mfd->ifp->RedoFrame();
                }
                break;
            case IDC_RADIOSM:
                if (mfd->drwmode[mfd->channel_mode]!=DRAWMODE_SPLINE){
                    if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN) {
                        mfd->poic[mfd->channel_mode]=2;
                        mfd->drwpoint[mfd->channel_mode][0][0]=0;
                        mfd->drwpoint[mfd->channel_mode][0][1]=0;
                        mfd->drwpoint[mfd->channel_mode][1][0]=255;
                        mfd->drwpoint[mfd->channel_mode][1][1]=255;
                        mfd->cp=0;}
                    mfd->drwmode[mfd->channel_mode]=DRAWMODE_SPLINE;
                    CalcCurve(*mfd);
                    SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                    SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                    hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                    ShowWindow(hWnd, SW_HIDE);
                    hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                    ShowWindow(hWnd, SW_HIDE);
                    hWnd = GetDlgItem(hdlg, IDC_POINTMINUS);
                    ShowWindow(hWnd, SW_SHOW);
                    hWnd = GetDlgItem(hdlg, IDC_POINTPLUS);
                    ShowWindow(hWnd, SW_SHOW);
                    hWnd = GetDlgItem(hdlg, IDC_POINT);
                    ShowWindow(hWnd, SW_SHOW);
                    hWnd = GetDlgItem(hdlg, IDC_POINTNO);
                    ShowWindow(hWnd, SW_SHOW);
                    mfd->ifp->RedoFrame();
                }
                break;
            case IDC_RADIOGM:
                if (mfd->drwmode[mfd->channel_mode]!=DRAWMODE_GAMMA){
                    if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN || mfd->poic[mfd->channel_mode]!=3) {
                        mfd->poic[mfd->channel_mode]=3;
                        mfd->drwpoint[mfd->channel_mode][0][0]=0;
                        mfd->drwpoint[mfd->channel_mode][0][1]=0;
                        mfd->drwpoint[mfd->channel_mode][1][0]=128;
                        mfd->drwpoint[mfd->channel_mode][1][1]=128;
                        mfd->drwpoint[mfd->channel_mode][2][0]=255;
                        mfd->drwpoint[mfd->channel_mode][2][1]=255;
                        mfd->cp=0;}
                    mfd->drwmode[mfd->channel_mode]=DRAWMODE_GAMMA;
                    CalcCurve(*mfd);
                    SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                    SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                    hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                    ShowWindow(hWnd, SW_SHOW);
                    SetWindowText(hWnd, mfd->gamma);
                    hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                    ShowWindow(hWnd, SW_SHOW);
                    hWnd = GetDlgItem(hdlg, IDC_POINTMINUS);
                    ShowWindow(hWnd, SW_SHOW);
                    hWnd = GetDlgItem(hdlg, IDC_POINTPLUS);
                    ShowWindow(hWnd, SW_SHOW);
                    hWnd = GetDlgItem(hdlg, IDC_POINT);
                    ShowWindow(hWnd, SW_SHOW);
                    hWnd = GetDlgItem(hdlg, IDC_POINTNO);
                    ShowWindow(hWnd, SW_SHOW);
                    mfd->ifp->RedoFrame();
                }
                break;
            case IDC_SPACE:
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    spacemode = SendDlgItemMessage(hdlg, IDC_SPACE, CB_GETCURSEL, 0, 0);
                    if (mfd->space_mode != spacemode) {
                        mfd->space_mode = Space(spacemode);
                        hWnd = GetDlgItem(hdlg, IDC_CHANNEL);
                        SendMessage(hWnd, CB_RESETCONTENT, 0, 0);
                        mfd->laboff = 0;
                        switch (mfd->space_mode){
                        case SPACE_RGB:
                            for(i=0; i<4; i++)
                            {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)RGBchannel_names[i]);}
                            mfd->channel_mode = Channel(0);
                            mfd->offset = 0;
                            hWnd = GetDlgItem(hdlg, IDC_RGB);
                            SetWindowText (hWnd,"RGB only");
                            hWnd = GetDlgItem(hdlg, IDC_FULL);
                            SetWindowText (hWnd,"RGB + R/G/B");
                            SendMessage (hWnd,BM_SETCHECK,0,0L);
                            hWnd = GetDlgItem(hdlg, IDC_RGBW);
                            SetWindowText (hWnd,"RGB weighted");
                            ShowWindow (hWnd, SW_SHOWNORMAL);
                            SendMessage (hWnd,BM_SETCHECK,0,0L);
                            hWnd = GetDlgItem(hdlg, IDC_FULLW);
                            SetWindowText (hWnd,"RGB weighted + R/G/B");
                            ShowWindow (hWnd, SW_SHOWNORMAL);
                            SendMessage (hWnd,BM_SETCHECK,0,0L);
                            hWnd = GetDlgItem(hdlg, IDC_OFF);
                            SetWindowText (hWnd,"no processing");
                            ShowWindow (hWnd, SW_SHOWNORMAL);
                            SendMessage (hWnd,BM_SETCHECK,0,0L);
                            if (mfd->process != PROCESS_OFF) mfd->process = PROCESS_RGB;
                            break;
                        case SPACE_YUV:
                            for(i=0; i<3; i++)
                            {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)YUVchannel_names[i]);}
                            mfd->channel_mode = Channel(0);
                            mfd->offset = 1;
                            hWnd = GetDlgItem(hdlg, IDC_RGB);
                            SetWindowText (hWnd,"Y/U/V");
                            if (mfd->process != PROCESS_OFF) mfd->process = PROCESS_YUV;
                            break;
                        case SPACE_CMYK:
                            for(i=0; i<4; i++)
                            {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)CMYKchannel_names[i]);}
                            mfd->channel_mode = Channel(0);
                            mfd->offset = 1;
                            hWnd = GetDlgItem(hdlg, IDC_RGB);
                            SetWindowText (hWnd,"C/M/Y/K");
                            if (mfd->process != PROCESS_OFF) mfd->process = PROCESS_CMYK;
                            break;
                        case SPACE_HSV:
                            for(i=0; i<3; i++)
                            {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)HSVchannel_names[i]);}
                            mfd->channel_mode = Channel(0);
                            mfd->offset = 1;
                            hWnd = GetDlgItem(hdlg, IDC_RGB);
                            SetWindowText (hWnd,"H/S/V");
                            if (mfd->process != PROCESS_OFF) mfd->process = PROCESS_HSV;
                            break;
                        case SPACE_LAB:
                            for(i=0; i<3; i++)
                            {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)LABchannel_names[i]);}
                            mfd->channel_mode = Channel(0);
                            mfd->offset = 1;
                            hWnd = GetDlgItem(hdlg, IDC_RGB);
                            SetWindowText (hWnd,"L/a/b");
                            if (mfd->process != PROCESS_OFF) mfd->process = PROCESS_LAB;
                            if (mfd->Labprecalc==0) { // build up the LUT for the Lab process if it is not precalculated already
                                hCursor = LoadCursor(NULL, IDC_WAIT);
                                SetCursor (hCursor);
                                PreCalcLut();
                                mfd->Labprecalc = 1;
                                hCursor = LoadCursor(NULL, IDC_ARROW);
                                SetCursor (hCursor);}
                            break;
                        }
                        if (mfd->space_mode != SPACE_RGB) {
                            hWnd = GetDlgItem(hdlg, IDC_FULL);
                            SetWindowText (hWnd,"no processing");
                            SendMessage (hWnd,BM_SETCHECK,0,0L);
                            hWnd = GetDlgItem(hdlg, IDC_RGBW);
                            ShowWindow (hWnd, SW_HIDE);
                            SendMessage (hWnd,BM_SETCHECK,0,0L);
                            hWnd = GetDlgItem(hdlg, IDC_FULLW);
                            ShowWindow (hWnd, SW_HIDE);
                            SendMessage (hWnd,BM_SETCHECK,0,0L);
                            hWnd = GetDlgItem(hdlg, IDC_OFF);
                            ShowWindow (hWnd, SW_HIDE);
                            SendMessage (hWnd,BM_SETCHECK,0,0L);
                            if (mfd->process != PROCESS_OFF) CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED);
                            else CheckDlgButton(hdlg, IDC_FULL,BST_CHECKED);
                        }
                        else {
                            if (mfd->process != PROCESS_OFF) CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED);
                            else CheckDlgButton(hdlg, IDC_OFF,BST_CHECKED);
                        }
                        hWnd = GetDlgItem(hdlg, IDC_CHANNEL);
                        SendMessage(hWnd, CB_SETCURSEL, mfd->channel_mode, 0);
                        mfd->channel_mode = Channel(SendDlgItemMessage(hdlg, IDC_CHANNEL, CB_GETCURSEL, 0, 0) + mfd->offset);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                        if (mfd->drwmode[mfd->channel_mode]!=DRAWMODE_GAMMA){
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            ShowWindow(hWnd, SW_HIDE);
                            hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                            ShowWindow(hWnd, SW_HIDE);}
                        if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN){
                            mfd->value=0;
                            SetDlgItemInt(hdlg, IDC_VALUE, (mfd->value), FALSE);
                            SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                            hWnd = GetDlgItem(hdlg, IDC_POINTMINUS);
                            ShowWindow(hWnd, SW_HIDE);
                            hWnd = GetDlgItem(hdlg, IDC_POINTPLUS);
                            ShowWindow(hWnd, SW_HIDE);
                            hWnd = GetDlgItem(hdlg, IDC_POINT);
                            ShowWindow(hWnd, SW_HIDE);
                            hWnd = GetDlgItem(hdlg, IDC_POINTNO);
                            ShowWindow(hWnd, SW_HIDE);}
                        else {
                            CalcCurve(*mfd);
                            mfd->cp=0;
                            hWnd = GetDlgItem(hdlg, IDC_POINTMINUS);
                            ShowWindow(hWnd, SW_SHOW);
                            hWnd = GetDlgItem(hdlg, IDC_POINTPLUS);
                            ShowWindow(hWnd, SW_SHOW);
                            hWnd = GetDlgItem(hdlg, IDC_POINT);
                            ShowWindow(hWnd, SW_SHOW);
                            hWnd = GetDlgItem(hdlg, IDC_POINTNO);
                            ShowWindow(hWnd, SW_SHOW);
                            SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                            SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                            SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                            if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_GAMMA){
                                hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                                ShowWindow(hWnd, SW_SHOW);
                                SetWindowText(hWnd, mfd->gamma);
                                hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                                ShowWindow(hWnd, SW_SHOW);}
                        }
                        switch  (mfd->drwmode[mfd->channel_mode]) {
                            case DRAWMODE_PEN:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_CHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                                break;
                            case DRAWMODE_LINEAR:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_CHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                                break;
                            case DRAWMODE_SPLINE:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_CHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                                break;
                            case DRAWMODE_GAMMA:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_CHECKED);
                                break;
                        }
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                        GrdDrawBorder(GetDlgItem(hdlg, IDC_HBORDER), GetDlgItem(hdlg, IDC_VBORDER), mfd);
                        mfd->ifp->RedoFrame();
                    }
                }
                return TRUE;
            case IDC_CHANNEL:
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    mode=SendDlgItemMessage(hdlg, IDC_CHANNEL, CB_GETCURSEL, 0, 0) + mfd->offset;
                    if (mode != mfd->channel_mode) {
                        mfd->channel_mode = Channel(mode);
                        if (mfd->space_mode == SPACE_LAB) {
                            if (mfd->channel_mode == 2) {mfd->laboff = -9;}
                            else if (mfd->channel_mode == 3) {mfd->laboff = 8;}
                            else {mfd->laboff = 0;}
                        }
                        else {mfd->laboff = 0;}
                        if (mfd->drwmode[mfd->channel_mode]!=DRAWMODE_GAMMA){
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            ShowWindow(hWnd, SW_HIDE);
                            hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                            ShowWindow(hWnd, SW_HIDE);}
                        if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_PEN){
                            mfd->value=0;
                            SetDlgItemInt(hdlg, IDC_VALUE, (mfd->value), FALSE);
                            SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                            hWnd = GetDlgItem(hdlg, IDC_POINTMINUS);
                            ShowWindow(hWnd, SW_HIDE);
                            hWnd = GetDlgItem(hdlg, IDC_POINTPLUS);
                            ShowWindow(hWnd, SW_HIDE);
                            hWnd = GetDlgItem(hdlg, IDC_POINT);
                            ShowWindow(hWnd, SW_HIDE);
                            hWnd = GetDlgItem(hdlg, IDC_POINTNO);
                            ShowWindow(hWnd, SW_HIDE);}
                        else {
                            CalcCurve(*mfd);
                            mfd->cp=0;
                            hWnd = GetDlgItem(hdlg, IDC_POINTMINUS);
                            ShowWindow(hWnd, SW_SHOW);
                            hWnd = GetDlgItem(hdlg, IDC_POINTPLUS);
                            ShowWindow(hWnd, SW_SHOW);
                            hWnd = GetDlgItem(hdlg, IDC_POINT);
                            ShowWindow(hWnd, SW_SHOW);
                            hWnd = GetDlgItem(hdlg, IDC_POINTNO);
                            ShowWindow(hWnd, SW_SHOW);
                            SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                            SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                            SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                            if (mfd->drwmode[mfd->channel_mode]==DRAWMODE_GAMMA){
                                hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                                ShowWindow(hWnd, SW_SHOW);
                                SetWindowText(hWnd, mfd->gamma);
                                hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                                ShowWindow(hWnd, SW_SHOW);}
                        }
                        switch  (mfd->drwmode[mfd->channel_mode]) {
                            case DRAWMODE_PEN:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_CHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                                break;
                            case DRAWMODE_LINEAR:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_CHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                                break;
                            case DRAWMODE_SPLINE:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_CHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                                break;
                            case DRAWMODE_GAMMA:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_CHECKED);
                                break;
                        }
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                        GrdDrawBorder(GetDlgItem(hdlg, IDC_HBORDER), GetDlgItem(hdlg, IDC_VBORDER), mfd);
                        mfd->ifp->RedoFrame();
                    }
                }
                return TRUE;
            }
            break;
    }
    return FALSE;
}

static int ConfigProc(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd) {
    MyFilterData *mfd = (MyFilterData *) fa->filter_data;
    MyFilterData mfd_old = *mfd;
    int ret;

    mfd->ifp = fa->ifp;
    if (DialogBoxParam(hInst,
            MAKEINTRESOURCE(IDD_FILTER), hwnd,
            ConfigDlgProc, (LPARAM) mfd))
    {
        *mfd = mfd_old;
        ret = TRUE;
    }
    else
    {
        ret = FALSE;
    }
    return(ret);
}

static void StringProc(const FilterActivation *fa, const FilterFunctions *ff, char *str) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

    sprintf(str, " (mode: %s)",process_names[mfd->process]);
}

static void ScriptConfig(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
    FilterActivation *fa = (FilterActivation *)lpVoid;
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;
    int j;
    int t;
    int cc;
    int cnt;
    bool nf;
    const char *tmp;
    nf = false;

    mfd->process = Process(argv[0].asInt());
    tmp = *argv[1].asString();
    for (j=0; j<5; j++) { //read raw curve data
        for (i=(j*256); i<((j+1)*256); i++) {
            sscanf(tmp + 2*i,"%02x", &t);
            mfd->ovalue[j][i-(j*256)] = t;
            switch (j) { //for faster RGB modes
                case 0:
                    mfd->rvalue[0][i]=(mfd->ovalue[0][i]<<16);
                    mfd->rvalue[2][i]=(mfd->ovalue[0][i]-i)<<16;
                    mfd->gvalue[0][i]=(mfd->ovalue[0][i]<<8);
                    mfd->gvalue[2][i]=(mfd->ovalue[0][i]-i)<<8;
                    mfd->bvalue[i]=mfd->ovalue[0][i]-i;
                break;
                case 1:
                    mfd->rvalue[1][i-256]=(mfd->ovalue[1][i-256]<<16);
                break;
                case 2:
                    mfd->gvalue[1][i-512]=(mfd->ovalue[2][i-512]<<8);
                break;}
        }
    }
    if (argc>2){ //new format extra data
        tmp = *argv[2].asString();
        sscanf(tmp +1,"%01x", &cc);
        cnt=2;
        cc=5;
        for (i=0;i<cc;i++) {
            sscanf(tmp + cnt,"%01x", &mfd->drwmode[i]);
            cnt++;}
        for (i=0;i<cc;i++) {
            sscanf(tmp + cnt,"%02x", &mfd->poic[i]);
            cnt=cnt+2;}
        for (j=0;j<cc;j++) {
            for (i=0; i<mfd->poic[j];i++){
                sscanf(tmp + cnt,"%02x", &mfd->drwpoint[j][i][0]);
                cnt=cnt+2;
                sscanf(tmp + cnt,"%02x", &mfd->drwpoint[j][i][1]);
                cnt=cnt+2;}
        }
    }
    else { //add data to old format for compatibility
        for (i=0;i<5;i++) {
            mfd->drwmode[i]=DRAWMODE_PEN;
            mfd->poic[i]=2;
            mfd->drwpoint[i][0][0]=0;
            mfd->drwpoint[i][0][1]=0;
            mfd->drwpoint[i][1][0]=255;
            mfd->drwpoint[i][1][1]=255;}
    }
}

static bool FssProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;
    int j;
    char tmp[16];
    std::ostringstream os;

    os << "Config(" << mfd->process << ",\"";
    for (j=0; j<5; j++) {
        for (i=0; i<256; i++) {
            sprintf(tmp, "%02x", mfd->ovalue[j][i]);
            os << tmp;
        }
    }
    os << "\",\"" << 1 << 5;
    for (i=0;i<5;i++) {
        sprintf(tmp, "%01x", mfd->drwmode[i]);
        os << tmp;
    }
    for (i=0;i<5;i++) {
        sprintf(tmp, "%02x", mfd->poic[i]);
        os << tmp;
    }
    for (j=0; j<5;j++) {
        for (i=0; i<mfd->poic[j]; i++) {
            sprintf(tmp, "%02x%02x", mfd->drwpoint[j][i][0], mfd->drwpoint[j][i][1]);
            os << tmp;
        }
    }
    os << "\")";

    const std::string &s = os.str();
    if (int(s.size()) < buflen) {
        strcpy(buf, s.c_str());
        return true;
    }
    return false;
}

static void GrdDrawGradTable(HWND hWnd, int table[], int loff, DrawMode dmode, int dp[16][2], int pc, int ap)  // draw the curve
{
    RECT rect;

    InvalidateRect(hWnd, NULL, TRUE);
    UpdateWindow(hWnd);
    GetClientRect(hWnd, &rect);
    double scaleX;
    double scaleY;
    char *tmp;
    tmp = "";
    int buflen=1000;
    int loffx;
    int loffy;

    int i;
    HDC hdc;
    HPEN hPen;
    HPEN hPen2;
    HBRUSH hBrush;

    loffx=int((double(rect.right - rect.left-1)/256)*loff);
    loffy=int((double(rect.bottom - rect.top)/256)*loff);
    hdc = GetDC(hWnd);

    scaleX = (double)(rect.right - rect.left-1)/4.0;
    scaleY = (double)(rect.bottom - rect.top)/4.0;

    SelectObject(hdc, CreatePen(PS_DOT, 1, RGB(70, 70, 70)));

    for(i = 1; i < 4; i++)
    {
        MoveToEx(hdc, rect.left + int(scaleX*i+0.5)+loffx, rect.top, NULL);
        LineTo(hdc, rect.left + int(scaleX*i+0.5)+loffx, rect.bottom - 1);
    }

    DeleteObject(SelectObject(hdc, CreatePen(PS_DOT, 1, RGB(70, 70, 70))));

    for(i = 1; i < 4; i++)
    {
        MoveToEx(hdc, rect.left, rect.bottom - int(scaleY*i+0.5)-loffy - 1, NULL);
        LineTo(hdc, rect.right, rect.bottom - int(scaleY*i+0.5)-loffy - 1);
    }

    DeleteObject(SelectObject(hdc, hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0))));

    scaleX = (double)(rect.right - rect.left-1) / 256.0;
    scaleY = (double)(rect.bottom - rect.top) / 256.0;

    MoveToEx(hdc, rect.left, rect.bottom - int(scaleY * (table[0])+1), NULL);
    for(i = 0; i < 256; i++)
    {
        LineTo(hdc, rect.left+int(scaleX*(i)+0.5), rect.bottom-int(scaleY*(table[i])+1.5));
    }
    LineTo(hdc, rect.left + int(scaleX * (255)), rect.bottom - int(scaleY * (table[255])));

    DeleteObject(SelectObject(hdc, hPen = CreatePen(PS_SOLID, 1, RGB(0,0,0))));
    hPen2 = CreatePen(PS_SOLID, 1, RGB(255,0,0));
    hBrush=CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(hdc,hBrush);
    if (dmode!=DRAWMODE_PEN) {
        for (i=0;i<pc;i++){
            if (i==ap) {SelectObject(hdc,hPen2);
                Rectangle(hdc, int((rect.left+scaleX *(dp[i][0]))-2), rect.bottom-int((scaleY *(dp[i][1]))-2), rect.left+int((scaleX *(dp[i][0]))+3), rect.bottom-int((scaleY *(dp[i][1]))+3));
                SelectObject(hdc,hPen);}
            else {Rectangle(hdc, rect.left+int((scaleX *(dp[i][0]))-2),rect.bottom-int((scaleY *(dp[i][1]))-2),rect.left+int((scaleX *(dp[i][0]))+3),rect.bottom-int((scaleY *(dp[i][1]))+3));}
        }
    }
    DeleteObject(hBrush);
    DeleteObject(hPen);
    DeleteObject(hPen2);
    ReleaseDC(hWnd, hdc);
}

static void GrdDrawBorder(HWND hWnd, HWND hWnd2, MyFilterData *mfd) // draw the two color borders
{
    RECT rect;
    double scaleX;
    double scaleY;
    int YY;
    int XX;
    int i;
    int j;
    int dif;
    int difb;
    int x;
    int y;
    int z;
    int r;
    int g;
    int b;
    int rr;
    int gg;
    int bb;
    int ch;
    int chi;
    int lab;
    int border;
    int start;
    int end;
    HDC hdc;
    HPEN hPen;

    for (border = 0; border < 2; border++) {
        if (border == 0) {
            InvalidateRect(hWnd, NULL, TRUE);
            UpdateWindow(hWnd);
            GetClientRect(hWnd, &rect);
            hdc = GetDC(hWnd);
            scaleX = (double)(rect.right - rect.left-1) / 256.0;
            YY=rect.bottom-rect.top;
            dif = (YY+1)/3;
            difb = 256/(rect.bottom-rect.top);
            start=1;
            end=YY;}
        else {
            InvalidateRect(hWnd2, NULL, TRUE);
            UpdateWindow(hWnd2);
            GetClientRect(hWnd2, &rect);
            hdc = GetDC(hWnd2);
            XX=rect.right-rect.left;
            scaleY = (double)(rect.bottom - rect.top) / 256.0;
            dif = (XX+1)/3;
            difb = 256/(rect.right-rect.left);
            start=0;
            end=XX-1;}
        SelectObject(hdc, CreatePen(PS_SOLID, 1, RGB(0, 0, 0)));
        for (j = start; j <= end; j++)
        {   if (border == 0) {
                MoveToEx(hdc, rect.left, rect.bottom - j, NULL);
                LineTo(hdc, rect.left, rect.bottom  - j);}
            else {
                MoveToEx(hdc, rect.left + j, rect.bottom - 1, NULL);
                LineTo(hdc, rect.left + j, rect.bottom - 1);}
            for(i = 0; i < 256; i++)
            {   switch(mfd->space_mode)
                {
                    case SPACE_RGB:
                        switch(mfd->channel_mode)
                        {
                        case CHANNEL_RGB:
                            r = i;
                            g = i;
                            b = i;
                        break;
                        case CHANNEL_RED:
                            r = i;
                            g = 0;
                            b = 0;
                        break;
                        case CHANNEL_GREEN:
                            r = 0;
                            g = i;
                            b = 0;
                        break;
                        case CHANNEL_BLUE:
                            r = 0;
                            g = 0;
                            b = i;
                        break;
                        }
                    break;
                    case SPACE_YUV:
                        switch(mfd->channel_mode)
                        {
                        case CHANNEL_Y:
                            x = i<<16;
                            y = 0;
                            z = 0;
                        break;
                        case CHANNEL_U:
                            x = (j*difb)<<16;
                            y = i-128;
                            z = 0;
                        break;
                        case CHANNEL_V:
                            x = (j*difb)<<16;
                            y = 0;
                            z = i-128;
                        break;
                        }
                        rr = (32768 + x + 91881 * z);
                        if (rr<0) {r=0;} else if (rr>16711680) {r=255;} else {r = rr>>16;}
                        gg = (32768 + x - 22553 * y - 46802 * z);
                        if (gg<0) {g=0;} else if (gg>16711680) {g=255;} else {g = gg>>16;}
                        bb = (32768 + x + 116130 * y);
                        if (bb<0) {b=0;} else if (bb>16711680) {b=255;} else {b = bb>>16;}
                    break;
                    case SPACE_CMYK:
                        switch(mfd->channel_mode)
                        {
                        case CHANNEL_CYAN:
                            r = 255 - i;
                            g = 255;
                            b = 255;
                        break;
                        case CHANNEL_MAGENTA:
                            r = 255;
                            g = 255 - i;
                            b = 255;
                        break;
                        case CHANNEL_YELLOW:
                            r = 255;
                            g = 255;
                            b = 255 - i;
                        break;
                        case CHANNEL_BLACK:
                            r = 255 - i;
                            g = 255 - i;
                            b = 255 - i;
                        break;
                        }
                    break;
                    case SPACE_HSV:
                        switch(mfd->channel_mode)
                        {
                        case CHANNEL_HUE:
                            x = i;
                            y = 255;
                            z = 255;
                        break;
                        case CHANNEL_SATURATION:
                            x = (j/dif)*86;
                            y = i;
                            z = 255;
                        break;
                        case CHANNEL_VALUE:
                            x = 0;
                            y = 0;
                            z = i;
                        break;
                        }
                        if (y==0)
                        {
                            r = z;
                            g = z;
                            b = z;
                        }
                        else
                        {   chi = ((x*6)&0xFF00);
                            ch  = (x*6-chi);;
                            switch(chi)
                            {
                            case 0:
                                r = z;
                                g = (z*(65263-(y*(256-ch)))+65531)>>16;
                                b = (z*(255-y)+94)>>8;
                                break;
                            case 256:
                                r = (z*(65263-y*ch)+65528)>>16;
                                g = z;
                                b = (z*(255-y)+89)>>8;
                                break;
                            case 512:
                                r = (z*(255-y)+89)>>8;
                                g = z;
                                b = (z*(65267-(y*(256-ch)))+65529)>>16;
                                break;
                            case 768:
                                r = (z*(255-y)+89)>>8;
                                g = (z*(65267-y*ch)+65529)>>16;
                                b = z;
                                break;
                            case 1024:
                                r = (z*(65263-(y*(256-ch)))+65528)>>16;
                                g = (z*(255-y)+89)>>8;
                                b = z;
                                break;
                            default:
                                r = z;
                                g = (z*(255-y)+89)>>8;
                                b = (z*(65309-y*(ch+1))+27)>>16;
                                break;
                            }
                        }
                    break;
                    case SPACE_LAB:
                        switch(mfd->channel_mode)
                        {
                        case CHANNEL_L:
                            lab=labrgb[((i<<16)+30603)];
                        break;
                        case CHANNEL_A:
                            lab=labrgb[(((j*difb)<<16)+(i<<8)+136)];
                        break;
                        case CHANNEL_B:
                            lab=labrgb[(((j*difb)<<16)+30464+i)];
                        break;
                        }
                        r = (lab & 0xFF0000)>>16;
                        g = (lab & 0x00FF00)>>8;
                        b = (lab & 0x0000FF);
                    break;
                }
                if (border == 0) {
                    DeleteObject(SelectObject(hdc, hPen = CreatePen(PS_SOLID, 1, RGB(r, g, b))));
                    LineTo(hdc, rect.left + 2 + (int)(scaleX * (i)), rect.bottom  - j);}
                else {
                    DeleteObject(SelectObject(hdc, hPen = CreatePen(PS_SOLID, 1, RGB(r, g, b))));
                    LineTo(hdc, rect.left + j, rect.bottom  - 2 - (int)(scaleY * (i)));}
            }
        }
        ReleaseDC(hWnd, hdc);
        DeleteObject(hPen);
    }
}
