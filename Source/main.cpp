/*
    Gradation Filter for VirtualDub -- adjusts the
    gradation curve.
    Copyright (C) 2003 A. Nagiller

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
int ConfigProc(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd);
void StringProc(const FilterActivation *fa, const FilterFunctions *ff, char *str);
void ScriptConfig(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc);
bool FssProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen);

///////////////////////////////////////////////////////////////////////////

typedef struct MyFilterData {
    IFilterPreview *ifp;
    Pixel32 *evalue;
    Pixel32 *evaluer;
    Pixel32 *evalueg;
    Pixel32 *evalueb;
    int ovalue[256];
    int value;
} MyFilterData;

ScriptFunctionDef func_defs[]={
    { (ScriptFunctionPtr)ScriptConfig, "Config", "0ii" },
    { NULL },
};

CScriptObject script_obj={
    NULL, func_defs
};

struct FilterDefinition filterDef = {

    NULL, NULL, NULL,       // next, prev, module
    "Gradation (0.9)",      // name
    "Edits the Gradation Curve. Coring and Invert are also possible",
                            // desc
    "A. Nagiller",          // maker
    NULL,                   // private_data
    sizeof(MyFilterData),   // inst_data_size

    InitProc,               // initProc
    NULL,                   // deinitProc
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
    if (!(fd = ff->addFilter(fm, &filterDef, sizeof(FilterDefinition))))
        return 1;

    vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
    vdfd_compat = VIRTUALDUB_FILTERDEF_COMPATIBLE;

    return 0;
}

void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(FilterModule *fm, const FilterFunctions *ff) {
    ff->removeFilter(fd);
}

int StartProc(FilterActivation *fa, const FilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;

    mfd->evalue = new Pixel32[256];
    mfd->evaluer = new Pixel32[256];
    mfd->evalueg = new Pixel32[256];
    mfd->evalueb = new Pixel32[256];

    for (i=0; i<256; i++) {
        mfd->evalue[i] = mfd->ovalue[i] - i;
        mfd->evaluer[i] = mfd->evalue[i]*65536;
        mfd->evalueg[i] = mfd->evalue[i]*256;
        mfd->evalueb[i] = mfd->evalue[i];
    }
    return 0;
}

int RunProc(const FilterActivation *fa, const FilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    PixDim w, h;
    Pixel32 *src, *dst;
    const PixDim width = fa->src.w;
    const PixDim height = fa->src.h;
    const Pixel32 *evaluer = mfd->evaluer;
    const Pixel32 *evalueg = mfd->evalueg;
    const Pixel32 *evalueb = mfd->evalueb;

    src = (Pixel32 *)fa->src.data;
    dst = (Pixel32 *)fa->dst.data;

    for (h = 0; h < height; h++)
    {
    for (w = 0; w < width; w++)
        {
        Pixel32 old_pixel, new_pixel;
        old_pixel = *src++;
        new_pixel = ((old_pixel & 0xFF0000) + evaluer[(old_pixel & 0xFF0000)>>16]) + ((old_pixel & 0x00FF00) + evalueg[(old_pixel & 0x00FF00)>>8]) + ((old_pixel & 0x0000FF) + evalueb[(old_pixel & 0x0000FF)]);
        *dst++ = new_pixel;
    }
    src = (Pixel32 *)((char *)src + fa->src.modulo);
    dst = (Pixel32 *)((char *)dst + fa->dst.modulo);
    }
    return 0;
}

int EndProc(FilterActivation *fa, const FilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;

    return 0;
}

int InitProc(FilterActivation *fa, const FilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;
    int ovalue[256];

    mfd->value = 0;
    for (i=0; i<256; i++) {
        mfd->ovalue[i] = i;
    }

    return 0;
}

BOOL CALLBACK ConfigDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    MyFilterData *mfd = (MyFilterData *)GetWindowLong(hdlg, DWL_USER);
    int i;
    int r;

    switch(msg) {
    case WM_INITDIALOG:
        SetWindowLong(hdlg, DWL_USER, lParam);
        mfd = (MyFilterData *)lParam;
            HWND hWnd;

            hWnd = GetDlgItem(hdlg, IDC_SVALUE);
            SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 255));
            SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->value);
            SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
            SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->value], FALSE);

            mfd->ifp->InitButton(GetDlgItem(hdlg, IDPREVIEW));

        return TRUE;

        case WM_HSCROLL:
            if ((HWND) lParam == GetDlgItem(hdlg, IDC_SVALUE))
            {
                int value = SendMessage(GetDlgItem(hdlg, IDC_SVALUE), TBM_GETPOS, 0, 0);
                if (value != mfd->value)
                {
                    mfd->value = value;
                    SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->value], FALSE);
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
            case IDC_INPUTPLUS:
                if (mfd->value < 255)
                {
                    mfd->value++;
                    SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->value], FALSE);
                    SendMessage(GetDlgItem(hdlg, IDC_SVALUE), TBM_SETPOS, (WPARAM)TRUE, mfd->value);
                }
                break;
            case IDC_INPUTMINUS:
                if (mfd->value > 0)
                {
                    mfd->value--;
                    SetDlgItemInt(hdlg, IDC_VALUE, mfd->value, FALSE);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->value], FALSE);
                    SendMessage(GetDlgItem(hdlg, IDC_SVALUE), TBM_SETPOS, (WPARAM)TRUE, mfd->value);
                }
                break;
            case IDC_OUTPUTPLUS:
                if (mfd->ovalue[mfd->value] < 255)
                {
                    mfd->ovalue[mfd->value]++;
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->value], FALSE);
                    mfd->ifp->RedoSystem();
                }
                break;
            case IDC_OUTPUTMINUS:
                if (mfd->ovalue[mfd->value] > 0)
                {
                    mfd->ovalue[mfd->value]--;
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->value], FALSE);
                    mfd->ifp->RedoSystem();
                }
                break;
            case IDC_RESET:
                for (i=0; i<256; i++) {
                    mfd->ovalue[i] = i;
                }
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->value], FALSE);
                mfd->ifp->RedoSystem();
                break;
            case IDC_INVERT:
                r=255;
                for (i=0; i<256; i++) {
                    mfd->ovalue[i] = r;
                    r--;
                }
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->value], FALSE);
                mfd->ifp->RedoSystem();;
                break;
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
    int i;
    char *tmp;

    tmp = "";
    strcpy (str, "");
    for (i=0; i<256; i++) {
        if (i == 0) {
            strcpy (str, "(");
            sprintf(tmp,"(%d",mfd->ovalue[i]);
            strcat (str,tmp);
        }
        if (i > 0) {
            sprintf(tmp,",%d",mfd->ovalue[i]);
            strcat (str,tmp);
        }
    }
    strcat (str, ")");
}

void ScriptConfig(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
    FilterActivation *fa = (FilterActivation *)lpVoid;
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;

    for (i=0; i<256; i++) {
        mfd->ovalue[i] = argv[i].asInt();
    }
}

bool FssProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;
    char *tmp;

    tmp = "";
    strcpy (buf, "");
    for (i=0; i<256; i++) {
        if (i == 0) {
            strcpy (buf, "Config(");
            _snprintf(tmp, buflen, "%d",mfd->ovalue[i]);
            strcat (buf,tmp);
        }
        if (i > 0) {
            _snprintf(tmp, buflen, ", %d",mfd->ovalue[i]);
            strcat (buf,tmp);
        }
    }
    strcat (buf, ")");
    return true;
}
