#ifndef GRADATION_H
#define GRADATION_H

#if __cplusplus >= 201103L
#include <stdint.h>
#else
typedef long int32_t;
typedef unsigned long uint32_t;
#endif

extern int rgblab[]; //LUT Lab
extern int labrgb[]; //LUT Lab

enum Space {
    SPACE_RGB               = 0,
    SPACE_YUV               = 1,
    SPACE_CMYK              = 2,
    SPACE_HSV               = 3,
    SPACE_LAB               = 4,
};

static const char *space_names[] = {
    "RGB",
    "YUV",
    "CMYK",
    "HSV",
    "Lab",
};

enum Channel {
    CHANNEL_RGB             = 0,
    CHANNEL_RED             = 1,
    CHANNEL_GREEN           = 2,
    CHANNEL_BLUE            = 3,

    CHANNEL_Y               = 1,
    CHANNEL_U               = 2,
    CHANNEL_V               = 3,

    CHANNEL_CYAN            = 1,
    CHANNEL_MAGENTA         = 2,
    CHANNEL_YELLOW          = 3,
    CHANNEL_BLACK           = 4,

    CHANNEL_HUE             = 1,
    CHANNEL_SATURATION      = 2,
    CHANNEL_VALUE           = 3,

    CHANNEL_L               = 1,
    CHANNEL_A               = 2,
    CHANNEL_B               = 3,
};

static const char *RGBchannel_names[] = {
    "RGB",
    "Red",
    "Green",
    "Blue",
};

static const char *YUVchannel_names[] = {
    "Luminance",
    "ChromaB",
    "ChromaR",
};

static const char *CMYKchannel_names[] = {
    "Cyan",
    "Magenta",
    "Yellow",
    "Black",
};

static const char *HSVchannel_names[] = {
    "Hue",
    "Saturation",
    "Value",
};

static const char *LABchannel_names[] = {
    "Luminance",
    "a Red-Green",
    "b Yellow-Blue",
};

enum Process {
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

static const char *process_names[] = {
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

enum DrawMode {
    DRAWMODE_PEN    = 0,
    DRAWMODE_LINEAR = 1,
    DRAWMODE_SPLINE = 2,
    DRAWMODE_GAMMA  = 3,
};

struct Gradation {
    int rvalue[3][256];
    int gvalue[3][256];
    int bvalue[256];
    int ovalue[5][256];
    Space space_mode;
    Channel channel_mode;
    Process process;
    int xl;
    int yl;
    int offset;
    char filename[1024];
    int filter;
    bool Labprecalc;
    int laboff;
    DrawMode drwmode[5];
    int drwpoint[5][16][2];
    int poic[5];
    int cp;
    bool psel;
    double scalex;
    double scaley;
    unsigned int boxx;
    int boxy;
    char gamma[10];
};

int Init(Gradation &grd);
int Run(Gradation &grd, int32_t width, int32_t height, uint32_t *src, uint32_t *dst, int32_t src_modulo, int32_t dst_modulo);

void PreCalcLut(Gradation &grd);
void CalcCurve(Gradation &grd);
bool ImportCurve(Gradation &grd);
void ExportCurve(Gradation &grd);

#endif // GRADATION_MAIN_H
