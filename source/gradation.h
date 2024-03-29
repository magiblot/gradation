#ifndef GRADATION_H
#define GRADATION_H

#include <stdint.h>
#include <stddef.h>

extern int rgblab[]; //LUT Lab
extern int labrgb[]; //LUT Lab

enum Space {
    SPACE_RGB               = 0,
    SPACE_YUV               = 1,
    SPACE_CMYK              = 2,
    SPACE_HSV               = 3,
    SPACE_LAB               = 4,
};

static const char * const space_names[] = {
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

static const char * const RGBchannel_names[] = {
    "RGB",
    "Red",
    "Green",
    "Blue",
};

static const char * const YUVchannel_names[] = {
    "Luminance",
    "ChromaB",
    "ChromaR",
};

static const char * const CMYKchannel_names[] = {
    "Cyan",
    "Magenta",
    "Yellow",
    "Black",
};

static const char * const HSVchannel_names[] = {
    "Hue",
    "Saturation",
    "Value",
};

static const char * const LABchannel_names[] = {
    "Luminance",
    "a Red-Green",
    "b Yellow-Blue",
};

enum ProcessingMode {
    PROCMODE_RGB    = 0,
    PROCMODE_FULL   = 1,
    PROCMODE_RGBW   = 2,
    PROCMODE_FULLW  = 3,
    PROCMODE_OFF    = 4,
    PROCMODE_YUV    = 5,
    PROCMODE_CMYK   = 6,
    PROCMODE_HSV    = 7,
    PROCMODE_LAB    = 8,
};

static const char * const process_names[] = {
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

enum CurveFileType {
    FILETYPE_AMP = 1,
    FILETYPE_ACV = 2,
    FILETYPE_CSV = 3,
    FILETYPE_CRV = 4,
    FILETYPE_MAP = 5,
    FILETYPE_SMARTCURVE_HSV = 6,
};

enum { maxPoints = 32 };

struct Gradation {
    int rvalue[3][256];
    int gvalue[3][256];
    int bvalue[256];
    uint8_t _ovalue[5][256];
    double _ovaluef[5][256];
    ProcessingMode process;
    uint8_t
        precise         : 1,
        Labprecalc      : 1;
    DrawMode drwmode[5];
    uint8_t drwpoint[5][maxPoints][2];
    int poic[5];
    char gamma[10];

    template <class I>
    constexpr const uint8_t (&ovalue(I &&i) const) [256] { return _ovalue[i]; }
    template <class I>
    constexpr const double (&ovaluef(I &&i) const) [256] { return _ovaluef[i]; }
    template <class I, class J>
    constexpr uint8_t ovalue(I &&i, J &&j) const { return _ovalue[i][j]; }
    template <class I, class J>
    constexpr double ovaluef(I &&i, J &&j) const { return _ovaluef[i][j]; }
    template <class I, class J>
    constexpr void ovalue(I &&i, J &&j, uint8_t value) {
        _ovalue[i][j] = value;
        _ovaluef[i][j] = value;
    }
    template <class I, class J>
    constexpr void ovaluef(I &&i, J &&j, double value) {
        _ovalue[i][j] = int(0.5 + value);
        _ovaluef[i][j] = value;
    }
};

void Init(Gradation &grd, bool precise = false);
void Run(const Gradation &grd, int32_t width, int32_t height, uint32_t *src, uint32_t *dst, int32_t src_pitch, int32_t dst_pitch);

void PreCalcLut(Gradation &grd);
void CalcCurve(Gradation &grd, Channel channel);
bool ImportCurve(Gradation &grd, const char *filename, CurveFileType type, DrawMode defDrawMode = DRAWMODE_SPLINE);
void ExportCurve(const Gradation &grd, const char *filename, CurveFileType type);
void ImportPoints(Gradation &grd, Channel channel, const uint8_t points[][2], size_t count, DrawMode drawMode);

inline void InitRGBValues(Gradation &grd, Channel channel, int x) {
    uint8_t val = grd.ovalue(channel, x);
    switch (channel) { // for faster RGB modes
        case CHANNEL_RGB:
            grd.rvalue[0][x] = val << 16;
            grd.rvalue[2][x] = (val - x) << 16;
            grd.gvalue[0][x] = val << 8;
            grd.gvalue[2][x] = (val - x) << 8;
            grd.bvalue[x] = val - x;
            break;
        case CHANNEL_RED:
            grd.rvalue[1][x] = val << 16;
            break;
        case CHANNEL_GREEN:
            grd.gvalue[1][x] = val << 8;
            break;
        default:
            break;
    }
}

inline Space GetSpace(ProcessingMode process) {
    switch (process) {
        case PROCMODE_YUV:  return SPACE_YUV;
        case PROCMODE_CMYK: return SPACE_CMYK;
        case PROCMODE_HSV:  return SPACE_HSV;
        case PROCMODE_LAB:  return SPACE_LAB;
        default:            return SPACE_RGB;
    }
};

inline int GetChannelCount(Space space) {
    switch (space) {
        case SPACE_YUV:  return 3;
        case SPACE_CMYK: return 4;
        case SPACE_HSV:  return 3;
        case SPACE_LAB:  return 3;
        default:         return 4;
    }
};

inline int GetFirstChannel(Space space) {
    switch (space) {
        case SPACE_YUV:  return CHANNEL_Y;
        case SPACE_CMYK: return CHANNEL_CYAN;
        case SPACE_HSV:  return CHANNEL_HUE;
        case SPACE_LAB:  return CHANNEL_L;
        default:         return CHANNEL_RGB;
    }
};


template <class T>
struct RGB { T r, g, b; };

inline RGB<uint8_t> unpackRGB(uint32_t p)
{
    return {
        uint8_t((p & 0xFF0000) >> 16),
        uint8_t((p & 0x00FF00) >> 8),
        uint8_t(p & 0x0000FF),
    };
}

inline uint32_t packRGB(RGB<uint8_t> p)
{
    return ((p.r << 16) + (p.g << 8) + p.b);
}

struct procModeRgb
{
    static RGB<uint8_t> processInt(const Gradation &grd, uint8_t r, uint8_t g, uint8_t b);
    static RGB<double> processDouble(const Gradation &grd, double r, double g, double b);
};

struct procModeFull
{
    static RGB<uint8_t> processInt(const Gradation &grd, uint8_t r, uint8_t g, uint8_t b);
    static RGB<double> processDouble(const Gradation &grd, double r, double g, double b);
};

struct procModeYuv
{
    static RGB<uint8_t> processInt(const Gradation &grd, uint8_t r, uint8_t g, uint8_t b);
    static RGB<double> processDouble(const Gradation &grd, double r, double g, double b);
};

struct procModeHsv
{
    static RGB<uint8_t> processInt(const Gradation &grd, uint8_t r, uint8_t g, uint8_t b);
    static RGB<double> processDouble(const Gradation &grd, double r, double g, double b);
};

#endif // GRADATION_MAIN_H
