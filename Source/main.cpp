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

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <math.h>

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
long *rgblab; //LUT Lab
long *labrgb; //LUT Lab

HINSTANCE hInst;

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

LRESULT CALLBACK FiWndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){ //curve box
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

enum {
    SPACE_RGB               = 0,
    SPACE_YUV               = 1,
    SPACE_CMYK              = 2,
    SPACE_HSV               = 3,
    SPACE_LAB               = 4,
};

static char *space_names[]={
    "RGB",
    "YUV",
    "CMYK",
    "HSV",
    "Lab",
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
    CHANNEL_L               = 1,
    CHANNEL_A               = 2,
    CHANNEL_B               = 3,
};
static char *LABchannel_names[]={
    "Luminance",
    "a Red-Green",
    "b Yellow-Blue",
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
    PROCESS_LAB     = 8,
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
    "L/a/b",
};

typedef struct MyFilterData {
    IFilterPreview *ifp;
    long rvalue[3][256];
    int gvalue[3][256];
    int bvalue[256];
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
    bool Labprecalc;
    int laboff;
    int drwmode[5];
    int drwpoint[5][16][2];
    int poic[5];
    int cp;
    bool psel;
    double scalex;
    double scaley;
    unsigned int boxx;
    int boxy;
    TCHAR gamma[10];
} MyFilterData;

ScriptFunctionDef func_defs[]={
    { (ScriptFunctionPtr)ScriptConfig, "Config", "0is" },
    { (ScriptFunctionPtr)ScriptConfig, NULL,     "0iss" },
    { NULL },
};

CScriptObject script_obj={
    NULL, func_defs
};

struct FilterDefinition filterDef = {

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

    vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
    vdfd_compat = VIRTUALDUB_FILTERDEF_COMPATIBLE;

    return 0;
}

void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(FilterModule *fm, const FilterFunctions *ff) {
    ff->removeFilter(fd);
}

///////////////////////////////////////////////////////////////////////////

void PreCalcLut();
void CalcCurve(MyFilterData *mfd);
void GrdDrawGradTable(HWND hWnd, int table[], int laboff, int dmode, int dp[16][2], int pc, int ap);
void GrdDrawBorder(HWND hWnd, HWND hWnd2, MyFilterData *mfd);
void ImportCurve(MyFilterData *mfd);
void ExportCurve(MyFilterData *mfd);

///////////////////////////////////////////////////////////////////////////

int StartProc(FilterActivation *fa, const FilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    if (mfd->Labprecalc==0 && mfd->process==8) { // build up the LUT for the Lab process if it is not precalculated already
        PreCalcLut();
        mfd->Labprecalc = 1;}
    return 0;
}

int RunProc(const FilterActivation *fa, const FilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    PixDim w, h;
    Pixel32 *src, *dst;
    const PixDim width = fa->src.w;
    const PixDim height = fa->src.h;

    long r;
    int g;
    int b;
    int bw;
    int cmin;
    int cdelta;
    int cdeltah;
    int ch;
    int chi;
    int div;
    int divh;
    int v;
    long x;
    long y;
    long z;
    long rr;
    long gg;
    long bb;
    long lab;

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
                new_pixel = mfd->rvalue[0][(old_pixel & 0xFF0000)>>16] + mfd->gvalue[0][(old_pixel & 0x00FF00)>>8] + mfd->ovalue[0][(old_pixel & 0x0000FF)];//((old_pixel & 0xFF0000) + evaluer[(old_pixel & 0xFF0000)>>16]) + ((old_pixel & 0x00FF00) + evalueg[(old_pixel & 0x00FF00)>>8]) + ((old_pixel & 0x0000FF) + evalueb[(old_pixel & 0x0000FF)]); //
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
                med_pixel = mfd->rvalue[1][(old_pixel & 0xFF0000)>>16] + mfd->gvalue[1][(old_pixel & 0x00FF00)>>8] + mfd->ovalue[3][(old_pixel & 0x0000FF)];//((old_pixel & 0xFF0000) + cvaluer[(old_pixel & 0xFF0000)>>16]) + ((old_pixel & 0x00FF00) + cvalueg[(old_pixel & 0x00FF00)>>8]) + ((old_pixel & 0x0000FF) + cvalueb[(old_pixel & 0x0000FF)]);
                new_pixel = mfd->rvalue[0][(med_pixel & 0xFF0000)>>16] + mfd->gvalue[0][(med_pixel & 0x00FF00)>>8] + mfd->ovalue[0][(med_pixel & 0x0000FF)];//((med_pixel & 0xFF0000) + evaluer[(med_pixel & 0xFF0000)>>16]) + ((med_pixel & 0x00FF00) + evalueg[(med_pixel & 0x00FF00)>>8]) + mfd->ovalue[0][(med_pixel & 0x0000FF)];//((med_pixel & 0x0000FF) + evalueb[(med_pixel & 0x0000FF)]);
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
                    r = r+mfd->rvalue[2][bw];
                    if (r<65536) r=0; else if (r>16711680) r=16711680;
                    g = g+mfd->gvalue[2][bw];
                    if (g<256) g=0; else if (g>65280) g=65280;
                    b = b+mfd->bvalue[bw];
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
                med_pixel = mfd->rvalue[1][(old_pixel & 0xFF0000)>>16] + mfd->gvalue[1][(old_pixel & 0x00FF00)>>8] + mfd->ovalue[3][(old_pixel & 0x0000FF)];//((old_pixel & 0xFF0000) + cvaluer[(old_pixel & 0xFF0000)>>16]) + ((old_pixel & 0x00FF00) + cvalueg[(old_pixel & 0x00FF00)>>8]) + ((old_pixel & 0x0000FF) + cvalueb[(old_pixel & 0x0000FF)]);
                r = (med_pixel & 0xFF0000);
                g = (med_pixel & 0x00FF00);
                b = (med_pixel & 0x0000FF);
                bw = int((77 * (r >> 16) + 150 * (g >> 8) + 29 * b)>>8);
                    r = r+mfd->rvalue[2][bw];
                    if (r<65536) r=0; else if (r>16711680) r=16711680;
                    g = g+mfd->gvalue[2][bw];
                    if (g<256) g=0; else if (g>65280) g=65280;
                    b = b+mfd->bvalue[bw];
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
                x = (32768 + 19595 * r + 38470 * g + 7471 * b)>>16; //correct rounding +32768
                y = (8421375 - 11058 * r - 21710 * g + 32768 * b)>>16; //correct rounding +32768
                z = (8421375 + 32768 * r - 27439 * g - 5329 * b)>>16; //correct rounding +32768
                // Applying the curves
                x = (mfd->ovalue[1][x])<<16;
                y = (mfd->ovalue[2][y])-128;
                z = (mfd->ovalue[3][z])-128;
                // YUV to RGB
                rr = (32768 + x + 91881 * z); //correct rounding +32768
                if (rr<0) {r=0;} else if (rr>16711680) {r=16711680;} else {r = (rr & 0xFF0000);}
                gg = (32768 + x - 22553 * y - 46802 * z); //correct rounding +32768
                if (gg<0) {g=0;} else if (gg>16711680) {g=65280;} else {g = (gg & 0xFF0000)>>8;}
                bb = (32768 + x + 116130 * y); //correct rounding +32768
                if (bb<0) {b=0;} else if (bb>16711680) {b=255;} else {b = bb>>16;}
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
                r = ((old_pixel & 0xFF0000)>>16);
                g = ((old_pixel & 0x00FF00)>>8);
                b = (old_pixel & 0x0000FF);
                if(r>=g && r>=b) { /* r is Maximum */
                    v = 255-r;
                    div  = r+1;
                    divh = div>>1;
                    x = 0;
                    y = (((r-g)<<8) + divh)/div;  //correct rounding  yy+(div>>1)
                    z = (((r-b)<<8) + divh)/div;} //correct rounding  zz+(div>>1)
                else if(g>=b) {/* g is maximum */
                    v = 255-g;
                    div  = g+1;
                    divh = div>>1;
                    x = (((g-r)<<8) + divh)/div;  //correct rounding  xx+(div>>1)
                    y = 0;
                    z = (((g-b)<<8) + divh)/div;} //correct rounding  zz+(div>>1)
                else {/* b is maximum */
                    v = 255-b;
                    div  = b+1;
                    divh = div>>1;
                    x = (((b-r)<<8) + divh)/div; //correct rounding  xx+(div>>1)
                    y = (((b-g)<<8) + divh)/div; //correct rounding  yy+(div>>1)
                    z = 0;}
                // Applying the curves
                x = mfd->ovalue[1][x];
                y = mfd->ovalue[2][y];
                z = mfd->ovalue[3][z];
                v = mfd->ovalue[4][v];
                // CMYK to RGB
                r = 255-((((x*(256-v))+128)>>8)+v); //correct rounding rr+128;
                if (r<0) r=0;
                g = 255-((((y*(256-v))+128)>>8)+v); //correct rounding gg+128;
                if (g<0) g=0;
                b = 255-((((z*(256-v))+128)>>8)+v); //correct rounding bb+128;
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
                {   y = (cdelta*255)/z;
                    cdelta = (cdelta*6);
                    cdeltah = cdelta>>1;
                    if(r==z) {x = (((g-b)<<16)+cdeltah)/cdelta;}
                    else if(g==z) {x = 21845+((((b-r)<<16)+cdeltah)/cdelta);}
                    else {x = 43689+((((r-g)<<16)+cdeltah)/cdelta);}
                    if(x<0) {x=(x+65577)>>8;}
                    else {x=(x+128)>>8;}
                }
                else
                {   y = 0;
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
                new_pixel = ((r<<16)+(g<<8)+b);
                *dst++ = new_pixel;
            }
            src = (Pixel32 *)((char *)src + fa->src.modulo);
            dst = (Pixel32 *)((char *)dst + fa->dst.modulo);
        }
    break;
    case 8: //LAB
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                lab = rgblab[(old_pixel & 0xFFFFFF)];
                rr = (lab & 0xFF0000)>>16;
                gg = (lab & 0x00FF00)>>8;
                bb = (lab & 0x0000FF);
                // Applying the curves
                x = mfd->ovalue[1][rr];
                y = mfd->ovalue[2][gg];
                z = mfd->ovalue[3][bb];
                //Lab to XYZ
                new_pixel = labrgb[((x<<16)+(y<<8)+z)];
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
    return 0;
}

