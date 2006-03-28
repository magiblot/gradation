/*
    Gradation Filter v1.24.2 for VirtualDub -- adjusts the
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
    wc.hCursor      = LoadCursor(NULL, IDC_CROSS);
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
    SPACE_RGB               = 0,
    SPACE_YUV               = 1,
    SPACE_CMYK              = 2,
    SPACE_HSV               = 3,
};

static char *space_names[]={
    "RGB",
    "YUV",
    "CMYK",
    "HSV",
};

enum {
    CHANNEL_RGB             = 0,
    CHANNEL_RED             = 1,
    CHANNEL_GREEN           = 2,
    CHANNEL_BLUE            = 3,
};
static char *RGBchannel_names[]={
    "RGB",
    "Red",
    "Green",
    "Blue",
};

enum {
    CHANNEL_Y               = 1,
    CHANNEL_U               = 2,
    CHANNEL_V               = 3,
};
static char *YUVchannel_names[]={
    "Luminance",
    "ChromaB",
    "ChromaR",
};

enum {
    CHANNEL_CYAN            = 1,
    CHANNEL_MAGENTA         = 2,
    CHANNEL_YELLOW          = 3,
    CHANNEL_BLACK           = 4,
};
static char *CMYKchannel_names[]={
    "Cyan",
    "Magenta",
    "Yellow",
    "Black",
};

enum {
    CHANNEL_HUE             = 1,
    CHANNEL_SATURATION      = 2,
    CHANNEL_VALUE           = 3,
};
static char *HSVchannel_names[]={
    "Hue",
    "Saturation",
    "Value",
};

enum {
    PROCESS_RGB     = 0,
    PROCESS_FULL    = 1,
    PROCESS_RGBW    = 2,
    PROCESS_FULLW   = 3,
    PROCESS_OFF     = 4,
    PROCESS_YUV     = 5,
    PROCESS_CMYK    = 6,
    PROCESS_HSV     = 7,

};

static char *process_names[]={
    "RGB only",
    "RGB + R/G/B",
    "RGB weighted",
    "RGB weighted + R/G/B",
    "off",
    "Y/U/V",
    "C/M/Y/K",
    "H/S/V",
};

typedef struct MyFilterData {
    IFilterPreview *ifp;
    Pixel32 *evaluer;
    Pixel32 *evalueg;
    Pixel32 *evalueb;
    Pixel32 *cvaluer;
    Pixel32 *cvalueg;
    Pixel32 *cvalueb;
    int ovalue[5][256];
    int value;
    int space_mode;
    int channel_mode;
    int process;
    int xl;
    int yl;
    int offset;
    char filename[1024];
    int filter;

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
    "Adjusts the gradation curves. The curves can be used for coring and invert as well. Version 1.24.2",
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

void GrdDrawGradTable(HWND hWnd, int table[]);
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
        mfd->cvaluer[i] = (mfd->ovalue[1][i] - i)<<16;
        mfd->cvalueg[i] = (mfd->ovalue[2][i] - i)<<8;
        mfd->cvalueb[i] = (mfd->ovalue[3][i] - i);
        mfd->evaluer[i] = (mfd->ovalue[0][i] - i)<<16;
        mfd->evalueg[i] = (mfd->ovalue[0][i] - i)<<8;
        mfd->evalueb[i] = (mfd->ovalue[0][i] - i);
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
    const Pixel32 *cvaluer = mfd->cvaluer;
    const Pixel32 *cvalueg = mfd->cvalueg;
    const Pixel32 *cvalueb = mfd->cvalueb;
    long r;
    int g;
    int b;
    int bw;
    int cmin;
    int cdelta;
    int rd;
    int gd;
    int bd;
    int ch;
    int chi;
    int v;
    int div;
    int dif;
    long x;
    long y;
    long z;
    long rr;
    long gg;
    long bb;
    int r2;
    int g2;
    int b2;

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
                bw = int((77 * (r >> 16) + 150 * (g >> 8) + 29 * b)>>8);
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
                bw = int((77 * (r >> 16) + 150 * (g >> 8) + 29 * b)>>8);
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
    case 5: //YUV
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                r = ((old_pixel & 0xFF0000)>>16);
                g = ((old_pixel & 0x00FF00)>>8);
                b = (old_pixel & 0x0000FF);
                //RGB to YUV (x=Y y=U z=V)
                x = (19595 * r + 38470 * g + 7471 * b);
                if ((x & 0x00FFFF)>32767) x=x+32768;
                x = (x & 0xFF0000)>>16;
                y = (8388607 - 11058 * r - 21710 * g + 32768 * b);
                if ((y & 0x00FFFF)>32767) y=y+32768;
                y = (y & 0xFF0000)>>16;
                z = (8388607 + 32768 * r - 27439 * g - 5329 * b);
                if ((z & 0x00FFFF)>32767) z=z+32768;
                z = (z & 0xFF0000)>>16;
                // Applying the curves
                x = (mfd->ovalue[1][x])<<16;
                y = (mfd->ovalue[2][y])-128;
                z = (mfd->ovalue[3][z])-128;
                // YUV to RGB
                rr = (x + 91881 * z);
                if (rr<0) rr=0; else if (rr>16744447) rr=16744447;
                if ((rr & 0x00FFFF)>32767) rr=rr+32768;
                r = (rr & 0xFF0000);
                gg = (x - 22553 * y - 46802 * z);
                if (gg<0) gg=0; else if (gg>16744447) gg=16744447;
                if ((gg & 0x00FFFF)>32767) gg=gg+32768;
                g = (gg & 0xFF0000)>>8;
                bb = (x + 116130 * y);
                if (bb<0) bb=0; else if (bb>16744447) bb=16744447;
                if ((bb & 0x00FFFF)>32767) bb=bb+32768;
                b = (bb & 0xFF0000)>>16;
                new_pixel = (r+g+b);
                *dst++ = new_pixel;
            }
            src = (Pixel32 *)((char *)src + fa->src.modulo);
            dst = (Pixel32 *)((char *)dst + fa->dst.modulo);
        }
    break;
    case 6: //CMYK
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                rr = (255-((old_pixel & 0xFF0000)>>16));
                gg = (255-((old_pixel & 0x00FF00)>>8));
                bb = (255-(old_pixel & 0x0000FF));
                v = rr;
                if (gg<v) v = gg;
                if (bb<v) v = bb;
                div= (256-v);
                x = ((rr - v)<<8);
                y = ((gg - v)<<8);
                z = ((bb - v)<<8);
                dif = (x-(x/div)*div);
                if (dif > ((div>>1)-1)) x=x+(div>>1);
                x = x/div;
                dif = (y-(y/div)*div);
                if (dif > ((div>>1)-1)) y=y+(div>>1);
                y = y/div;
                dif = (z-(z/div)*div);
                if (dif > ((div>>1)-1)) z=z+(div>>1);
                z = z/div;
                    x = mfd->ovalue[1][x];
                    y = mfd->ovalue[2][y];
                    z = mfd->ovalue[3][z];
                    v = mfd->ovalue[4][v];
                r2=(x * (256 - v));
                if ((r2 &0x0000FF)>127) r2=r2+128;
                r = 255 - (r2/256 + v);
                if (r<0) r=0;
                g2=(y * (256 - v));
                if ((g2 & 0x0000FF)>127) g2=g2+128;
                g = 255 - (g2/256 + v);
                if (g<0) g=0;
                b2=(z * (256 - v));
                if ((b2 & 0x0000FF)>127) b2=b2+128;
                b = 255 - (b2/256 + v);
                if (b<0) b=0;
                new_pixel = ((r<<16)+(g<<8)+b);
                *dst++ = new_pixel;
            }
            src = (Pixel32 *)((char *)src + fa->src.modulo);
            dst = (Pixel32 *)((char *)dst + fa->dst.modulo);
        }
    break;
    case 7: //HSV
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                r = ((old_pixel & 0xFF0000)>>16);
                g = ((old_pixel & 0x00FF00)>>8);
                b = (old_pixel & 0x0000FF);
                //RGB to HSV (x=H y=S z=V)
                cmin = min(r,g);
                cmin = min(b,cmin);
                z = max(r,g);
                z = max(b,z);
                cdelta = z - cmin;
                if( cdelta != 0 )
                {
                    y = (cdelta*255)/z;
                    cdelta = (cdelta*6);
                    if(r==z) {x = ((g-b)<<16)/cdelta;}
                    else if(g==z) {x = 21845+(((b-r)<<16)/cdelta);}
                    else {x = 43691+(((r-g)<<16)/cdelta);}
                    if( x < 0 ) {x = x + 65535;}
                    if ((x&0xFF)>127) x=x+128;
                    x=x>>8;
                }
                else
                {
                    y = 0;
                    x = 0;
                }
                // Applying the curves
                x = mfd->ovalue[1][x];
                y = mfd->ovalue[2][y];
                z = mfd->ovalue[3][z];
                // HSV to RGB
                if (y==0)
                {
                    r = z;
                    g = z;
                    b = z;
                }
                else
                {
                    chi=((x*6)&0xFF00)>>8;
                    ch=(x*6-(chi<<8));
                    rd=(z*(255-y));
                    if ((rd&0x00FF)>127) rd=rd+128;
                    rd = (rd&0xFF00)>>8;
                    gd = (z*(255-((y*ch)>>8)));
                    if ((gd&0x00FF)>127) gd=gd+128;
                    gd = (gd&0xFF00)>>8;
                    bd = (z*(255-(y*(256-ch)>>8)));
                    if ((bd&0x00FF)>127) bd=bd+128;
                    bd = (bd&0xFF00)>>8;
                    if (chi==0)      {r=z; g=bd; b=rd;}
                    else if (chi==1) {r=gd; g=z; b=rd;}
                    else if (chi==2) {r=rd; g=z; b=bd;}
                    else if (chi==3) {r=rd; g=gd; b=z;}
                    else if (chi==4) {r=bd; g=rd; b=z;}
                    else             {r=z; g=rd; b=gd;}
                }
                new_pixel = ((r<<16)+(g<<8)+b);
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

void DeinitProc(FilterActivation *fa, const FilterFunctions *ff) {
    UnregisterClass("FiWndProc", g_hInst);
}

int InitProc(FilterActivation *fa, const FilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;

    mfd->value = 0;
    mfd->process = 0;
    mfd->xl = 300;
    mfd->yl = 300;
    mfd->offset = 0;
    for (i=0; i<256; i++) {
        mfd->ovalue[0][i] = i;
        mfd->ovalue[1][i] = i;
        mfd->ovalue[2][i] = i;
        mfd->ovalue[3][i] = i;
        mfd->ovalue[4][i] = i;
    }

    return 0;
}

BOOL CALLBACK ConfigDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    MyFilterData *mfd = (MyFilterData *)GetWindowLong(hdlg, DWL_USER);
    int i;
    int r;
    signed int inv[256];
    int cx;
    int cy;
    int ax;
    int ay;
    int inc;
    signed int delta[256];
    signed int line[256];
    int a;
    signed int b;

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
            hWnd = GetDlgItem(hdlg, IDC_SPACE);
            for(i=0; i<4; i++)
            {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)space_names[i]);}
            if (mfd->process > 4) mfd->space_mode = mfd->process - 4;
            SendMessage(hWnd, CB_SETCURSEL, mfd->space_mode, 0);
            hWnd = GetDlgItem(hdlg, IDC_CHANNEL);
            switch (mfd->space_mode){
                case 0:
                    for(i=0; i<4; i++)
                    {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)RGBchannel_names[i]);}
                    mfd->channel_mode = 0;
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
                break;
                case 1:
                    for(i=0; i<3; i++)
                    {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)YUVchannel_names[i]);}
                    mfd->channel_mode = 0;
                    mfd->offset = 1;
                    hWnd = GetDlgItem(hdlg, IDC_RGB);
                    SetWindowText (hWnd,"Y/U/V");
                break;
                case 2:
                    for(i=0; i<4; i++)
                    {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)CMYKchannel_names[i]);}
                    mfd->channel_mode = 0;
                    mfd->offset = 1;
                    hWnd = GetDlgItem(hdlg, IDC_RGB);
                    SetWindowText (hWnd,"C/M/Y/K");
                break;
                case 3:
                    for(i=0; i<3; i++)
                    {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)HSVchannel_names[i]);}
                    mfd->channel_mode = 0;
                    mfd->offset = 1;
                    hWnd = GetDlgItem(hdlg, IDC_RGB);
                    SetWindowText (hWnd,"H/S/V");
                break;
            }
            if (mfd->space_mode != 0) {
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
            mfd->channel_mode = SendDlgItemMessage(hdlg, IDC_CHANNEL, CB_GETCURSEL, 0, 0) + mfd->offset;
            switch (mfd->process)
            {
                case PROCESS_RGB: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); break;
                case PROCESS_FULL: CheckDlgButton(hdlg, IDC_FULL,BST_CHECKED); break;
                case PROCESS_RGBW: CheckDlgButton(hdlg, IDC_RGBW,BST_CHECKED); break;
                case PROCESS_FULLW: CheckDlgButton(hdlg, IDC_FULLW,BST_CHECKED); break;
                case PROCESS_OFF:
                    if (mfd->space_mode != 0) CheckDlgButton(hdlg, IDC_FULL,BST_CHECKED);
                    else CheckDlgButton(hdlg, IDC_OFF,BST_CHECKED); break;
                case PROCESS_YUV: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); mfd->space_mode = 1; break;
                case PROCESS_CMYK: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); mfd->space_mode = 2; break;
                case PROCESS_HSV: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); mfd->space_mode = 3; break;
            }
            mfd->ifp->InitButton(GetDlgItem(hdlg, IDPREVIEW));

            return TRUE;

        case WM_PAINT:
            {
                PAINTSTRUCT ps;

                BeginPaint(hdlg, &ps);
                EndPaint(hdlg, &ps);
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)]);
                GrdDrawHBorder(GetDlgItem(hdlg, IDC_HBORDER), mfd);
                GrdDrawVBorder(GetDlgItem(hdlg, IDC_VBORDER), mfd);
            }
            return TRUE;

        case WM_LBUTTONDOWN:
            mfd->xl = 300;
            mfd->yl = 300;
            return 0;
        case WM_LBUTTONUP:
            mfd->xl = 300;
            mfd->yl = 300;
            return 0;
        case WM_MOUSEMOVE:
            if (mfd->xl != 300){mfd->xl = 300;}
            if (mfd->yl != 300){mfd->yl = 300;}
            return 0;
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
                        cy=(((ay-(mfd->yl))<<8)/(ax-(mfd->xl)));
                        for (cx=((mfd->xl)+1);cx<ax;cx++)
                        {
                            mfd->ovalue[mfd->channel_mode][cx]=((mfd->ovalue[mfd->channel_mode][mfd->xl])+(((cx-(mfd->xl))*cy)>>8));
                        }
                    }
                    else
                    {
                        cy=(((mfd->yl)-ay)<<8)/(ax-(mfd->xl));
                        for (cx=((mfd->xl)+1);cx<ax;cx++)
                        {
                            mfd->ovalue[mfd->channel_mode][cx]=((mfd->ovalue[mfd->channel_mode][mfd->xl])-(((cx-(mfd->xl))*cy)>>8));
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
                        }
                    }
                    else
                    {
                        cy=(((mfd->yl)-ay)<<8)/((mfd->xl)-ax);
                        for (cx=((mfd->xl)-1);cx>ax;cx--)
                        {
                            mfd->ovalue[mfd->channel_mode][cx]=((mfd->ovalue[mfd->channel_mode][mfd->xl])-((((mfd->xl)-cx)*cy)>>8));
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
            GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)]);
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
                ofn.lpstrFilter = "Map Settings (*.amp)\0*.amp\0Comma Separated Values (*.csv)\0*.csv\0Tone Curve File (*.crv)\0*.crv\0Tone Map File (*.map)\0*.map\0All Files\0*.*\0\0";
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
                    ImportCurve (hdlg, mfd);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)]);
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
                ofn.lpstrFilter = "Map Settings (*.amp)\0*.amp\0Comma Separated Values (*.csv)\0*.csv\0All Files\0*.*\0\0";
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
                    ExportCurve (hdlg, mfd);
                }
                break;
                }
                return TRUE;
            case IDC_RGB:
                switch (mfd->space_mode){
                    case 0:
                    mfd->process = PROCESS_RGB;
                    mfd->ifp->RedoSystem();
                    break;
                    case 1:
                    mfd->process = PROCESS_YUV;
                    mfd->ifp->RedoSystem();
                    break;
                    case 2:
                    mfd->process = PROCESS_CMYK;
                    mfd->ifp->RedoSystem();
                    break;
                    case 3:
                    mfd->process = PROCESS_HSV;
                    mfd->ifp->RedoSystem();
                    break;
                }
            break;
            case IDC_FULL:
                switch (mfd->space_mode){
                    case 0:
                    mfd->process = PROCESS_FULL;
                    mfd->ifp->RedoSystem();
                    break;
                    case 1:
                    mfd->process = PROCESS_OFF;
                    mfd->ifp->RedoSystem();
                    break;
                    case 2:
                    mfd->process = PROCESS_OFF;
                    mfd->ifp->RedoSystem();
                    break;
                    case 3:
                    mfd->process = PROCESS_OFF;
                    mfd->ifp->RedoSystem();
                    break;
                }
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
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)]);
                mfd->ifp->RedoSystem();
                break;
            case IDC_OUTPUTMINUS:
                if (mfd->ovalue[mfd->channel_mode][mfd->value] > 0)
                {
                    mfd->ovalue[mfd->channel_mode][mfd->value]--;
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                }
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)]);
                mfd->ifp->RedoSystem();
                break;
            case IDC_RESET:
                for (i=0; i<256; i++)
                {
                    mfd->ovalue[mfd->channel_mode][i] = i;
                }
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)]);
                mfd->ifp->RedoSystem();
                break;
            case IDC_SMOOTH:
                if (mfd->ovalue[mfd->channel_mode][0]<=mfd->ovalue[mfd->channel_mode][255])
                {
                    a = mfd->ovalue[mfd->channel_mode][0];
                    b = mfd->ovalue[mfd->channel_mode][255]-a;
                }
                else if (mfd->ovalue[mfd->channel_mode][0]>mfd->ovalue[mfd->channel_mode][255])
                {
                    a = mfd->ovalue[mfd->channel_mode][0];
                    b = -a+mfd->ovalue[mfd->channel_mode][255];
                }
                for (i=0; i<256; i++)
                {
                    delta[i] = mfd->ovalue[mfd->channel_mode][i]-(((i*b)/255)+a);
                }
                for (i=0; i<255; i++)
                {
                    if (i < 2)
                    {
                        if (i==0)
                        {
                            inv[i]=(delta[i]*25)/25;
                        }
                        else
                        {
                            inv[i]=(delta[i]*25+delta[i-1]*50)/75;
                        }
                    }
                    else
                    {
                        inv[i]=(delta[i]*25+delta[i-1]*50+delta[i-2]*25)/100;
                    }
                }
                for (i=0; i<255; i++)
                {
                    delta[i]=inv[i];
                }
                for (i=255; i>0; i--)
                {
                    if (i > 253)
                    {
                        if (i==255)
                        {
                            inv[i]=(delta[i]*25)/25;
                        }
                        else
                        {
                            inv[i]=(delta[i]*25+delta[i+1]*50)/75;
                        }
                    }
                    else
                    {
                        inv[i]=(delta[i]*25+delta[i+1]*50+delta[i+2]*25)/100;
                    }
                }
                for (i=255; i>0; i--)
                {
                    delta[i]=inv[i];
                }
                for (i=0; i<256; i++)
                {
                    mfd->ovalue[mfd->channel_mode][i] = delta[i]+((i*b)/255)+a;
                }
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)]);
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
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)]);
                mfd->ifp->RedoSystem();
                break;
            case IDC_SPACE:
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    mfd->space_mode = SendDlgItemMessage(hdlg, IDC_SPACE, CB_GETCURSEL, 0, 0);
                    hWnd = GetDlgItem(hdlg, IDC_CHANNEL);
                    SendMessage(hWnd, CB_RESETCONTENT, 0, 0);
                    switch (mfd->space_mode){
                    case 0:
                        for(i=0; i<4; i++)
                        {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)RGBchannel_names[i]);}
                        mfd->channel_mode = 0;
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
                    case 1:
                        for(i=0; i<3; i++)
                        {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)YUVchannel_names[i]);}
                        mfd->channel_mode = 0;
                        mfd->offset = 1;
                        hWnd = GetDlgItem(hdlg, IDC_RGB);
                        SetWindowText (hWnd,"Y/U/V");
                        if (mfd->process != PROCESS_OFF) mfd->process = PROCESS_YUV;
                        break;
                    case 2:
                        for(i=0; i<4; i++)
                        {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)CMYKchannel_names[i]);}
                        mfd->channel_mode = 0;
                        mfd->offset = 1;
                        hWnd = GetDlgItem(hdlg, IDC_RGB);
                        SetWindowText (hWnd,"C/M/Y/K");
                        if (mfd->process != PROCESS_OFF) mfd->process = PROCESS_CMYK;
                        break;
                    case 3:
                        for(i=0; i<3; i++)
                        {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)HSVchannel_names[i]);}
                        mfd->channel_mode = 0;
                        mfd->offset = 1;
                        hWnd = GetDlgItem(hdlg, IDC_RGB);
                        SetWindowText (hWnd,"H/S/V");
                        if (mfd->process != PROCESS_OFF) mfd->process = PROCESS_HSV;
                        break;
                    }
                    if (mfd->space_mode != 0) {
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
                    mfd->channel_mode = SendDlgItemMessage(hdlg, IDC_CHANNEL, CB_GETCURSEL, 0, 0) + mfd->offset;
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)]);
                    GrdDrawHBorder(GetDlgItem(hdlg, IDC_HBORDER), mfd);
                    GrdDrawVBorder(GetDlgItem(hdlg, IDC_VBORDER), mfd);
                    mfd->ifp->RedoSystem();
                }
                return TRUE;
            case IDC_CHANNEL:
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    mfd->channel_mode = SendDlgItemMessage(hdlg, IDC_CHANNEL, CB_GETCURSEL, 0, 0) + mfd->offset;
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][mfd->value], FALSE);
                    GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)]);
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
    int j;
    int t;
    const char *tmp;

    mfd->process = argv[0].asInt();

    tmp = *argv[1].asString();
    for (j=0; j<5; j++) {
        for (i=(j*256); i<((j+1)*256); i++) {
            sscanf(tmp + 2*i,"%02x", &t);
            mfd->ovalue[j][i-(j*256)] = t;
        }
    }
}

bool FssProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;
    int j;
    char *tmp;
    tmp = "";

    _snprintf(buf, buflen, "Config(%d,\"",mfd->process);

    for (j=0; j<5; j++) {
        for (i=0; i<256; i++) {
                _snprintf(tmp, buflen, "%02x",mfd->ovalue[j][i]);
                strcat (buf,tmp);
        }
    }
    strcat (buf, "\")");
    return true;
}

void GrdDrawGradTable(HWND hWnd, int table[])
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
    int rd;
    int gd;
    int bd;
    int ch;
    int chi;
    int dif;
    HDC hdc;
    HPEN hPen;

    hdc = GetDC(hWnd);

    scaleX = (double)(rect.right - rect.left) / 257.0;
    Y = (int)(rect.bottom - rect.top);
    if ((Y-(Y/3)*3) > 1) dif = (Y+1)/3;
    else dif = Y/3;

    SelectObject(hdc, CreatePen(PS_SOLID, 1, RGB(0, 0, 0)));

    for (j = 1; j <= Y; j++)
    {
        MoveToEx(hdc, rect.left, rect.bottom - j, NULL);
        LineTo(hdc, rect.left, rect.bottom  - j);
        for(i = 0; i < 256; i++)
        {
            switch(mfd->space_mode)
            {
            case 0:
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
            break;
            case 1:
                switch(mfd->channel_mode)
                {
                case 1:
                    r = i;
                    g = i;
                    b = i;
                break;
                case 2:
                    r = 0;
                    g = ((0 - 88 * (i-128))/256);
                    if (g<0) g=0; else if (g>255) g=255;
                    b = ((0 + 451 * (i-128))/256);
                    if (b<0) b=0; else if (b>255) b=255;
                break;
                case 3:
                    r = ((359 * (i-128))/256);
                    if (r<0) r=0;   if (r>255) r=255;
                    g = ((0 - 182 * (i-128))/256);
                    if (g<0) g=0; else if (g>255) g=255;
                    b = 0;
                break;
                }
            break;
            case 2:
                switch(mfd->channel_mode)
                {
                case 1:
                    r = 255 - i;
                    g = 255;
                    b = 255;
                break;
                case 2:
                    r = 255;
                    g = 255 - i;
                    b = 255;
                break;
                case 3:
                    r = 255;
                    g = 255;
                    b = 255 - i;
                break;
                case 4:
                    r = 255 - i;
                    g = 255 - i;
                    b = 255 - i;
                break;
                }
            break;
            case 3:
                switch(mfd->channel_mode)
                {
                case 1:
                    chi=((i*6)&0xFF00)>>8;
                    ch=(i*6-(chi<<8));
                    rd=(255)/256;
                    gd=(255*(256-((255*ch)/256)))/256;
                    bd=(255*(256-(255*(256-ch)/256)))/256;
                    if (chi==0)      {r=255; g=bd; b=rd;}
                    else if (chi==1) {r=gd; g=255; b=rd;}
                    else if (chi==2) {r=rd; g=255; b=bd;}
                    else if (chi==3) {r=rd; g=gd; b=255;}
                    else if (chi==4) {r=bd; g=rd; b=255;}
                    else             {r=255; g=rd; b=gd;}
                break;
                case 2:
                    chi=((((j-1)/dif)*512)&0xFF00)>>8;
                    ch=(((j-1)/dif)*512-(chi<<8));
                    rd=(255*(256-i))/256;
                    gd=(255*(256-((i*ch)/256)))/256;
                    if (gd<0) gd=0;
                    if (gd>255) gd= 255;
                    bd=(255*(256-(i*(256-ch)/256)))/256;
                    if (bd<0) bd=0;
                    if (bd>255) bd= 255;
                    if (chi==0)      {r=255; g=bd; b=rd;}
                    else if (chi==1) {r=gd; g=255; b=rd;}
                    else if (chi==2) {r=rd; g=255; b=bd;}
                    else if (chi==3) {r=rd; g=gd; b=255;}
                    else if (chi==4) {r=bd; g=rd; b=255;}
                    else             {r=255; g=rd; b=gd;}
                break;
                case 3:
                    r = i;
                    g = i;
                    b = i;
                break;
                }
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
    int rd;
    int gd;
    int bd;
    int ch;
    int chi;
    int dif;
    HDC hdc;
    HPEN hPen;

    hdc = GetDC(hWnd);

    X = (int)(rect.right - rect.left);
    scaleY = (double)(rect.bottom - rect.top) / 256.0;
    if ((X-(X/3)*3) > 1) dif = (X+1)/3;
    else dif = X/3;

    SelectObject(hdc, CreatePen(PS_SOLID, 1, RGB(0, 0, 0)));

    for (j = 0; j < X; j++)
    {
        MoveToEx(hdc, rect.left + j, rect.bottom - 1, NULL);
        LineTo(hdc, rect.left + j, rect.bottom - 1);
        for(i = 0; i < 256; i++)
        {
            switch(mfd->space_mode)
            {
            case 0:
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
            break;
            case 1:
                switch(mfd->channel_mode)
                {
                case 1:
                    r = i;
                    g = i;
                    b = i;
                break;
                case 2:
                    r = 0;
                    g = ((0 - 88 * (i-128))/256);
                    if (g<0) g=0; else if (g>255) g=255;
                    b = ((0 + 451 * (i-128))/256);
                    if (b<0) b=0; else if (b>255) b=255;
                break;
                case 3:
                    r = ((359 * (i-128))/256);
                    if (r<0) r=0;   if (r>255) r=255;
                    g = ((0 - 182 * (i-128))/256);
                    if (g<0) g=0; else if (g>255) g=255;
                    b = 0;
                break;
                }
            break;
            case 2:
                switch(mfd->channel_mode)
                {
                case 1:
                    r = 255 - i;
                    g = 255;
                    b = 255;
                break;
                case 2:
                    r = 255;
                    g = 255 - i;
                    b = 255;
                break;
                case 3:
                    r = 255;
                    g = 255;
                    b = 255 - i;
                break;
                case 4:
                    r = 255 - i;
                    g = 255 - i;
                    b = 255 - i;
                break;
                }
            break;
            case 3:
                switch(mfd->channel_mode)
                {
                case 1:
                    chi=((i*6)&0xFF00)>>8;
                    ch=(i*6-(chi<<8));
                    rd=(255)/256;
                    gd=(255*(256-((255*ch)/256)))/256;
                    bd=(255*(256-(255*(256-ch)/256)))/256;
                    if (chi==0)      {r=255; g=bd; b=rd;}
                    else if (chi==1) {r=gd; g=255; b=rd;}
                    else if (chi==2) {r=rd; g=255; b=bd;}
                    else if (chi==3) {r=rd; g=gd; b=255;}
                    else if (chi==4) {r=bd; g=rd; b=255;}
                    else             {r=255; g=rd; b=gd;}
                break;
                case 2:
                    chi=(((j/dif)*512)&0xFF00)>>8;
                    ch=((j/dif)*512-(chi<<8));
                    rd=(255*(256-i))/256;
                    gd=(255*(256-((i*ch)/256)))/256;
                    if (gd<0) gd=0;
                    if (gd>255) gd= 255;
                    bd=(255*(256-(i*(256-ch)/256)))/256;
                    if (bd<0) bd=0;
                    if (bd>255) bd= 255;
                    if (chi==0)      {r=255; g=bd; b=rd;}
                    else if (chi==1) {r=gd; g=255; b=rd;}
                    else if (chi==2) {r=rd; g=255; b=bd;}
                    else if (chi==3) {r=rd; g=gd; b=255;}
                    else if (chi==4) {r=bd; g=rd; b=255;}
                    else             {r=255; g=rd; b=gd;}
                break;
                case 3:
                    r = i;
                    g = i;
                    b = i;
                break;
                }
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
    int stor[1280];
    int temp[1280];
    long lSize;
    int start;
    int mark;
    int cv;
    int count;
    int csta;
    int offset;

    mark = 0;
    start = 10000;
    csta = 10000;
    count = 0;
    offset = 0;

    if (mfd->filter == 2)
    {
        pFile = fopen (mfd->filename, "r");
        if (pFile==NULL)
        {
            MessageBox (NULL, TEXT ("Error"), TEXT ("Error opening file"),0);
        }
        else
        {
            fseek (pFile , 0 , SEEK_END);
            lSize = ftell (pFile);
            rewind (pFile);
            for(i=0; (i < 1280) && ( feof(pFile) == 0 ); i++ )
            {
                fscanf (pFile, "%02X", &stor[i]);
            }
            fclose (pFile);
            lSize = lSize/4;
        }
    }
    else if (mfd->filter == 3 || mfd->filter == 4)
    {
        pFile = fopen (mfd->filename, "rb");
        if (pFile==NULL)
        {
            MessageBox (NULL, TEXT ("Error"), TEXT ("Error opening file"),0);
        }
        else
        {
            fseek (pFile , 0 , SEEK_END);
            lSize = ftell (pFile);
            rewind (pFile);
            for(i=0; (i <= lSize) && ( feof(pFile) == 0 ); i++ )
            {
                cv = fgetc(pFile);
                if (mark == 5)
                {
                    if (i>=start)
                    {
                        if (offset+count < 1280)
                        {
                            stor[offset+count] = cv;
                        }
                        count ++;
                        if (count > 255)
                        {
                            count = 0;
                            offset = offset + 256;
                            mark = 0;
                        }

                    }
                }
                if (mark == 4)
                {
                    if (i == csta)
                    {
                        start = i+2*cv+1;
                        mark = 5;
                    }
                }
                if (mark == 3)
                {
                    if (cv == 0)
                    {
                        mark = 4;
                        csta = i+1;
                    }
                    else
                    {
                        mark = 0;
                    }
                }
                if (mark == 2)
                {
                    if (cv == 0)
                    {
                        mark = 3;
                    }
                    else
                    {
                        mark = 0;
                    }
                }
                if (mark == 1)
                {
                    if (cv == 0)
                    {
                        mark = 2;
                    }
                    else
                    {
                        mark = 0;
                    }
                }
                if (mark == 0)
                {
                    if (cv == 100)
                    {
                        mark = 1;
                    }
                }
            }
            fclose (pFile);
        }
        if (mfd->filter == 4)
        {
            for(i=0; (i < 1280);i++)
            {
                if (i<1024)
                {
                    temp[i+256]=stor[i];
                }
                else
                {
                    temp[i-1024]=stor[i];
                }
            }
            for(i=0; (i < 1280);i++)
            {
                stor[i]=temp[i];
            }
        }
    }
    else
    {
        pFile = fopen (mfd->filename, "rb");
        if (pFile==NULL)
        {
            MessageBox (NULL, TEXT ("Error"), TEXT ("Error opening file"),0);
        }
        else
        {
            fseek (pFile , 0 , SEEK_END);
            lSize = ftell (pFile);
            rewind (pFile);
            for(i=0; (i < 1280) && ( feof(pFile) == 0 ); i++ )
            {
                stor[i] = fgetc(pFile);
            }
            fclose (pFile);
        }
    }

    if (lSize > 768)
    {
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
        for(i=1024; i < 1280; i++) {
            mfd->ovalue[4][(i-1024)] = stor[i];
        }
    }
    if (lSize < 769 && lSize > 256)
    {
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
    if (lSize < 257 && lSize > 0)
    {
        for(i=0; i < 256; i++) {
            mfd->ovalue[0][i] = stor[i];
        }
    }
}
void ExportCurve(HWND hWnd, MyFilterData *mfd)
{
    char str [80];
    FILE *pFile;
    int i;
    int j;
    char c;

    if (mfd->filter == 2){
        pFile = fopen (mfd->filename,"w");
        for (j=0; j<5;j++) {
            for (i=0; i<256; i++) {
                fprintf (pFile, "%02X\n",(mfd->ovalue[j][i]));
            }
        }
    }
    else {
        pFile = fopen (mfd->filename,"wb");
        for (j=0; j<5;j++) {
            for (i=0; i<256; i++) {
                c = char (mfd->ovalue[j][i]);
                fprintf (pFile, "%c",c);
            }
        }
    }
    fclose (pFile);
}
