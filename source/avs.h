#ifndef GRADATION_AVS_H
#define GRADATION_AVS_H

#include <type_traits>
#include <avisynth.h>
#include "gradation.h"
#include "util.h"

static const int planesRGB[4] {PLANAR_B, PLANAR_G, PLANAR_R, PLANAR_A};

template<int bpc>
struct PixelTraits
{
    using pixel_t =
        std::conditional_t<
            bpc == 8,
            uint8_t,
            std::conditional_t<
                bpc < 32,
                uint16_t,
                float
        >   >;

    static constexpr pixel_t maxValue()
        { return (bpc == 32) ? 1 : (1LL << bpc) - 1; }
};


using FrameProcesser = void(const Gradation &grd, int width, int height, int pixel_type, const PVideoFrame &src, const PVideoFrame &dst);
using GradationProcesser = RGB<double>(const Gradation &grd, double r, double g, double b);

template <GradationProcesser &process, int bpc>
inline void applyToFrame(const Gradation &grd, int width, int height, int pixel_type, const PVideoFrame &src, const PVideoFrame &dst)
// Pre: clip is RGB(A).
{
    using pixel_t = typename PixelTraits<bpc>::pixel_t;
    enum { iB, iG, iR, iA };

    bool hasAlpha = pixel_type & VideoInfo::CS_RGBA_TYPE;
    bool isPacked = pixel_type & VideoInfo::CS_INTERLEAVED;
    int nComponents = 3 + hasAlpha;
    int packSize = isPacked ? (hasAlpha ? 4 : 3) : 1;

    const BYTE *srcp[4];
    BYTE *dstp[4];
    for (int p = 0; p < nComponents; ++p)
    {
        if (isPacked)
        {
            srcp[p] = src->GetReadPtr() + p*sizeof(pixel_t);
            dstp[p] = dst->GetWritePtr() + p*sizeof(pixel_t);
        }
        else
        {
            srcp[p] = src->GetReadPtr(planesRGB[p]);
            dstp[p] = dst->GetWritePtr(planesRGB[p]);
        }
    }
    int srcPitch = src->GetPitch(),
        dstPitch = dst->GetPitch();

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < packSize*width; x += packSize)
        {
            constexpr pixel_t maxValue = PixelTraits<bpc>::maxValue();
            constexpr double multiplier = 255.0/maxValue;
            constexpr bool isInt = bpc < 32;
            RGB<double> in {
                clamp<pixel_t>(((pixel_t *) srcp[iR])[x], 0, maxValue)*multiplier,
                clamp<pixel_t>(((pixel_t *) srcp[iG])[x], 0, maxValue)*multiplier,
                clamp<pixel_t>(((pixel_t *) srcp[iB])[x], 0, maxValue)*multiplier,
            };
            RGB<double> out = process(grd, in.r, in.g, in.b);
            ((pixel_t *) dstp[iR])[x] = pixel_t(out.r/multiplier + isInt*0.5);
            ((pixel_t *) dstp[iG])[x] = pixel_t(out.g/multiplier + isInt*0.5);
            ((pixel_t *) dstp[iB])[x] = pixel_t(out.b/multiplier + isInt*0.5);
            if (hasAlpha)
                ((pixel_t *) dstp[iA])[x] = ((pixel_t *) srcp[iA])[x];
        }
        for (int p = 0; p < nComponents; ++p)
        {
            srcp[p] += srcPitch;
            dstp[p] += dstPitch;
        }
    }
}

#endif // GRADATION_AVS_H