void DeinitProc(FilterActivation *fa, const FilterFunctions *ff) {
    UnregisterClass("FiWndProc", g_hInst);
}

int InitProc(FilterActivation *fa, const FilterFunctions *ff) {
    MyFilterData *mfd = (MyFilterData *)fa->filter_data;
    int i;

    mfd->Labprecalc = 0;
    for (i=0; i<5; i++){
        mfd->drwmode[i]=2;
        mfd->poic[i]=2;
        mfd->drwpoint[i][0][0]=0;
        mfd->drwpoint[i][0][1]=0;
        mfd->drwpoint[i][1][0]=255;
        mfd->drwpoint[i][1][1]=255;}
    mfd->value = 0;
    mfd->process = 0;
    mfd->xl = 300;
    mfd->yl = 300;
    mfd->offset = 0;
    mfd->psel=false;
    mfd->cp=0;
    _snprintf(mfd->gamma, 10, "%.3lf",1.000);
    for (i=0; i<256; i++) {
        mfd->ovalue[0][i] = i;
        mfd->rvalue[0][i]=(mfd->ovalue[0][i]<<16);
        mfd->rvalue[2][i]=(mfd->ovalue[0][i]-i)<<16;
        mfd->gvalue[0][i]=(mfd->ovalue[0][i]<<8);
        mfd->gvalue[2][i]=(mfd->ovalue[0][i]-i)<<8;
        mfd->bvalue[i]=(mfd->ovalue[0][i]-i);
        mfd->ovalue[1][i] = i;
        mfd->rvalue[1][i]=(mfd->ovalue[1][i]<<16);
        mfd->ovalue[2][i] = i;
        mfd->gvalue[1][i]=(mfd->ovalue[2][i]<<8);
        mfd->ovalue[3][i] = i;
        mfd->ovalue[4][i] = i;
    }
    return 0;
}

