#include <avisynth.h>
#include "gradation.h"

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

const AVS_Linkage *AVS_linkage = 0;

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

    static ProcessingMode parseProcessingMode(int, IScriptEnvironment *);
    static DrawMode parseDrawMode(int, IScriptEnvironment *);
    static CurveFileType parseCurveFileType(const char *, int, IScriptEnvironment *);

    enum { iChild, iPmode, iDrawmode, iFile, iFtype };

    static const char *Name()
        { return "Gradation"; }
    static const char *Signature()
        { return "c[pmode]i[drawmode]i[file]s[ftype]i"; }
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
    auto &&dst = std::move(src->IsWritable() ? (env->MakeWritable(&src), src)
                                             : env->NewVideoFrameP(vi, &src));
    Run( *grd, vi.width, vi.height,
         (uint32_t *) src->GetReadPtr(), (uint32_t *) dst->GetWritePtr(),
         src->GetPitch(), dst->GetPitch() );
    return dst;
}

ProcessingMode GradationFilter::parseProcessingMode(int p, IScriptEnvironment *env)
{
    if (p < PROCMODE_RGB || PROCMODE_LAB < p)
        env->ThrowError("%s: Invalid 'pmode' %d. Expected a value in the range [0..8]", Name(), p);
    return ProcessingMode(p);
}

DrawMode GradationFilter::parseDrawMode(int d, IScriptEnvironment *env)
{
    if (d < DRAWMODE_PEN || DRAWMODE_GAMMA < d)
        env->ThrowError("%s: Invalid 'drawmode' %d. Expected a value in the range [0..3]", Name(), d);
    return DrawMode(d);
}

CurveFileType GradationFilter::parseCurveFileType(const char *filename, int type, IScriptEnvironment *env)
{
    if (0 <= type)
    {
        if (type < FILETYPE_AMP || FILETYPE_SMARTCURVE_HSV < type)
            env->ThrowError("%s: Invalid 'ftype' %d. Expected a value in the range [1..6]", Name(), type);
        return CurveFileType(type);
    }
    size_t length = strlen(filename);
    if (length >= 4)
    {
        static const struct { const char *ext; CurveFileType type; } formats[] =
        {
            {".amp", FILETYPE_AMP},
            {".acv", FILETYPE_ACV},
            {".csv", FILETYPE_CSV},
            {".crv", FILETYPE_CRV},
            {".map", FILETYPE_MAP},
        };
        const char *ext = filename + length - 4;
        for (auto &f : formats)
            if (stricmp(ext, f.ext) == 0)
                return f.type;
    }
    env->ThrowError("%s: Cannot determine type of file '%s'", Name(), filename);
    abort();
}

AVSValue __cdecl GradationFilter::Create(AVSValue args, void *, IScriptEnvironment *env)
{
    auto &&grd = std::make_unique<Gradation>();
    Init(*grd);

    if (!args[iPmode].IsInt())
        env->ThrowError("%s: Missing parameter 'pmode'", Name());
    if (!args[iFile].IsString())
        env->ThrowError("%s: Missing parameter 'file'", Name());

    auto &&child = args[iChild].AsClip();
    auto &vi = child->GetVideoInfo();
    if (!vi.IsRGB32())
        env->ThrowError("%s: Source must be RGB32", Name());

    grd->process = parseProcessingMode(args[iPmode].AsInt(), env);
    DrawMode drawMode = parseDrawMode(args[iDrawmode].AsInt(2), env);
    CurveFileType type = parseCurveFileType(args[iFile].AsString(), args[iFtype].AsInt(-1), env);
    if (!ImportCurve(*grd, args[iFile].AsString(), type, drawMode))
        env->ThrowError("%s: Cannot open file '%s'", Name(), args[iFile].AsString());

    PreCalcLut(*grd);

    return new GradationFilter(child, grd);
}

extern "C" __declspec(dllexport) const char* __cdecl AvisynthPluginInit3(IScriptEnvironment* env, AVS_Linkage* vectors)
{
    AVS_linkage = vectors;
    GradationFilter::Register(env);
    return 0;
}
