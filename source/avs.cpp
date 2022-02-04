#include <avisynth.h>
#include "gradation.h"

#include <string>
#include <memory>
#include <utility>
#include <stdlib.h>

#ifndef _MSC_VER

#include <strings.h>

static int stricmp(const char *a, const char *b) noexcept
{
    return strcasecmp(a, b);
}

#endif // _MSC_VER

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
    {"pen", DRAWMODE_PEN},
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

class GradationFilter : public GenericVideoFilter
{
    const std::unique_ptr<const Gradation> grd;

    GradationFilter(PClip &aChild, std::unique_ptr<Gradation> &aGrd) :
        GenericVideoFilter(std::move(aChild)),
        grd(std::move(aGrd))
    {
    }

    int __stdcall SetCacheHints(int cachehints, int frame_range) override;
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

    static int parseEnumImpl(const char *, const char *, const std::pair<const char *, int> *, size_t, IScriptEnvironment *);
    template <class T, size_t N>
    static T parseEnum(const char *, const char *, const std::pair<const char *, int>(&)[N], IScriptEnvironment *);

    static CurveFileType parseCurveFileType(const char *, const char *, const char *, IScriptEnvironment *);

    enum { iChild, iPmode, iDrawmode, iFile, iFtype };

    static const char *Name()
        { return "Gradation"; }
    static const char *Signature()
        { return "c[pmode]s[drawmode]s[file]s[ftype]s"; }
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
    auto &&dst = src->IsWritable() ? (PVideoFrame &&) src
                                   : (PVideoFrame &&) env->NewVideoFrameP(vi, &src);
    Run( *grd, vi.width, vi.height,
         (uint32_t *) src->GetReadPtr(), (uint32_t *) dst->GetWritePtr(),
         src->GetPitch(), dst->GetPitch() );
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

CurveFileType GradationFilter::parseCurveFileType(const char *filename, const char *ftype, const char *argName, IScriptEnvironment *env)
{
    CurveFileType type = parseEnum<CurveFileType>(ftype, argName, curveFileTypes, env);
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

AVSValue __cdecl GradationFilter::Create(AVSValue args, void *, IScriptEnvironment *env)
{
    auto &&grd = std::make_unique<Gradation>();
    Init(*grd);

    if (!args[iPmode].IsString())
        env->ThrowError("%s: Missing parameter 'pmode'", Name());
    if (!args[iFile].IsString())
        env->ThrowError("%s: Missing parameter 'file'", Name());

    auto &&child = args[iChild].AsClip();
    auto &vi = child->GetVideoInfo();
    if (!vi.IsRGB32())
        env->ThrowError("%s: Source must be RGB32", Name());

    grd->process = parseEnum<ProcessingMode>(args[iPmode].AsString(), "pmode", processingModes, env);
    DrawMode drawMode = parseEnum<DrawMode>(args[iDrawmode].AsString("spline"), "drawmode", drawModes, env);
    CurveFileType type = parseCurveFileType(args[iFile].AsString(), args[iFtype].AsString("auto"), "ftype", env);
    if (!ImportCurve(*grd, args[iFile].AsString(), type, drawMode))
        env->ThrowError("%s: Cannot open file '%s'", Name(), args[iFile].AsString());

    PreCalcLut(*grd);

    return new GradationFilter(child, grd);
}

const AVS_Linkage *AVS_linkage = 0;

extern "C" __declspec(dllexport) const char* __cdecl AvisynthPluginInit3(IScriptEnvironment* env, AVS_Linkage* vectors)
{
    AVS_linkage = vectors;
    GradationFilter::Register(env);
    return 0;
}