BOOL CALLBACK ConfigDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    MyFilterData *mfd = (MyFilterData *)GetWindowLong(hdlg, DWL_USER);
    signed int inv[256];
    int invp[16][2];
    int cx;
    int cy;
    int ax;
    int ay;
    signed int delta[256];
    int a;
    int i;
    int j;
    int tmpx=0;
    int tmpy=0;
    bool stp;
    bool ptp;
    signed int b;
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
            SetWindowLong(hdlg, DWL_USER, lParam);
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
            if (mfd->process > 4) mfd->space_mode = mfd->process - 4;
            SendMessage(hWnd, CB_SETCURSEL, mfd->space_mode, 0);
            hWnd = GetDlgItem(hdlg, IDC_CHANNEL);
            switch (mfd->space_mode){
                case 0:
                    for(i=0; i<4; i++)
                    {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)RGBchannel_names[i]);}
                    mfd->channel_mode = 0;
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
                case 4:
                    for(i=0; i<3; i++)
                    {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)LABchannel_names[i]);}
                    mfd->channel_mode = 0;
                    mfd->offset = 1;
                    hWnd = GetDlgItem(hdlg, IDC_RGB);
                    SetWindowText (hWnd,"L/a/b");
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
            if (mfd->drwmode[mfd->channel_mode]!=0){
                SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                if (mfd->drwmode[mfd->channel_mode]==3){
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
                    if (mfd->space_mode != 0) CheckDlgButton(hdlg, IDC_FULL,BST_CHECKED);
                    else CheckDlgButton(hdlg, IDC_OFF,BST_CHECKED); break;
                case PROCESS_YUV: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); mfd->space_mode = 1; break;
                case PROCESS_CMYK: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); mfd->space_mode = 2; break;
                case PROCESS_HSV: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); mfd->space_mode = 3; break;
                case PROCESS_LAB: CheckDlgButton(hdlg, IDC_RGB,BST_CHECKED); mfd->space_mode = 4; break;
            }
            switch  (mfd->drwmode[mfd->channel_mode]) {
                case 0:
                    CheckDlgButton(hdlg, IDC_RADIOPM, BST_CHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                    break;
                case 1:
                    CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOLM, BST_CHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                    break;
                case 2:
                    CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOSM, BST_CHECKED);
                    CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                    break;
                case 3:
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
            {   if (mfd->Labprecalc==0 && mfd->process==8) { // build up the LUT for the Lab process if it is not precalculated already
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
            if (mfd->drwmode[mfd->channel_mode]==0){
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
                    if (mfd->drwmode[mfd->channel_mode]==3){
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
                CalcCurve(mfd);
                if (mfd->drwmode[mfd->channel_mode]==3){
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
            if (mfd->drwmode[mfd->channel_mode]==0){
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
                if (mfd->drwmode[mfd->channel_mode]!=3){ // add point
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
                            CalcCurve(mfd);
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
            if (mfd->drwmode[mfd->channel_mode]==0){
                mfd->value=ax;
                SetDlgItemInt(hdlg, IDC_VALUE, ax, FALSE);
                SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, mfd->ovalue[mfd->channel_mode][ax], FALSE);}
            else if ((mfd->drwmode[mfd->channel_mode]==1 || mfd->drwmode[mfd->channel_mode]==2) && mfd->poic[mfd->channel_mode]>2){ // delete point
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
                            CalcCurve(mfd);
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
                    ImportCurve (mfd);
                    if (mfd->drwmode[mfd->channel_mode]==0) {
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
                        if (mfd->drwmode[mfd->channel_mode]==3) {
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
                        case 0:
                            CheckDlgButton(hdlg, IDC_RADIOPM, BST_CHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                            break;
                        case 1:
                            CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOLM, BST_CHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                            break;
                        case 2:
                            CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOSM, BST_CHECKED);
                            CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                            break;
                        case 3:
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
                    ExportCurve (mfd);
                }
                break;
                }
                return TRUE;
            case IDC_RGB:
                switch (mfd->space_mode){
                    case 0:
                    mfd->process = PROCESS_RGB;
                    break;
                    case 1:
                    mfd->process = PROCESS_YUV;
                    break;
                    case 2:
                    mfd->process = PROCESS_CMYK;
                    break;
                    case 3:
                    mfd->process = PROCESS_HSV;
                    break;
                    case 4:
                    mfd->process = PROCESS_LAB;
                    break;
                }
                mfd->ifp->RedoFrame();
            break;
            case IDC_FULL:
                switch (mfd->space_mode){
                    case 0:
                    mfd->process = PROCESS_FULL;
                    break;
                    case 1:
                    mfd->process = PROCESS_OFF;
                    break;
                    case 2:
                    mfd->process = PROCESS_OFF;
                    break;
                    case 3:
                    mfd->process = PROCESS_OFF;
                    break;
                    case 4:
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
                if (mfd->drwmode[mfd->channel_mode]==0){
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
                        CalcCurve(mfd);
                        if (mfd->drwmode[mfd->channel_mode]==3){
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            SetWindowText(hWnd, mfd->gamma);}
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                        mfd->ifp->RedoFrame();
                    }
                }
                break;
            case IDC_INPUTMINUS:
                if (mfd->drwmode[mfd->channel_mode]==0){
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
                        CalcCurve(mfd);
                        if (mfd->drwmode[mfd->channel_mode]==3){
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            SetWindowText(hWnd, mfd->gamma);}
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                        mfd->ifp->RedoFrame();
                    }
                }

                break;
            case IDC_OUTPUTPLUS:
                if (mfd->drwmode[mfd->channel_mode]==0){
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
                    if (mfd->drwmode[mfd->channel_mode]==3) {
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
                        CalcCurve(mfd);
                        if (mfd->drwmode[mfd->channel_mode]==3){
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            SetWindowText(hWnd, mfd->gamma);}
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                        mfd->ifp->RedoFrame();
                    }
                }
                break;
            case IDC_OUTPUTMINUS:
                if (mfd->drwmode[mfd->channel_mode]==0){
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
                    if (mfd->drwmode[mfd->channel_mode]==3) {
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
                        CalcCurve(mfd);
                        if (mfd->drwmode[mfd->channel_mode]==3){
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            SetWindowText(hWnd, mfd->gamma);}
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                        mfd->ifp->RedoFrame();
                    }
                }
                break;
            case IDC_POINTPLUS:
                if (mfd->drwmode[mfd->channel_mode]!=0){
                    if (mfd->cp<mfd->poic[mfd->channel_mode]-1){
                        mfd->cp++;
                        SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                        SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                        GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);}
                }
                break;
            case IDC_POINTMINUS:
                if (mfd->drwmode[mfd->channel_mode]!=0){
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
                    case 0:
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
                    case 1:
                        mfd->poic[mfd->channel_mode]=2;
                        mfd->drwpoint[mfd->channel_mode][0][0]=0;
                        mfd->drwpoint[mfd->channel_mode][0][1]=0;
                        mfd->drwpoint[mfd->channel_mode][1][0]=255;
                        mfd->drwpoint[mfd->channel_mode][1][1]=255;
                        CalcCurve(mfd);
                        mfd->cp=0;
                        SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                        SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                        break;
                    case 2:
                        mfd->poic[mfd->channel_mode]=2;
                        mfd->drwpoint[mfd->channel_mode][0][0]=0;
                        mfd->drwpoint[mfd->channel_mode][0][1]=0;
                        mfd->drwpoint[mfd->channel_mode][1][0]=255;
                        mfd->drwpoint[mfd->channel_mode][1][1]=255;
                        CalcCurve(mfd);
                        mfd->cp=0;
                        SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                        SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                        SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                        break;
                    case 3:
                        mfd->poic[mfd->channel_mode]=3;
                        mfd->drwpoint[mfd->channel_mode][0][0]=0;
                        mfd->drwpoint[mfd->channel_mode][0][1]=0;
                        mfd->drwpoint[mfd->channel_mode][1][0]=128;
                        mfd->drwpoint[mfd->channel_mode][1][1]=128;
                        mfd->drwpoint[mfd->channel_mode][2][0]=255;
                        mfd->drwpoint[mfd->channel_mode][2][1]=255;
                        CalcCurve(mfd);
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
                if (mfd->drwmode[mfd->channel_mode] == 0){
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
                if (mfd->drwmode[mfd->channel_mode] == 0){
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
                    CalcCurve(mfd);
                    SetDlgItemInt(hdlg, IDC_VALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][0]), FALSE);
                    SetDlgItemInt(hdlg, IDC_OUTPUTVALUE, (mfd->drwpoint[mfd->channel_mode][mfd->cp][1]), FALSE);
                    SetDlgItemInt(hdlg, IDC_POINTNO, (mfd->cp+1), FALSE);
                    if (mfd->drwmode[mfd->channel_mode] == 3){
                        hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                        SetWindowText(hWnd, mfd->gamma);}
                }
                GrdDrawGradTable(GetDlgItem(hdlg, IDC_GRADCURVE), mfd->ovalue[(mfd->channel_mode)], mfd->laboff, mfd->drwmode[mfd->channel_mode], mfd->drwpoint[(mfd->channel_mode)], mfd->poic[(mfd->channel_mode)], mfd->cp);
                mfd->ifp->RedoFrame();
                break;
            case IDC_RADIOPM:
                if (mfd->drwmode[mfd->channel_mode]!=0){
                    mfd->drwmode[mfd->channel_mode]=0;
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
                if (mfd->drwmode[mfd->channel_mode]!=1){
                    if (mfd->drwmode[mfd->channel_mode]==0) {
                        mfd->poic[mfd->channel_mode]=2;
                        mfd->drwpoint[mfd->channel_mode][0][0]=0;
                        mfd->drwpoint[mfd->channel_mode][0][1]=0;
                        mfd->drwpoint[mfd->channel_mode][1][0]=255;
                        mfd->drwpoint[mfd->channel_mode][1][1]=255;
                        mfd->cp=0;}
                    mfd->drwmode[mfd->channel_mode]=1;
                    CalcCurve(mfd);
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
                if (mfd->drwmode[mfd->channel_mode]!=2){
                    if (mfd->drwmode[mfd->channel_mode]==0) {
                        mfd->poic[mfd->channel_mode]=2;
                        mfd->drwpoint[mfd->channel_mode][0][0]=0;
                        mfd->drwpoint[mfd->channel_mode][0][1]=0;
                        mfd->drwpoint[mfd->channel_mode][1][0]=255;
                        mfd->drwpoint[mfd->channel_mode][1][1]=255;
                        mfd->cp=0;}
                    mfd->drwmode[mfd->channel_mode]=2;
                    CalcCurve(mfd);
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
                if (mfd->drwmode[mfd->channel_mode]!=3){
                    if (mfd->drwmode[mfd->channel_mode]==0 || mfd->poic[mfd->channel_mode]!=3) {
                        mfd->poic[mfd->channel_mode]=3;
                        mfd->drwpoint[mfd->channel_mode][0][0]=0;
                        mfd->drwpoint[mfd->channel_mode][0][1]=0;
                        mfd->drwpoint[mfd->channel_mode][1][0]=128;
                        mfd->drwpoint[mfd->channel_mode][1][1]=128;
                        mfd->drwpoint[mfd->channel_mode][2][0]=255;
                        mfd->drwpoint[mfd->channel_mode][2][1]=255;
                        mfd->cp=0;}
                    mfd->drwmode[mfd->channel_mode]=3;
                    CalcCurve(mfd);
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
                        mfd->space_mode=spacemode;
                        hWnd = GetDlgItem(hdlg, IDC_CHANNEL);
                        SendMessage(hWnd, CB_RESETCONTENT, 0, 0);
                        mfd->laboff = 0;
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
                        case 4:
                            for(i=0; i<3; i++)
                            {   SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)LABchannel_names[i]);}
                            mfd->channel_mode = 0;
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
                        if (mfd->drwmode[mfd->channel_mode]!=3){
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            ShowWindow(hWnd, SW_HIDE);
                            hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                            ShowWindow(hWnd, SW_HIDE);}
                        if (mfd->drwmode[mfd->channel_mode]==0){
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
                            CalcCurve(mfd);
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
                            if (mfd->drwmode[mfd->channel_mode]==3){
                                hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                                ShowWindow(hWnd, SW_SHOW);
                                SetWindowText(hWnd, mfd->gamma);
                                hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                                ShowWindow(hWnd, SW_SHOW);}
                        }
                        switch  (mfd->drwmode[mfd->channel_mode]) {
                            case 0:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_CHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                                break;
                            case 1:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_CHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                                break;
                            case 2:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_CHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                                break;
                            case 3:
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
                        mfd->channel_mode = mode;
                        if (mfd->space_mode == 4) {
                            if (mfd->channel_mode == 2) {mfd->laboff = -9;}
                            else if (mfd->channel_mode == 3) {mfd->laboff = 8;}
                            else {mfd->laboff = 0;}
                        }
                        else {mfd->laboff = 0;}
                        if (mfd->drwmode[mfd->channel_mode]!=3){
                            hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                            ShowWindow(hWnd, SW_HIDE);
                            hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                            ShowWindow(hWnd, SW_HIDE);}
                        if (mfd->drwmode[mfd->channel_mode]==0){
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
                            CalcCurve(mfd);
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
                            if (mfd->drwmode[mfd->channel_mode]==3){
                                hWnd = GetDlgItem(hdlg, IDC_GAMMAVALUE);
                                ShowWindow(hWnd, SW_SHOW);
                                SetWindowText(hWnd, mfd->gamma);
                                hWnd = GetDlgItem(hdlg, IDC_GAMMADSC);
                                ShowWindow(hWnd, SW_SHOW);}
                        }
                        switch  (mfd->drwmode[mfd->channel_mode]) {
                            case 0:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_CHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                                break;
                            case 1:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_CHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                                break;
                            case 2:
                                CheckDlgButton(hdlg, IDC_RADIOPM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOLM, BST_UNCHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOSM, BST_CHECKED);
                                CheckDlgButton(hdlg, IDC_RADIOGM, BST_UNCHECKED);
                                break;
                            case 3:
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

int ConfigProc(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd) {
    MyFilterData *mfd = (MyFilterData *) fa->filter_data;
    MyFilterData mfd_old = *mfd;
    int ret;

    hInst = fa->filter->module->hInstModule;
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
    int cc;
    int cnt;
    bool nf;
    const char *tmp;
    nf = false;

    mfd->process = argv[0].asInt();
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
            mfd->drwmode[i]=0;
            mfd->poic[i]=2;
            mfd->drwpoint[i][0][0]=0;
            mfd->drwpoint[i][0][1]=0;
            mfd->drwpoint[i][1][0]=255;
            mfd->drwpoint[i][1][1]=255;}
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
    strcat (buf, "\",\"");
    _snprintf(tmp, buflen, "%01x",1);
    strcat (buf,tmp);
    _snprintf(tmp, buflen, "%01x",5);
    strcat (buf,tmp);
    for (i=0;i<5;i++) {
        _snprintf(tmp, buflen, "%01x",mfd->drwmode[i]);
        strcat (buf,tmp);}
    for (i=0;i<5;i++) {
        _snprintf(tmp, buflen, "%02x",mfd->poic[i]);
        strcat (buf,tmp);}
    for (j=0; j<5;j++) {
        for (i=0; i<mfd->poic[j]; i++) {
            _snprintf(tmp, buflen, "%02x",mfd->drwpoint[j][i][0]);
            strcat (buf,tmp);
            _snprintf(tmp, buflen, "%02x",mfd->drwpoint[j][i][1]);
            strcat (buf,tmp);}
        }
    strcat (buf, "\")");
    return true;
}

void CalcCurve(MyFilterData *mfd)
{
    int c1;
    int c2;
    int dx;
    int dy;
    int dxg;
    int dyg;
    int i;
    int j;
    int vy;
    double div;
    double inc;
    double ofs;
    double ga;
    double x[16][16];
    double y[16];
    double a[16];
    double b[16];
    double c[16];

    if (mfd->drwpoint[mfd->channel_mode][0][0]>0) {for (c2=0;c2<mfd->drwpoint[mfd->channel_mode][0][0];c2++){mfd->ovalue[mfd->channel_mode][c2]=mfd->drwpoint[mfd->channel_mode][0][1];}}
    switch (mfd->drwmode[mfd->channel_mode]){
        case 1: //linear mode
            for (c1=0; c1<(mfd->poic[mfd->channel_mode]-1); c1++){
                div=(mfd->drwpoint[mfd->channel_mode][(c1+1)][0]-mfd->drwpoint[mfd->channel_mode][c1][0]);
                inc=(mfd->drwpoint[mfd->channel_mode][(c1+1)][1]-mfd->drwpoint[mfd->channel_mode][c1][1])/div;
                ofs=mfd->drwpoint[mfd->channel_mode][c1][1]-inc*mfd->drwpoint[mfd->channel_mode][c1][0];
                for (c2=mfd->drwpoint[mfd->channel_mode][c1][0];c2<(mfd->drwpoint[mfd->channel_mode][(c1+1)][0]+1);c2++)
                {mfd->ovalue[mfd->channel_mode][c2]=int(c2*inc+ofs+0.5);}
            }
        break;
        case 2: //spline mode
            for (i=0;i<16;i++){ //clear tables
                for (j=0;j<16;j++) {x[i][j]=0;}
                y[i]=0;
                a[i]=0;
                b[i]=0;
                c[i]=0;}

            if (mfd->poic[mfd->channel_mode]>3) { //curve has more than 3 coordinates
                j=mfd->poic[mfd->channel_mode]-3; //fill the matrix needed to calculate the b coefficients of the cubic functions an*x^3+bn*x^2+cn*x+dn
                x[0][0]=double(2*(mfd->drwpoint[mfd->channel_mode][2][0]-mfd->drwpoint[mfd->channel_mode][0][0]));
                x[0][1]=double((mfd->drwpoint[mfd->channel_mode][2][0]-mfd->drwpoint[mfd->channel_mode][1][0]));
                y[0]=3*(double(mfd->drwpoint[mfd->channel_mode][2][1]-mfd->drwpoint[mfd->channel_mode][1][1])/double(mfd->drwpoint[mfd->channel_mode][2][0]-mfd->drwpoint[mfd->channel_mode][1][0])-double(mfd->drwpoint[mfd->channel_mode][1][1]-mfd->drwpoint[mfd->channel_mode][0][1])/double(mfd->drwpoint[mfd->channel_mode][1][0]-mfd->drwpoint[mfd->channel_mode][0][0]));
                for (i=1;i<j;i++){
                    x[i][i-1]=double((mfd->drwpoint[mfd->channel_mode][i+1][0]-mfd->drwpoint[mfd->channel_mode][i][0]));
                    x[i][i]=double(2*(mfd->drwpoint[mfd->channel_mode][i+2][0]-mfd->drwpoint[mfd->channel_mode][i][0]));
                    x[i][i+1]=double((mfd->drwpoint[mfd->channel_mode][i+2][0]-mfd->drwpoint[mfd->channel_mode][i+1][0]));
                    y[i]=3*(double(mfd->drwpoint[mfd->channel_mode][i+2][1]-mfd->drwpoint[mfd->channel_mode][i+1][1])/double(mfd->drwpoint[mfd->channel_mode][i+2][0]-mfd->drwpoint[mfd->channel_mode][i+1][0])-double(mfd->drwpoint[mfd->channel_mode][i+1][1]-mfd->drwpoint[mfd->channel_mode][i][1])/double(mfd->drwpoint[mfd->channel_mode][i+1][0]-mfd->drwpoint[mfd->channel_mode][i][0]));
                }
                x[j][j-1]=double(mfd->drwpoint[mfd->channel_mode][j+1][0]-mfd->drwpoint[mfd->channel_mode][j][0]);
                x[j][j]=double(2*(mfd->drwpoint[mfd->channel_mode][j+2][0]-mfd->drwpoint[mfd->channel_mode][j][0]));
                y[j]=3*(double(mfd->drwpoint[mfd->channel_mode][j+2][1]-mfd->drwpoint[mfd->channel_mode][j+1][1])/double(mfd->drwpoint[mfd->channel_mode][j+2][0]-mfd->drwpoint[mfd->channel_mode][j+1][0])-double(mfd->drwpoint[mfd->channel_mode][j+1][1]-mfd->drwpoint[mfd->channel_mode][j][1])/double(mfd->drwpoint[mfd->channel_mode][j+1][0]-mfd->drwpoint[mfd->channel_mode][j][0]));

                for (i=0;i<mfd->poic[mfd->channel_mode]-3;i++) { //resolve the matrix to get the b coefficients
                    div=x[i+1][i]/x[i][i];
                    x[i+1][i]=x[i+1][i]-x[i][i]*div;
                    x[i+1][i+1]=x[i+1][i+1]-x[i][i+1]*div;
                    x[i+1][i+2]=x[i+1][i+2]-x[i][i+2]*div;
                    y[i+1]=y[i+1]-y[i]*div;}
                b[mfd->poic[mfd->channel_mode]-2]=y[mfd->poic[mfd->channel_mode]-3]/x[mfd->poic[mfd->channel_mode]-3][mfd->poic[mfd->channel_mode]-3]; //last b coefficient
                for (i=mfd->poic[mfd->channel_mode]-3;i>0;i--) {b[i]=(y[i-1]-x[i-1][i]*b[i+1])/x[i-1][i-1];} // backward subsitution to get the rest of the the b coefficients
            }
            else if (mfd->poic[mfd->channel_mode]==3) { //curve has 3 coordinates
                b[1]=3*(double(mfd->drwpoint[mfd->channel_mode][2][1]-mfd->drwpoint[mfd->channel_mode][1][1])/double(mfd->drwpoint[mfd->channel_mode][2][0]-mfd->drwpoint[mfd->channel_mode][1][0])-double(mfd->drwpoint[mfd->channel_mode][1][1]-mfd->drwpoint[mfd->channel_mode][0][1])/double(mfd->drwpoint[mfd->channel_mode][1][0]-mfd->drwpoint[mfd->channel_mode][0][0]))/double(2*(mfd->drwpoint[mfd->channel_mode][2][0]-mfd->drwpoint[mfd->channel_mode][0][0]));}

            for (c2=0;c2<(mfd->poic[mfd->channel_mode]-1);c2++){ //get the a and c coefficients
                a[c2]=(double(b[c2+1]-b[c2])/double(3*(mfd->drwpoint[mfd->channel_mode][c2+1][0]-mfd->drwpoint[mfd->channel_mode][c2][0])));
                c[c2]=double(mfd->drwpoint[mfd->channel_mode][c2+1][1]-mfd->drwpoint[mfd->channel_mode][c2][1])/double(mfd->drwpoint[mfd->channel_mode][c2+1][0]-mfd->drwpoint[mfd->channel_mode][c2][0])-double(b[c2+1]-b[c2])*double(mfd->drwpoint[mfd->channel_mode][c2+1][0]-mfd->drwpoint[mfd->channel_mode][c2][0])/3-b[c2]*(mfd->drwpoint[mfd->channel_mode][c2+1][0]-mfd->drwpoint[mfd->channel_mode][c2][0]);}
            for (c1=0;c1<(mfd->poic[mfd->channel_mode]-1);c1++){ //calculate the y values of the spline curve
                for (c2=mfd->drwpoint[mfd->channel_mode][(c1)][0];c2<(mfd->drwpoint[mfd->channel_mode][(c1+1)][0]+1);c2++){
                    vy=int(0.5+a[c1]*(c2-mfd->drwpoint[mfd->channel_mode][c1][0])*(c2-mfd->drwpoint[mfd->channel_mode][c1][0])*(c2-mfd->drwpoint[mfd->channel_mode][c1][0])+b[c1]*(c2-mfd->drwpoint[mfd->channel_mode][c1][0])*(c2-mfd->drwpoint[mfd->channel_mode][c1][0])+c[c1]*(c2-mfd->drwpoint[mfd->channel_mode][c1][0])+mfd->drwpoint[mfd->channel_mode][c1][1]);
                    if (vy>255) {mfd->ovalue[mfd->channel_mode][c2]=255;}
                    else if (vy<0) {mfd->ovalue[mfd->channel_mode][c2]=0;}
                    else {mfd->ovalue[mfd->channel_mode][c2]=vy;}}
            }
        break;
        case 3: //gamma mode
            dx=mfd->drwpoint[mfd->channel_mode][2][0]-mfd->drwpoint[mfd->channel_mode][0][0];
            dy=mfd->drwpoint[mfd->channel_mode][2][1]-mfd->drwpoint[mfd->channel_mode][0][1];
            dxg=mfd->drwpoint[mfd->channel_mode][1][0]-mfd->drwpoint[mfd->channel_mode][0][0];
            dyg=mfd->drwpoint[mfd->channel_mode][1][1]-mfd->drwpoint[mfd->channel_mode][0][1];
            ga=log(double(dyg)/double(dy))/log(double(dxg)/double(dx));
            _snprintf(mfd->gamma, 10, "%.3lf",(1/ga));
            for (c1=0; c1<dx+1; c1++){
                mfd->ovalue[mfd->channel_mode][c1+mfd->drwpoint[mfd->channel_mode][0][0]]=int(0.5+dy*(pow((double(c1)/dx),(ga))))+mfd->drwpoint[mfd->channel_mode][0][1];
            }
        break;
    }
    if (mfd->drwpoint[mfd->channel_mode][((mfd->poic[mfd->channel_mode])-1)][0]<255) {for (c2=mfd->drwpoint[mfd->channel_mode][((mfd->poic[mfd->channel_mode])-1)][0];c2<256;c2++){mfd->ovalue[mfd->channel_mode][c2]=mfd->drwpoint[mfd->channel_mode][(mfd->poic[mfd->channel_mode]-1)][1];}}
    switch (mfd->channel_mode) { //for faster RGB modes
        case 0:
            for (i=0;i<256;i++) {
                mfd->rvalue[0][i]=(mfd->ovalue[0][i]<<16);
                mfd->rvalue[2][i]=(mfd->ovalue[0][i]-i)<<16;
                mfd->gvalue[0][i]=(mfd->ovalue[0][i]<<8);
                mfd->gvalue[2][i]=(mfd->ovalue[0][i]-i)<<8;
                mfd->bvalue[i]=mfd->ovalue[0][i]-i;}
        break;
        case 1:
            for (i=0;i<256;i++) {mfd->rvalue[1][i]=(mfd->ovalue[1][i]<<16);}
        break;
        case 2:
            for (i=0;i<256;i++) {mfd->gvalue[1][i]=(mfd->ovalue[2][i]<<8);}
        break;
    }
}

void PreCalcLut()
{
    long count;
    long x;
    long y;
    long z;
    long rr;
    long gg;
    long bb;
    long r1;
    long g1;
    long b1;
    int r;
    int g;
    int b;

    rgblab = new long[16777216];
    labrgb = new long[16777216];

    count=0;
    for (r=0; r<256; r++) {
        for (g=0; g<256; g++) {
            for (b=0; b<256; b++) {
                if (r > 10) {rr=long(pow(((r<<4)+224.4),(2.4)));}
                else {rr=long((r<<4)*9987.749);}
                if (g > 10) {gg=long(pow(((g<<4)+224.4),(2.4)));}
                else {gg=long((g<<4)*9987.749);}
                if (b > 10) {bb=long(pow(((b<<4)+224.4),(2.4)));}
                else {bb=long((b<<4)*9987.749);}
                x = long((rr+6.38287545)/12.7657509 + (gg+7.36187255)/14.7237451 + (bb+14.58712555)/29.1742511);
                y = long((rr+12.37891725)/24.7578345 + (gg+3.68093628)/7.36187256 + (bb+36.4678139)/72.9356278);
                z = long((rr+136.1678335)/272.335667 + (gg+22.0856177)/44.1712354 + (bb+2.76970661)/5.53941322);
                //XYZ to Lab
                if (x>841776){rr=long(pow((x),(0.33333333333333333333333333333333))*21.9122842);}
                else {rr=long((x+610.28989295)/1220.5797859+1379.3103448275862068965517241379);}
                if (y>885644){gg=long(pow((y),(0.33333333333333333333333333333333))*21.5443498);}
                else {gg=long((y+642.0927467)/1284.1854934+1379.3103448275862068965517241379);}
                if (z>964440){bb=long(pow((z),(0.33333333333333333333333333333333))*20.9408726);}
                else {bb=long((z+699.1298454)/1398.2596908+1379.3103448275862068965517241379);}
                x=long(((gg+16.90331)/33.806620)-40.8);
                y=long(((rr-gg+7.23208898)/14.46417796)+119.167434);
                z=long(((gg-bb+19.837527645)/39.67505529)+135.936123);
                rgblab[count]=((x<<16)+(y<<8)+z);
                count++;
            }
        }
    }
    count = 0;
    for (x=0; x<256; x++) {
        for (y=0; y<256; y++) {
            for (z=0; z<256; z++) {
                gg=x*50+2040;
                rr=long(y*21.392519204-2549.29163142+gg);
                bb=long(gg-z*58.67940678+7976.6510628);
                if (gg>3060) {g1=long(gg*gg/32352.25239*gg);}
                else {g1=long(x*43413.9788);}
                if (rr>3060) {r1=long(rr*rr/34038.16258*rr);}
                else {r1=long(rr*825.27369-1683558);}
                if (bb>3060) {b1=long(bb*bb/29712.85911*bb);}
                else {b1=long(bb*945.40885-1928634);}
                //XYZ to RGB
                rr = long(r1*16.20355 + g1*-7.6863 + b1*-2.492855);
                gg = long(r1*-4.84629 + g1*9.37995 + b1*0.2077785);
                bb = long(r1*0.278176 + g1*-1.01998 + b1*5.28535);
                if (rr>1565400) {r=int((pow((rr),(0.41666666666666666666666666666667))+7.8297554795)/15.659510959-13.996);}
                else {r=int((rr+75881.7458872)/151763.4917744);}
                if (gg>1565400) {g=int((pow((gg),(0.41666666666666666666666666666667))+7.8297554795)/15.659510959-14.019);}
                else {g=int((gg+75881.7458872)/151763.4917744);}
                if (bb>1565400) {b=int((pow((bb),(0.41666666666666666666666666666667))+7.8297554795)/15.659510959-13.990);}
                else {b=int((bb+75881.7458872)/151763.4917744);}
                if (r<0) {r=0;} else if (r>255) {r=255;}
                if (g<0) {g=0;} else if (g>255) {g=255;}
                if (b<0) {b=0;} else if (b>255) {b=255;}
                labrgb[count]=((r<<16)+(g<<8)+b);
                count++;
            }
        }
    }
}

void GrdDrawGradTable(HWND hWnd, int table[], int loff, int dmode, int dp[16][2], int pc, int ap)  // draw the curve
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
    if (dmode!=0) {
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

void GrdDrawBorder(HWND hWnd, HWND hWnd2, MyFilterData *mfd) // draw the two color borders
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
    long rr;
    long gg;
    long bb;
    int ch;
    int chi;
    long lab;
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
                            x = i<<16;
                            y = 0;
                            z = 0;
                        break;
                        case 2:
                            x = (j*difb)<<16;
                            y = i-128;
                            z = 0;
                        break;
                        case 3:
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
                            x = i;
                            y = 255;
                            z = 255;
                        break;
                        case 2:
                            x = (j/dif)*86;
                            y = i;
                            z = 255;
                        break;
                        case 3:
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
                    case 4:
                        switch(mfd->channel_mode)
                        {
                        case 1:
                            lab=labrgb[((i<<16)+30603)];
                        break;
                        case 2:
                            lab=labrgb[(((j*difb)<<16)+(i<<8)+136)];
                        break;
                        case 3:
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

void ImportCurve(MyFilterData *mfd) // import curves
{
    FILE *pFile;
    int i;
    int j;
    int stor[1280];
    int temp[1280];
    long lSize;
    int beg;
    int cv;
    int count;
    int noocur;
    int curpos;
    int cordpos;
    int curposnext;
    int cordcount;
    int cmtmp;
    int drwmodtmp;
    int pictmp;
    int drwtmp[16][2];
    bool nrf;
    int gma;
    curpos = 0;
    cordpos = 7;
    cordcount = 0;
    gma=1;
    nrf=false;

    for (i=0;i<5;i++){mfd->drwmode[i]=0;}

        if (mfd->filter == 2) // *.acv
    {
        pFile = fopen (mfd->filename, "rb");
        if (pFile==NULL)
        {
            MessageBox (NULL, TEXT ("Error"), TEXT ("Error opening file"),0);
        }
        else
        {   fseek (pFile , 0 , SEEK_END);
            lSize = ftell (pFile);
            rewind (pFile);
            for(i=0; (i < lSize) && ( feof(pFile) == 0 ); i++ ) //read the file and store the coordinates
            {
                cv = fgetc(pFile);
                if (i==3) { noocur = cv;
                    if (noocur>5) {noocur=5;}
                    curpos = 0;}
                if (i==5) {mfd->poic[curpos]=cv;
                    if (noocur >= (curpos+1))
                    {curposnext = i+mfd->poic[curpos]*4+2;
                    curpos++;}}
                if (i==curposnext) {
                    mfd->poic[curpos] = cv;
                    if (noocur >= (curpos+1))
                    {curposnext = i+mfd->poic[curpos]*4+2;
                    if (mfd->poic[curpos-1]>16) {mfd->poic[curpos-1]=16;}
                    curpos++;
                    cordcount=0;
                    cordpos=i+2;}}
                if (i==cordpos) {
                    mfd->drwpoint[curpos-1][cordcount][1]=cv;}
                if (i==(cordpos+2)) {
                    mfd->drwpoint[curpos-1][cordcount][0]=cv;
                    if (cordcount<15) {cordcount++;}
                    cordpos=cordpos+4;}
            }
            fclose (pFile);
            if (noocur<5){ //fill empty curves if acv does contain less than 5 curves
                for (i=noocur;i<5;i++)
                    {mfd->poic[i]=2;
                    mfd->drwpoint[i][0][0]=0;
                    mfd->drwpoint[i][0][1]=0;
                    mfd->drwpoint[i][1][0]=255;
                    mfd->drwpoint[i][1][1]=255;}
                noocur=5;}
            mfd->cp=0;
            cmtmp=mfd->channel_mode;
            for (i=0;i<5;i++) { // calculate curve values
                mfd->drwmode[i]=2;
                mfd->channel_mode=i;
                CalcCurve(mfd);}
            mfd->channel_mode=cmtmp;
            nrf=true;
        }
    }
    if (mfd->filter == 3) { // *.csv
        pFile = fopen (mfd->filename, "r");
        if (pFile==NULL) {MessageBox (NULL, TEXT ("Error"), TEXT ("Error opening file"),0);}
        else
        {
            fseek (pFile , 0 , SEEK_END);
            lSize = ftell (pFile);
            rewind (pFile);
            for(i=0; (i < 1280) && ( feof(pFile) == 0 ); i++ )
            {
                fscanf (pFile, "%d", &stor[i]);
            }
            fclose (pFile);
            lSize = lSize/4;
        }
    }
    else if (mfd->filter == 4 || mfd->filter == 5) { // *.crv *.map
        if (mfd->filter == 4) {beg=64;}
        else {beg=320;}
        curpos = -1;
        curposnext = 65530;
        cordpos = beg+6;
        pFile = fopen (mfd->filename, "rb");
        if (pFile==NULL) {MessageBox (NULL, TEXT ("Error"), TEXT ("Error opening file"),0);}
        else
        {   fseek (pFile , 0 , SEEK_END);
            lSize = ftell (pFile);
            rewind (pFile);
            for(i=0; (i < lSize) && ( feof(pFile) == 0 ); i++ )
            {
                cv = fgetc(pFile);
                if (i == beg) {
                    curpos++;
                    mfd->drwmode[curpos]=cv;
                    curposnext = 65530;
                    if (mfd->drwmode[curpos]==2 || mfd->drwmode[curpos]==0) {mfd->drwmode[curpos]=abs(mfd->drwmode[curpos]-2);
                    }
                }
                if (i == beg+1 && mfd->drwmode[curpos]==3) {gma=cv;}
                if (i == beg+2 && mfd->drwmode[curpos]==3) {gma=gma+(cv<<8);}
                if (i == beg+5) {
                    mfd->poic[curpos]=cv;
                    cordpos=i+1;
                    curposnext = i+mfd->poic[curpos]*2+1;
                    if (curpos<4) {beg=i+mfd->poic[curpos]*2+257;}
                    cordcount=0;
                    count=0;
                    if (mfd->poic[curpos]>16) {mfd->poic[curpos]=16;} // limit to 16 points
                }
                if (i>=curposnext) { // read raw curve data
                    cordpos=0;
                    if (count<256) {mfd->ovalue[curpos][count]=cv;}
                    count++;}
                if (i == cordpos) {
                    if (mfd->drwmode[curpos]==3 && cordcount==1) {
                        if (gma>250) {mfd->drwpoint[curpos][cordcount][0]=64;}
                        else if (gma<50) {mfd->drwpoint[curpos][cordcount][0]=192;}
                        else {mfd->drwpoint[curpos][cordcount][0]=128;}
                        mfd->drwpoint[curpos][cordcount][1]=int(pow(float(mfd->drwpoint[curpos][cordcount][0])/256,100/float(gma))*256+0.5);
                        cordcount++;
                        mfd->poic[curpos]++;}
                    mfd->drwpoint[curpos][cordcount][0]=cv;}
                if (i == cordpos+1) {
                    mfd->drwpoint[curpos][cordcount][1]=cv;
                    if (cordcount<mfd->poic[curpos]-1 && cordcount<15) {cordcount++;} // limit to 16 points
                    cordpos=cordpos+2;}
            }
            fclose (pFile);
        }
        if (mfd->filter == 5) { //*.map exchange 4<->0
            drwmodtmp=mfd->drwmode[4];
            pictmp=mfd->poic[4];
            for (i=0;i<pictmp;i++){
                drwtmp[i][0]=mfd->drwpoint[4][i][0];
                drwtmp[i][1]=mfd->drwpoint[4][i][1];}
            for (j=4;j>0;j--) {
                for (i=0;i<mfd->poic[j-1];i++) {
                    mfd->drwpoint[j][i][0]=mfd->drwpoint[j-1][i][0];
                    mfd->drwpoint[j][i][1]=mfd->drwpoint[j-1][i][1];}
                mfd->poic[j]=mfd->poic[j-1];
                mfd->drwmode[j]=mfd->drwmode[j-1];}
            for (i=0;i<pictmp;i++){
                mfd->drwpoint[0][i][0]=drwtmp[i][0];
                mfd->drwpoint[0][i][1]=drwtmp[i][1];}
            mfd->poic[0]=pictmp;
            mfd->drwmode[0]=drwmodtmp;
            for (i=0;i<256;i++) {temp[i]=mfd->ovalue[4][i];}
            for (j=4;j>0;j--) {
                for (i=0;i<256;i++) {mfd->ovalue[j][i]=mfd->ovalue[j-1][i];}
            }
            for (i=0;i<256;i++) {mfd->ovalue[0][i]=temp[i];}
        }
        cmtmp=mfd->channel_mode;
        for (i=0;i<5;i++) { // calculate curve values
            mfd->channel_mode=i;
            if (mfd->drwmode[i]!=0) {CalcCurve(mfd);}
        }
        mfd->channel_mode=cmtmp;
        mfd->cp=0;
        nrf=true;
    }
    else if (mfd->filter == 6) // *.amp Smartvurve hsv
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
            for(i=0; (i < 768) && ( feof(pFile) == 0 ); i++ )
            {
                if (i<256)
                {stor[i+512] = fgetc(pFile);}
                if (i>255 && i <512)
                {stor[i] = fgetc(pFile);}
                if (i>511)
                {stor[i-512] = fgetc(pFile);}
            }
            fclose (pFile);
            lSize = 768;
        }
    }
    else
    {
        pFile = fopen (mfd->filename, "rb"); // *.amp
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
    if (nrf==false) { //fill curves for non coordinates file types
        if (lSize > 768){
            for(i=0; i < 256; i++) {
                mfd->ovalue[0][i] = stor[i];
                mfd->rvalue[0][i]=(mfd->ovalue[0][i]<<16);
                mfd->rvalue[2][i]=(mfd->ovalue[0][i]-i)<<16;
                mfd->gvalue[0][i]=(mfd->ovalue[0][i]<<8);
                mfd->gvalue[2][i]=(mfd->ovalue[0][i]-i)<<8;
                mfd->bvalue[i]=mfd->ovalue[0][i]-i;
            }
            for(i=256; i < 512; i++) {
                mfd->ovalue[1][(i-256)] = stor[i];
                mfd->rvalue[1][(i-256)]=(mfd->ovalue[1][(i-256)]<<16);
            }
            for(i=512; i < 768; i++) {
                mfd->ovalue[2][(i-512)] = stor[i];
                mfd->gvalue[1][(i-512)]=(mfd->ovalue[2][(i-512)]<<8);
            }
            for(i=768; i < 1024; i++) {mfd->ovalue[3][(i-768)] = stor[i];}
            for(i=1024; i < 1280; i++) {mfd->ovalue[4][(i-1024)] = stor[i];}
        }
        if (lSize < 769 && lSize > 256){
            for(i=0; i < 256; i++) {
                mfd->ovalue[1][i] = stor[i];
                mfd->rvalue[1][i]=(mfd->ovalue[1][i]<<16);
            }
            for(i=256; i < 512; i++) {
                mfd->ovalue[2][(i-256)] = stor[i];
                mfd->gvalue[1][(i-256)]=(mfd->ovalue[2][(i-256)]<<8);
            }
            for(i=512; i < 768; i++) {mfd->ovalue[3][(i-512)] = stor[i];}
        }
        if (lSize < 257 && lSize > 0) {
            for(i=0; i < 256; i++) {
                mfd->ovalue[0][i] = stor[i];
                mfd->rvalue[0][i]=(mfd->ovalue[0][i]<<16);
                mfd->rvalue[2][i]=(mfd->ovalue[0][i]-i)<<16;
                mfd->gvalue[0][i]=(mfd->ovalue[0][i]<<8);
                mfd->gvalue[2][i]=(mfd->ovalue[0][i]-i)<<8;
                mfd->bvalue[i]=mfd->ovalue[0][i]-i;
            }
        }
        for (i=0;i<5;i++) {
            mfd->drwmode[i]=0;
            mfd->poic[i]=2;
            mfd->drwpoint[i][0][0]=0;
            mfd->drwpoint[i][0][1]=0;
            mfd->drwpoint[i][1][0]=255;
            mfd->drwpoint[i][1][1]=255;
        }
    }
}
void ExportCurve(MyFilterData *mfd) // export curves
{
    FILE *pFile;
    int i;
    int j;
    char c;
    char zro;

    if (mfd->filter == 2) { // *.acv
        zro = char (0);
        pFile = fopen (mfd->filename,"wb");
        fprintf (pFile, "%c",zro);
        c = char (4);
        fprintf (pFile, "%c",c);
        fprintf (pFile, "%c",zro);
        c = char (5);
        fprintf (pFile, "%c",c);
        for (j=0; j<5;j++) {
            fprintf (pFile, "%c",zro);
            c = char (mfd->poic[j]);
            fprintf (pFile, "%c",c);
            for (i=0; i<mfd->poic[j]; i++) {
                fprintf (pFile, "%c",zro);
                c = char (mfd->drwpoint[j][i][1]);
                fprintf (pFile, "%c",c);
                fprintf (pFile, "%c",zro);
                c = char (mfd->drwpoint[j][i][0]);
                fprintf (pFile, "%c",c);
            }
        }
    }
    else if (mfd->filter == 3) { // *.csv
        pFile = fopen (mfd->filename,"w");
        for (j=0; j<5;j++) {
            for (i=0; i<256; i++) {
                fprintf (pFile, "%d\n",(mfd->ovalue[j][i]));
            }
        }
    }
    else { // *.amp
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
