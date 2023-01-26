#include "avs.h"

#include <string>
#include <memory>
#include <utility>
#include <stdlib.h>

static constexpr std::pair<const char *, int> processingModes[] =
{
    {"rgb", PROCMODE_RGB},
    {"full", PROCMODE_FULL},
    {"rgbw", PROCMODE_RGBW},
    {"fullw", PROCMODE_FULLW},
    {"yuv", PROCMODE_YUV},
    {"cmyk", PROCMODE_CMYK},
    {"hsv", PROCMODE_HSV},
    {"lab", PROCMODE_LAB},
};

static constexpr std::pair<const char *, int> drawModes[] =
{
    {"linear", DRAWMODE_LINEAR},
    {"spline", DRAWMODE_SPLINE},
    {"gamma", DRAWMODE_GAMMA},
};

static constexpr std::pair<const char *, int> curveFileTypes[] =
{
    {"auto", -1},
    {"SmartCurve HSV", FILETYPE_SMARTCURVE_HSV},
};

static constexpr std::pair<const char *, int> curveFileExtensions[] =
{
    {".amp", FILETYPE_AMP},
    {".acv", FILETYPE_ACV},
    {".csv", FILETYPE_CSV},
    {".crv", FILETYPE_CRV},
    {".map", FILETYPE_MAP},
};

class GradationFilter final : public GenericVideoFilter
{
    const std::unique_ptr<const Gradation> grd;
    FrameProcesser &processFrame;

    GradationFilter( PClip &aChild, std::unique_ptr<Gradation> &aGrd,
                     FrameProcesser &aProcessFrame) :
        GenericVideoFilter(std::move(aChild)),
        grd(std::move(aGrd)),
        processFrame(aProcessFrame)
    {
    }

    int __stdcall SetCacheHints(int cachehints, int frame_range) override;
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

    static int parseEnumImpl(const char *, const char *, const std::pair<const char *, int> *, size_t, IScriptEnvironment *);
    template <class T, size_t N>
    static T parseEnum(const char *, const char *, const std::pair<const char *, int>(&)[N], IScriptEnvironment *);

    static CurveFileType parseCurveFileType(const char *, const char *, const char *, IScriptEnvironment *);
    static void parsePoints(Gradation &, DrawMode, const AVSValue &, const char *Name, IScriptEnvironment *);

    template <GradationProcesser &process>
    static FrameProcesser &getFrameProcesser(const VideoInfo &vi, IScriptEnvironment *env);

    enum { iChild, iProcess, iCurveType, iPoints, iFile, iFileType, iPrecise };

    static const char *Name()
        { return "Gradation"; }
    static const char *Signature()
        { return "c[process]s[curve_type]s[points].[file]s[file_type]s[precise]b"; }
    static AVSValue __cdecl Create(AVSValue args, void *, IScriptEnvironment *env);

public:

    static void Register(IScriptEnvironment *);
};

void GradationFilter::Register(IScriptEnvironment *env)
{
    env->AddFunction(Name(), Signature(), &Create, 0);
}

int __stdcall GradationFilter::SetCacheHints(int cachehints, int)
{
    switch (cachehints)
    {
        case CACHE_GET_MTMODE: return MT_NICE_FILTER;
        default: return 0;
    }
}

PVideoFrame __stdcall GradationFilter::GetFrame(int n, IScriptEnvironment* env)
{
    auto &&src = child->GetFrame(n, env);
    auto &&dst = src->IsWritable() ? (const PVideoFrame &) src
                                   : (const PVideoFrame &) env->NewVideoFrameP(vi, &src);
    processFrame(*grd, vi.width, vi.height, vi.pixel_type, src, dst);
    return dst;
}

int GradationFilter::parseEnumImpl(const char *str, const char *argName, const std::pair<const char *, int> *mappings, size_t count, IScriptEnvironment *env)
{
    for (size_t i = 0; i < count; ++i)
        if (stricmp(str, mappings[i].first) == 0)
            return mappings[i].second;

    std::string enumList {"'"};
    if (count > 0)
        enumList.append(mappings[0].first);
    for (size_t i = 1; i < count; ++i)
        enumList.append("', '"),
        enumList.append(mappings[i].first);
    enumList.append("'"),

    env->ThrowError("%s: Invalid '%s': '%s'. Expected one of: %s", Name(), argName, str, enumList.c_str());
    abort();
}

template <class T, size_t N>
inline T GradationFilter::parseEnum(const char *str, const char *argName, const std::pair<const char *, int> (&mappings)[N], IScriptEnvironment *env)
{
    return T(parseEnumImpl(str, argName, mappings, N, env));
}

CurveFileType GradationFilter::parseCurveFileType(const char *filename, const char *file_type, const char *argName, IScriptEnvironment *env)
{
    CurveFileType type = parseEnum<CurveFileType>(file_type, argName, curveFileTypes, env);
    if (type != CurveFileType(-1))
        return type;
    size_t length = strlen(filename);
    if (length >= 4)
    {
        const char *ext = filename + length - 4;
        for (auto &m : curveFileExtensions)
            if (stricmp(ext, m.first) == 0)
                return CurveFileType(m.second);
    }
    env->ThrowError("%s: Cannot determine type of file '%s'", Name(), filename);
    abort();
}

