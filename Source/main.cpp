/*
    Gradation Filter v1.21 for VirtualDub -- adjusts the
    gradation curve.
    Copyright (C) 2005 Alexander Nagiller

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

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "ScriptInterpreter.h"
#include "ScriptError.h"
#include "ScriptValue.h"

#include "resource.h"
#include "filter.h"

///////////////////////////////////////////////////////////////////////////

int RunProc(const FilterActivation *fa, const FilterFunctions *ff);
int StartProc(FilterActivation *fa, const FilterFunctions *ff);
int EndProc(FilterActivation *fa, const FilterFunctions *ff);
long ParamProc(FilterActivation *fa, const FilterFunctions *ff);
int InitProc(FilterActivation *fa, const FilterFunctions *ff);
void DeinitProc(FilterActivation *fa, const FilterFunctions *ff);
int ConfigProc(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd);
void StringProc(const FilterActivation *fa, const FilterFunctions *ff, char *str);
void ScriptConfig(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc);
bool FssProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen);

///////////////////////////////////////////////////////////////////////////
static LRESULT CALLBACK FiWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

HINSTANCE g_hInst;

const char BBFilterWindowName[]="BBFilterWindow";

bool WINAPI DllMain(HINSTANCE hInst, ULONG ulReason, LPVOID lpReserved) {
    g_hInst = hInst;
    return TRUE;
}

ATOM RegisterFilterControl() {
    WNDCLASS wc;

    wc.style        = CS_DBLCLKS;
    wc.lpfnWndProc  = FiWndProc;
    wc.cbClsExtra   = 0;
    wc.cbWndExtra   = 0;
    wc.hInstance    = g_hInst;
    wc.hIcon        = NULL;
    wc.hCursor      = NULL;
    wc.hbrBackground= (HBRUSH)(COLOR_3DFACE+1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName= BBFilterWindowName;

    return RegisterClass(&wc);
};

LRESULT CALLBACK FiWndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){
    switch(message){
    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON)
        {
        SendMessage((GetParent (hwnd)),WM_USER + 100,(LOWORD (lParam)),(HIWORD (lParam)));
        }
        return 0;
    case WM_LBUTTONDOWN:
        SendMessage((GetParent (hwnd)),WM_USER + 101,0,0);
        return 0;
    }
    return DefWindowProc (hwnd, message, wParam, lParam) ;
}

enum {
    CHANNEL_RGB             = 0,
    CHANNEL_RED             = 1,
    CHANNEL_GREEN           = 2,
    CHANNEL_BLUE            = 3,
};

static char *channel_names[]={
    "RGB",
    "Red",
    "Green",
    "Blue",
};

enum {
    PROCESS_RGB     = 0,
    PROCESS_FULL    = 1,
    PROCESS_RGBW    = 2,
    PROCESS_FULLW   = 3,
    PROCESS_OFF     = 4,
};

static char *process_names[]={
    "RGB only",
    "RGB + R/G/B",
    "RGB weighted",
    "RGB weighted + R/G/B",
    "off",
};

typedef struct MyFilterData {
    IFilterPreview *ifp;
    Pixel32 *evaluer;
    Pixel32 *evalueg;
    Pixel32 *evalueb;
    Pixel32 *cvaluer;
    Pixel32 *cvalueg;
    Pixel32 *cvalueb;
    int ovalue[4][256];
    int value;
    int channel_mode;
    int process;
    int xl;
    int yl;
    char filename[1024];

} MyFilterData;

ScriptFunctionDef func_defs[]={
    { (ScriptFunctionPtr)ScriptConfig, "Config", "0is" },
    { NULL },
};

CScriptObject script_obj={
    NULL, func_defs
};

struct FilterDefinition filterDef = {

    NULL, NULL, NULL,       // next, prev, module
    "gradation curves",     // name
    "Adjusts the gradation curves. The curves can be used for coring and invert as well. Version 1.21",
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

    vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
    vdfd_compat = VIRTUALDUB_FILTERDEF_COMPATIBLE;

    return 0;
}

void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(FilterModule *fm, const FilterFunctions *ff) {
    ff->removeFilter(fd);
}

///////////////////////////////////////////////////////////////////////////

void GrdDrawTable(HWND hWnd, int table[]);
void GrdDrawGradTable(HWND hWnd, MyFilterData *mfd);
void GrdDrawHBorder(HWND hWnd, MyFilterData *mfd);
void GrdDrawVBorder(HWND hWnd, MyFilterData *mfd);
void ImportCurve(HWND hWnd, MyFilterData *mfd);
void ExportCurve(HWND hWnd, MyFilterData *mfd);

///////////////////////////////////////////////////////////////////////////

int StartProc(FilterActivation *fa, const FilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;

    mfd->evaluer = new Pixel32[256];
    mfd->evalueg = new Pixel32[256];
    mfd->evalueb = new Pixel32[256];
    mfd->cvaluer = new Pixel32[256];
    mfd->cvalueg = new Pixel32[256];
    mfd->cvalueb = new Pixel32[256];

    for (i=0; i<256; i++) {
        mfd->cvaluer[i] = (mfd->ovalue[1][i] - i)*65536;
        mfd->cvalueg[i] = (mfd->ovalue[2][i] - i)*256;
        mfd->cvalueb[i] = (mfd->ovalue[3][i] - i);
        mfd->evaluer[i] = (mfd->ovalue[0][i] - i)*65536;
        mfd->evalueg[i] = (mfd->ovalue[0][i] - i)*256;
        mfd->evalueb[i] = (mfd->ovalue[0][i] - i);
    }
    return 0;
}

int RunProc(const FilterActivation *fa, const FilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    PixDim w, h;
    Pixel32 *src, *dst;
    const PixDim    width = fa->src.w;
    const PixDim    height = fa->src.h;
    const Pixel32 *evaluer = mfd->evaluer;
    const Pixel32 *evalueg = mfd->evalueg;
    const Pixel32 *evalueb = mfd->evalueb;
    const Pixel32 *cvaluer = mfd->cvaluer;
    const Pixel32 *cvalueg = mfd->cvalueg;
    const Pixel32 *cvalueb = mfd->cvalueb;
    long r;
    int g;
    int b;
    int bw;

    src = (Pixel32 *)fa->src.data;
    dst = (Pixel32 *)fa->dst.data;
    Pixel32 old_pixel, new_pixel, med_pixel;

    switch(mfd->process)
    {
    case 0:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                new_pixel = ((old_pixel & 0xFF0000) + evaluer[(old_pixel & 0xFF0000)>>16]) + ((old_pixel & 0x00FF00) + evalueg[(old_pixel & 0x00FF00)>>8]) + ((old_pixel & 0x0000FF) + evalueb[(old_pixel & 0x0000FF)]);
                *dst++ = new_pixel;
            }
            src = (Pixel32 *)((char *)src + fa->src.modulo);
            dst = (Pixel32 *)((char *)dst + fa->dst.modulo);
        }
    break;
    case 1:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                med_pixel = ((old_pixel & 0xFF0000) + cvaluer[(old_pixel & 0xFF0000)>>16]) + ((old_pixel & 0x00FF00) + cvalueg[(old_pixel & 0x00FF00)>>8]) + ((old_pixel & 0x0000FF) + cvalueb[(old_pixel & 0x0000FF)]);
                new_pixel = ((med_pixel & 0xFF0000) + evaluer[(med_pixel & 0xFF0000)>>16]) + ((med_pixel & 0x00FF00) + evalueg[(med_pixel & 0x00FF00)>>8]) + ((med_pixel & 0x0000FF) + evalueb[(med_pixel & 0x0000FF)]);
                *dst++ = new_pixel;
            }
            src = (Pixel32 *)((char *)src + fa->src.modulo);
            dst = (Pixel32 *)((char *)dst + fa->dst.modulo);
        }
    break;
    case 2:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                r = (old_pixel & 0xFF0000);
                g = (old_pixel & 0x00FF00);
                b = (old_pixel & 0x0000FF);
                bw = int((77 * (r >> 16) + 150 * (g >> 8) + 29 * b)>>8);//((54 * (r >> 16) + 183 * (g >> 8) + 19 * b)/256);
                    r = r+evaluer[bw];
                    if (r<65536) r=0; else if (r>16711680) r=16711680;
                    g = g+evalueg[bw];
                    if (g<256) g=0; else if (g>65280) g=65280;
                    b = b+evalueb[bw];
                    if (b<0) b=0; else if (b>255) b=255;
                new_pixel = (r+g+b);
                *dst++ = new_pixel;
            }
            src = (Pixel32 *)((char *)src + fa->src.modulo);
            dst = (Pixel32 *)((char *)dst + fa->dst.modulo);
        }
    break;
    case 3:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                med_pixel = ((old_pixel & 0xFF0000) + cvaluer[(old_pixel & 0xFF0000)>>16]) + ((old_pixel & 0x00FF00) + cvalueg[(old_pixel & 0x00FF00)>>8]) + ((old_pixel & 0x0000FF) + cvalueb[(old_pixel & 0x0000FF)]);
                r = (med_pixel & 0xFF0000);
                g = (med_pixel & 0x00FF00);
                b = (med_pixel & 0x0000FF);
                bw = int((77 * (r >> 16) + 150 * (g >> 8) + 29 * b)>>8);//int((54 * (r >> 16) + 183 * (g >> 8) + 19 * b)/256);
                    r = r+evaluer[bw];
                    if (r<65536) r=0; else if (r>16711680) r=16711680;
                    g = g+evalueg[bw];
                    if (g<256) g=0; else if (g>65280) g=65280;
                    b = b+evalueb[bw];
                    if (b<0) b=0; else if (b>255) b=255;
                new_pixel = (r+g+b);
                *dst++ = new_pixel;
            }
            src = (Pixel32 *)((char *)src + fa->src.modulo);
            dst = (Pixel32 *)((char *)dst + fa->dst.modulo);
        }
    break;
    case 4:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                new_pixel = old_pixel;
                *dst++ = new_pixel;
            }
            src = (Pixel32 *)((char *)src + fa->src.modulo);
            dst = (Pixel32 *)((char *)dst + fa->dst.modulo);
        }
    break;
    }
    return 0;
}

int EndProc(FilterActivation *fa, const FilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

    delete[] mfd->evaluer; mfd->evaluer = NULL;
    delete[] mfd->evalueg; mfd->evalueg = NULL;
    delete[] mfd->evalueb; mfd->evalueb = NULL;
    delete[] mfd->cvaluer; mfd->cvaluer = NULL;
    delete[] mfd->cvalueg; mfd->cvalueg = NULL;
    delete[] mfd->cvalueb; mfd->cvalueb = NULL;

    return 0;
}

int InitProc(FilterActivation *fa, const FilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;
    int ovalue[4][256];

    mfd->value = 0;
    mfd->process = 0;
    mfd->xl = 300;
    mfd->yl = 300;
    for (i=0; i<256; i++) {
        mfd->ovalue[0][i] = i;
        mfd->ovalue[1][i] = i;
        mfd->ovalue[2][i] = i;
        mfd->ovalue[3][i] = i;
    }

    return 0;
}

void DeinitProc(FilterActivation *fa, const FilterFunctions *ff) {
    UnregisterClass("FiWndProc", g_hInst);
}

BOOL CALLBACK ConfigDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    MyFilterData *mfd = (MyFilterData *)GetWindowLong(hdlg, DWL_USER);
    int i;
    int r;
    int inv[256];
    int cx;
    int cy;
    int ax;
    int ay;
    int inc;

    switch(msg) {
        case WM_INITDIALOG:
            SetWindowLong(hdlg, DWL_USER, lParam);
            mfd = (MyFilterData *)lParam;
            HWND hWnd;
            int i;

            hWnd = GetDlgItem(hdlg, IDC_SVALUE);
            SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 255));
            SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->value);
            SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
            SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[0][mfd->value], FALSE);
            hWnd = GetDlgItem(hdlg, IDC_CHANNEL);
            for(i=0; i<(sizeof channel_names/sizeof channel_names[0]); i++)
                SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)channel_names[i]);
            SendMessage(hWnd, CB_SETCURSEL, mfd->channel_mode, 0);
            switch (mfd->process)
            {
                case PROCESS_RGB: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); break;
                case PROCESS_FULL: CheckDlgButton(hdlg, IDC_FULL,BST_CHECKED); break;
                case PROCESS_RGBW: CheckDlgButton(hdlg, IDC_RGBW,BST_CHECKED); break;
                case PROCESS_FULLW: CheckDlgButton(hdlg, IDC_FULLW,BST_CHECKED); break;
                case PROCESS_OFF: CheckDlgButton(hdlg, IDC_OFF,BST_CHECKED); break;
            }
            mfd->ifp->InitButton(GetDlgItem(hdlg, IDPREVIEW));

            return TRUE;

        case WM_PAINT:
            {
                PAINTSTRUCT ps;

                BeginPaint(hdlg, &ps);
                EndPaint(hdlg, &ps);
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd);
                GrdDrawHBorder(GetDlgItem(hdlg, IDC_HBORDER), mfd);
                GrdDrawVBorder(GetDlgItem(hdlg, IDC_VBORDER), mfd);
            }
            return TRUE;

        case WM_USER + 100:
            if (wParam > 255)
            {
                ax = 255;
            }
            if (wParam <= 255)
            {
                ax = wParam;
            }
            if (lParam > 255)
            {
                ay = 0;
            }
            if (lParam <= 255)
            {
                ay = 255-lParam;
            }
            mfd->ovalue[mfd->channel_mode][ax]=(ay);

            if (ax > mfd->xl)
            {
                if ((ax-(mfd->xl))>1 && (ax-(mfd->xl))<256 && (mfd->xl)<256 && (mfd->yl)<256)
                {
                    if (ay > (mfd->yl))
                    {
                        cy=(((ay-(mfd->yl))*255)/(ax-(mfd->xl)));
                        for (cx=((mfd->xl)+1);cx<ax;cx++)
                        {
                            mfd->ovalue[mfd->channel_mode][cx]=((mfd->ovalue[mfd->channel_mode][mfd->xl])+(((cx-(mfd->xl))*cy)/255));
                        }
                    }
                    else
                    {
                        cy=(((mfd->yl)-ay)*255)/(ax-(mfd->xl));
                        for (cx=((mfd->xl)+1);cx<ax;cx++)
                        {
                            mfd->ovalue[mfd->channel_mode][cx]=((mfd->ovalue[mfd->channel_mode][mfd->xl])-(((cx-(mfd->xl))*cy)/255));
                        }
                    }
                }
            }
            else
            {
                if (((mfd->xl)-ax)>1 && ((mfd->xl)-ax)<100 && (mfd->xl)<256 && (mfd->yl)<256)
                {
                    if (ay >= mfd->yl)
                    {
                        cy=((ay-(mfd->yl))*100)/((mfd->xl)-ax);
                        for (cx=((mfd->xl)-1);cx>ax;cx--)
                        {
                            mfd->ovalue[mfd->channel_mode][cx]=((mfd->ovalue[mfd->channel_mode][mfd->xl])+((((mfd->xl)-cx)*cy)/100));
                        }
                    }
                    else
                    {
                        cy=(((mfd->yl)-ay)*100)/((mfd->xl)-ax);
                        for (cx=((mfd->xl)-1);cx>ax;cx--)
                        {
                            mfd->ovalue[mfd->channel_mode][cx]=((mfd->ovalue[mfd->channel_mode][mfd->xl])-((((mfd->xl)-cx)*cy)/100));
                        }
                    }
                }
            }
            mfd->xl = ax;
            mfd->yl = ay;
            if (ax = mfd->value)
            {
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
            }
            GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd);
            mfd->ifp->RedoSystem();
            break;

        case WM_USER + 101:
            mfd->xl = 300;
            mfd->yl = 300;
            break;

        case WM_HSCROLL:
            if ((HWND) lParam == GetDlgItem(hdlg, IDC_SVALUE))
            {
                int value = SendMessage(GetDlgItem(hdlg, IDC_SVALUE), TBM_GETPOS, 0, 0);
                if (value != mfd->value)
                {
                    mfd->value = value;
                    SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                }
            }
        break;

        case WM_COMMAND:
            switch(LOWORD(wParam)) {
            case IDPREVIEW:
                mfd->ifp->Toggle(hdlg);
                break;
            case IDCANCEL:
                EndDialog(hdlg, 1);
                return TRUE;
            case IDOK:
                EndDialog(hdlg, 0);
                return TRUE;
            case IDHELP:
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
            case IDIMPORT:
                {
                OPENFILENAME ofn;
                mfd->filename[0] = NULL;
                ofn.lStructSize = sizeof(OPENFILENAME);
                ofn.hwndOwner = hdlg;
                ofn.hInstance = NULL;
                ofn.lpTemplateName = NULL;
                ofn.lpstrFilter = "Map Settings (*.amp)\0*.amp\0All Files\0*.*\0\0";
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
                if (mfd->filename[0] != 0)
                {
                    ImportCurve (hdlg, mfd);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd);
                    mfd->ifp->RedoSystem();
                }
                break;
                }
                return TRUE;
            case IDEXPORT:
                {
                OPENFILENAME ofn;
                mfd->filename[0] = NULL;
                ofn.lStructSize = sizeof(OPENFILENAME);
                ofn.hwndOwner = hdlg;
                ofn.hInstance = NULL;
                ofn.lpTemplateName = NULL;
                ofn.lpstrFilter = "Map Settings (*.amp)\0*.amp\0All Files\0*.*\0\0";
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
                if (mfd->filename[0] != 0)
                {
                    ExportCurve (hdlg, mfd);
                }
                break;
                }
                return TRUE;
            case IDC_RGB:
                mfd->process = PROCESS_RGB;
                mfd->ifp->RedoSystem();
            break;
            case IDC_FULL:
                mfd->process = PROCESS_FULL;
                mfd->ifp->RedoSystem();
            break;
            case IDC_RGBW:
                mfd->process = PROCESS_RGBW;
                mfd->ifp->RedoSystem();
            break;
            case IDC_FULLW:
                mfd->process = PROCESS_FULLW;
                mfd->ifp->RedoSystem();
            break;
            case IDC_OFF:
                mfd->process = PROCESS_OFF;
                mfd->ifp->RedoSystem();
            break;
            case IDC_INPUTPLUS:
                if (mfd->value < 255)
                {
                    mfd->value++;
                    SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
                    SendMessage(GetDlgItem(hdlg, IDC_SVALUE), TBM_SETPOS, (WPARAM)TRUE, mfd->value);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                }
                break;
            case IDC_INPUTMINUS:
                if (mfd->value > 0)
                {
                    mfd->value--;
                    SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
                    SendMessage(GetDlgItem(hdlg, IDC_SVALUE), TBM_SETPOS, (WPARAM)TRUE, mfd->value);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                }
                break;
            case IDC_OUTPUTPLUS:
                if (mfd->ovalue[mfd->channel_mode][mfd->value] < 255)
                {
                    mfd->ovalue[mfd->channel_mode][mfd->value]++;
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                }
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd);
                mfd->ifp->RedoSystem();
                break;
            case IDC_OUTPUTMINUS:
                if (mfd->ovalue[mfd->channel_mode][mfd->value] > 0)
                {
                    mfd->ovalue[mfd->channel_mode][mfd->value]--;
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[0][mfd->value], FALSE);
                }
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd);
                mfd->ifp->RedoSystem();
                break;
            case IDC_RESET:
                for (i=0; i<256; i++)
                {
                    mfd->ovalue[mfd->channel_mode][i] = i;
                }
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd);
                mfd->ifp->RedoSystem();
                break;
            case IDC_INVERT:
                r=255;
                for (i=0; i<256; i++)
                {
                    inv[r] = mfd->ovalue[mfd->channel_mode][i];
                    r--;
                }
                for (i=0; i<256; i++)
                {
                    mfd->ovalue[mfd->channel_mode][i] = inv[i];
                }
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd);
                mfd->ifp->RedoSystem();
                break;
            case IDC_CHANNEL:
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    mfd->channel_mode = SendDlgItemMessage(hdlg, IDC_CHANNEL, CB_GETCURSEL, 0, 0);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd);
                    GrdDrawHBorder(GetDlgItem(hdlg, IDC_HBORDER), mfd);
                    GrdDrawVBorder(GetDlgItem(hdlg, IDC_VBORDER), mfd);
                    mfd->ifp->RedoSystem();
                }
                return TRUE;
            }
            break;
    }
    return FALSE;
}

int ConfigProc(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd) {
    MyFilterData *mfd = (MyFilterData *) fa->filter_data;
    MyFilterData mfd_old = *mfd;
    int ret;

    mfd->ifp = fa->ifp;
    if (DialogBoxParam(fa->filter->module->hInstModule,
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

void StringProc(const FilterActivation *fa, const FilterFunctions *ff, char *str) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

    sprintf(str, " (mode: %s)",process_names[mfd->process]);

}

void ScriptConfig(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
    FilterActivation *fa = (FilterActivation *)lpVoid;
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;
    int t;
    const char *tmp;

    mfd->process = argv[0].asInt();

    tmp = *argv[1].asString();
    for (i=0; i<256; i++) {
        sscanf(tmp + 2*i,"%02x", &t);
        mfd->ovalue[0][i] = t;
    }
    for (i=256; i<512; i++) {
        sscanf(tmp + 2*i,"%02x", &t);
        mfd->ovalue[1][(i-256)] = t;
    }
    for (i=512; i<768; i++) {
        sscanf(tmp + 2*i,"%02x", &t);
        mfd->ovalue[2][(i-512)] = t;
    }
    for (i=768; i<1024; i++) {
        sscanf(tmp + 2*i,"%02x", &t);
        mfd->ovalue[3][(i-768)] = t;
    }
}

bool FssProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;
    char *tmp;
    tmp = "";

    _snprintf(buf, buflen, "Config(%d,\"",mfd->process);

    for (i=0; i<256; i++) {
            _snprintf(tmp, buflen, "%02x",mfd->ovalue[0][i]);
            strcat (buf,tmp);
    }
    for (i=0; i<256; i++) {
            _snprintf(tmp, buflen, "%02x",mfd->ovalue[1][i]);
            strcat (buf,tmp);
    }
    for (i=0; i<256; i++) {
            _snprintf(tmp, buflen, "%02x",mfd->ovalue[2][i]);
            strcat (buf,tmp);
    }
    for (i=0; i<256; i++) {
            _snprintf(tmp, buflen, "%02x",mfd->ovalue[3][i]);
            strcat (buf,tmp);
    }
    strcat (buf, "\")");
    return true;
}

void GrdDrawTable(HWND hWnd, int table[])
{
    RECT rect;

    InvalidateRect(hWnd, NULL, TRUE);
    UpdateWindow(hWnd);
    GetClientRect(hWnd, &rect);
    double scaleX;
    double scaleY;

    int i;
    HDC hdc;
    HPEN hPen;

    hdc = GetDC(hWnd);

    scaleX = (double)(rect.right - rect.left) / 4.0;
    scaleY = (double)(rect.bottom - rect.top) / 4.0;

    SelectObject(hdc, CreatePen(PS_DOT, 1, RGB(0, 0, 0)));

    for(i = 1; i < 4; i++)
    {
        MoveToEx(hdc, rect.left + (int)(scaleX * i), rect.top, NULL);
        LineTo(hdc, rect.left + (int)(scaleX * i), rect.bottom - 1);
    }

    DeleteObject(SelectObject(hdc, CreatePen(PS_DOT, 1, RGB(0, 0, 0))));

    for(i = 1; i < 4; i++)
    {
        MoveToEx(hdc, rect.left, rect.bottom - (int)(scaleY * i) - 1, NULL);
        LineTo(hdc, rect.right, rect.bottom - (int)(scaleY * i) - 1);
    }

    DeleteObject(SelectObject(hdc, hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0))));

    scaleX = (double)(rect.right - rect.left) / 256.0;
    scaleY = (double)(rect.bottom - rect.top) / 256.0;

    MoveToEx(hdc, rect.left, rect.bottom - (int)(scaleY * (table[0])+1), NULL);

    for(i = 0; i < 256; i++)
    {
        LineTo(hdc, rect.left + (int)((scaleX * (i))), rect.bottom - (int)(scaleY * (table[i])+1));
    }

    LineTo(hdc, rect.left + (int)((scaleX * (255))), rect.bottom - (int)(scaleY * (table[255])));

    ReleaseDC(hWnd, hdc);
    DeleteObject(hPen);
}

void GrdDrawGradTable(HWND hWnd, MyFilterData *mfd)
{
    switch(mfd->channel_mode)
    {
    case 0:
        GrdDrawTable(hWnd, mfd->ovalue[0]);
    break;
    case 1:
        GrdDrawTable(hWnd, mfd->ovalue[1]);
    break;
    case 2:
        GrdDrawTable(hWnd, mfd->ovalue[2]);
    break;
    case 3:
        GrdDrawTable(hWnd, mfd->ovalue[3]);
    break;
    }
}

void GrdDrawHBorder(HWND hWnd, MyFilterData *mfd)
{
    RECT rect;

    InvalidateRect(hWnd, NULL, TRUE);
    UpdateWindow(hWnd);
    GetClientRect(hWnd, &rect);
    double scaleX;
    int Y;
    int i;
    int j;
    int r;
    int g;
    int b;
    HDC hdc;
    HPEN hPen;

    hdc = GetDC(hWnd);

    scaleX = (double)(rect.right - rect.left) / 257.0;
    Y = (int)(rect.bottom - rect.top);

    SelectObject(hdc, CreatePen(PS_SOLID, 1, RGB(0, 0, 0)));

    for (j = 1; j <= Y; j++)
    {
        MoveToEx(hdc, rect.left, rect.bottom - j, NULL);
        LineTo(hdc, rect.left, rect.bottom  - j);
        for(i = 0; i < 256; i++)
        {
            switch(mfd->channel_mode)
            {
            case 0:
                r = i;
                g = i;
                b = i;
            break;
            case 1:
                r = i;
                g = 0;
                b = 0;
            break;
            case 2:
                r = 0;
                g = i;
                b = 0;
            break;
            case 3:
                r = 0;
                g = 0;
                b = i;
            break;
            }
            DeleteObject(SelectObject(hdc, hPen = CreatePen(PS_SOLID, 1, RGB(r, g, b))));
            LineTo(hdc, rect.left + 2 + (int)(scaleX * (i)), rect.bottom  - j);
        }
    }
    ReleaseDC(hWnd, hdc);
    DeleteObject(hPen);
}

void GrdDrawVBorder(HWND hWnd, MyFilterData *mfd)
{
    RECT rect;

    InvalidateRect(hWnd, NULL, TRUE);
    UpdateWindow(hWnd);
    GetClientRect(hWnd, &rect);
    int X;
    double scaleY;
    int i;
    int j;
    int r;
    int g;
    int b;
    HDC hdc;
    HPEN hPen;

    hdc = GetDC(hWnd);

    X = (int)(rect.right - rect.left);
    scaleY = (double)(rect.bottom - rect.top) / 256.0;

    SelectObject(hdc, CreatePen(PS_SOLID, 1, RGB(0, 0, 0)));

    for (j = 0; j < X; j++)
    {
        MoveToEx(hdc, rect.left + j, rect.bottom - 1, NULL);
        LineTo(hdc, rect.left + j, rect.bottom - 1);
        for(i = 0; i < 256; i++)
        {
            switch(mfd->channel_mode)
            {
            case 0:
                r = i;
                g = i;
                b = i;
            break;
            case 1:
                r = i;
                g = 0;
                b = 0;
            break;
            case 2:
                r = 0;
                g = i;
                b = 0;
            break;
            case 3:
                r = 0;
                g = 0;
                b = i;
            break;
            }
            DeleteObject(SelectObject(hdc, hPen = CreatePen(PS_SOLID, 1, RGB(r, g, b))));
            LineTo(hdc, rect.left + j, rect.bottom  - 2 - (int)(scaleY * (i)));
        }
    }
    ReleaseDC(hWnd, hdc);
    DeleteObject(hPen);
}

void ImportCurve(HWND hWnd, MyFilterData *mfd)
{
    FILE *pFile;
    int i;
    int stor[1024];
    long lSize;

    pFile = fopen (mfd->filename, "rb");
    if (pFile==NULL)
    {
        SendMessage(GetDlgItem(hWnd, IDC_TEST), WM_SETTEXT, 0, (LPARAM)"Error opening file");
    }
    else
    {
        fseek (pFile , 0 , SEEK_END);
        lSize = ftell (pFile);
        rewind (pFile);
        if (lSize > 768)
        {
            for(i=0; (i < 1024) && ( feof(pFile) == 0 ); i++ )
            {
                stor[i] = fgetc(pFile);
            }
            fclose (pFile);
            for(i=0; i < 256; i++) {
                mfd->ovalue[0][i] = stor[i];
            }
            for(i=256; i < 512; i++) {
                mfd->ovalue[1][(i-256)] = stor[i];
            }
            for(i=512; i < 768; i++) {
                mfd->ovalue[2][(i-512)] = stor[i];
            }
            for(i=768; i < 1024; i++) {
                mfd->ovalue[3][(i-768)] = stor[i];
            }
        }
        if (lSize < 769 && lSize > 256)
        {
            for(i=0; (i < 768) && ( feof(pFile) == 0 ); i++ )
            {
                stor[i] = fgetc(pFile);
            }
            fclose (pFile);
            for(i=0; i < 256; i++) {
                mfd->ovalue[1][i] = stor[i];
            }
            for(i=256; i < 512; i++) {
                mfd->ovalue[2][(i-256)] = stor[i];
            }
            for(i=512; i < 768; i++) {
                mfd->ovalue[3][(i-512)] = stor[i];
            }
        }
        if (lSize < 257)
        {
            for(i=0; (i < 256) && ( feof(pFile) == 0 ); i++ )
            {
                stor[i] = fgetc(pFile);
            }
            fclose (pFile);
            for(i=0; i < 256; i++) {
                mfd->ovalue[0][i] = stor[i];
            }
        }
    }
}
void ExportCurve(HWND hWnd, MyFilterData *mfd)
{
char str [80];
    FILE *pFile;
    int i;
    char c;

    pFile = fopen (mfd->filename,"wb");
    for (i=0; i<256; i++) {
        c = char (mfd->ovalue[0][i]);
        fprintf (pFile, "%c",c);
    }
    for (i=0; i<256; i++) {
        c = char (mfd->ovalue[1][i]);
        fprintf (pFile, "%c",c);
    }
    for (i=0; i<256; i++) {
        c = char (mfd->ovalue[2][i]);
        fprintf (pFile, "%c",c);
    }
    for (i=0; i<256; i++) {
        c = char (mfd->ovalue[3][i]);
        fprintf (pFile, "%c",c);
    }
    for (i=0; i<256; i++) {
        c = char (i);
        fprintf (pFile, "%c",c);
    }
    fclose (pFile);
}