void GradationFilter::parsePoints(Gradation &grd, DrawMode drawMode, const AVSValue &elems, const char *argName, IScriptEnvironment *env)
{
    Space space = GetSpace(grd.process);
    int channelCount = GetChannelCount(space);
    int firstChannel = GetFirstChannel(space);
    if (elems.ArraySize() > channelCount)
        env->ThrowError("%s: Too many lists of points (%d). Space '%s' only has %d channels", Name(), elems.ArraySize(), space_names[space], channelCount);
    for (int l = 0; l < elems.ArraySize(); ++l)
    {
        auto &points = elems[l];
        if (!points.IsArray())
            env->ThrowError("%s: In list %d of '%s': Not an array", Name(), l, argName);
        if (maxPoints < points.ArraySize() )
            env->ThrowError("%s: In list %d of '%s': Can't have more than %d points", Name(), l, argName, maxPoints);
        uint8_t outPoints[maxPoints][2];
        int16_t pos[256] {0};
        size_t count = 0;
        for (int p = 0; p < points.ArraySize(); ++p, ++count)
        {
            auto &point = points[p];
            if (!point.IsArray() || point.ArraySize() != 2 || !point[0].IsInt() || !point[1].IsInt())
                env->ThrowError("%s: In point %d of list %d of '%s': Invalid point. Expected an array of two integers", Name(), p, l, argName);
            int x = point[0].AsInt();
            int y = point[1].AsInt();
            if (x < 0 || 255 < x || y < 0 || 255 < y)
                env->ThrowError("%s: In list %d of '%s': Out-of-range point (%d, %d)", Name(), l, argName, x, y);
            if (pos[x])
                env->ThrowError("%s: In list %d of '%s': Points (%d, %d) and (%d, %d) overlap", Name(), l, argName, x, pos[x] - 1, x, y);
            pos[x] = y + 1;
            outPoints[count][0] = (uint8_t) x;
            outPoints[count][1] = (uint8_t) y;
        }
        Channel ch = Channel(l + firstChannel);
        ImportPoints(grd, ch, outPoints, count, drawMode);
    }
}

template <GradationProcesser &process>
FrameProcesser &GradationFilter::getFrameProcesser(const VideoInfo &vi, IScriptEnvironment *env)
{
    if (!vi.IsRGB())
        env->ThrowError("%s: Input clip must be RGB(A)", Name());
    switch (vi.BitsPerComponent())
    {
        case 8:  return applyToFrame<process, 8>;
        case 10: return applyToFrame<process, 10>;
        case 12: return applyToFrame<process, 12>;
        case 14: return applyToFrame<process, 14>;
        case 16: return applyToFrame<process, 16>;
        case 32: return applyToFrame<process, 32>;
    }
    env->ThrowError("%s: Unsupported pixel type", Name());
    abort();
}

static void runGradationOld(const Gradation &grd, int width, int height, int, const PVideoFrame &src, const PVideoFrame &dst)
// Pre: clip is RGB32.
{
    Run( grd, width, height,
         (uint32_t *) src->GetReadPtr(), (uint32_t *) dst->GetWritePtr(),
         src->GetPitch(), dst->GetPitch() );
}

AVSValue __cdecl GradationFilter::Create(AVSValue args, void *, IScriptEnvironment *env)
{
    bool precise = args[iPrecise].AsBool(false);
    auto &&grd = std::make_unique<Gradation>();
    Init(*grd, precise);

    if (!args[iProcess].IsString())
        env->ThrowError("%s: Missing parameter 'process'", Name());
    if (args[iPoints].Defined() && !args[iPoints].IsArray())
        env->ThrowError("%s: 'points' is not an array", Name());
    if (!args[iPoints].IsArray() && !args[iFile].IsString())
        env->ThrowError("%s: No 'points' and no 'file' provided", Name());
    if (args[iPoints].IsArray() && args[iFile].IsString())
        env->ThrowError("%s: Only one of 'points', 'file' can be provided at a time", Name());

    grd->process = parseEnum<ProcessingMode>(args[iProcess].AsString(), "process", processingModes, env);
    DrawMode drawMode = parseEnum<DrawMode>(args[iCurveType].AsString("spline"), "curve_type", drawModes, env);
    if (args[iPoints].IsArray())
        parsePoints(*grd, drawMode, args[iPoints], "points", env);
    else
    {
        CurveFileType type = parseCurveFileType(args[iFile].AsString(), args[iFileType].AsString("auto"), "file_type", env);
        if (!ImportCurve(*grd, args[iFile].AsString(), type, drawMode))
            env->ThrowError("%s: Cannot open file '%s'", Name(), args[iFile].AsString());
    }

    PreCalcLut(*grd);

    auto &&child = args[iChild].AsClip();
    auto &vi = child->GetVideoInfo();
    if (precise)
        switch (grd->process)
        {
            case PROCMODE_RGB: return new GradationFilter(child, grd, getFrameProcesser<procModeRgb::processDouble>(vi, env));
            case PROCMODE_FULL: return new GradationFilter(child, grd, getFrameProcesser<procModeFull::processDouble>(vi, env));
            case PROCMODE_YUV: return new GradationFilter(child, grd, getFrameProcesser<procModeYuv::processDouble>(vi, env));
            case PROCMODE_HSV: return new GradationFilter(child, grd, getFrameProcesser<procModeHsv::processDouble>(vi, env));
            default: env->ThrowError("%s: 'precise' not supported for processing mode '%s'", Name(), args[iProcess].AsString());
        }

    if (!vi.IsRGB32())
        env->ThrowError("%s: Input clip must be RGB32", Name());

    return new GradationFilter(child, grd, runGradationOld);
}

const AVS_Linkage *AVS_linkage = 0;

extern "C" EXPORT const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, AVS_Linkage* vectors)
{
    AVS_linkage = vectors;
    GradationFilter::Register(env);
    return 0;
}
