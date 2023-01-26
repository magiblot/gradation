#include "test.h"

#include "gradation.h"

std::ostream &operator<<(std::ostream &os, const RGB<uint8_t> &input)
{
    os << "{"
       << (int) input.r << ", "
       << (int) input.g << ", "
       << (int) input.b << "}" << std::endl;
    return os;
}

std::ostream &operator<<(std::ostream &os, const RGB<double> &input)
{
    os << "{"
       << input.r << ", "
       << input.g << ", "
       << input.b << "}" << std::endl;
    return os;
}

template <class T>
bool operator==(const RGB<T> &a, const RGB<T> &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

struct Curve
{
    Channel channel;
    size_t count;
    uint8_t points[maxPoints][2];
    DrawMode drawMode {DRAWMODE_LINEAR};
};

TEST(Gradation, ShouldProcessRGB)
{
    static constexpr Curve curves[] =
    {
        {CHANNEL_RGB, 2, {{0, 0}, {252, 63}}}, // y = x/4.
    };
    static constexpr TestCase<RGB<double>> testCases[] =
    {
        {{0, 0, 0}, {0, 0, 0}},
        {{1, 0, 0}, {0.25, 0, 0}},
        {{2, 1, 0}, {0.5, 0.25, 0}},
        {{4, 2, 3}, {1, 0.5, 0.75}},
        {{6, 4, 5}, {1.5, 1, 1.25}},
    };

    for (auto &testCase : testCases)
    {
        Gradation grd;
        Init(grd, false);
        for (auto &curve : curves)
            ImportPoints(grd, curve.channel, curve.points, curve.count, curve.drawMode);
        auto &in = testCase.input;
        auto actual = procModeRgb::processDouble(grd, in.r, in.g, in.b);
        expectMatchingResult(actual, testCase);
    }
}

TEST(Gradation, ShouldProcessRGBInt)
{
    static constexpr Curve curves[] =
    {
        {CHANNEL_RGB, 2, {{0, 0}, {252, 63}}}, // y = x/4
    };
    static constexpr TestCase<RGB<uint8_t>> testCases[] =
    {
        {{0, 0, 0}, {0, 0, 0}},
        {{1, 0, 0}, {0, 0, 0}},
        {{2, 1, 0}, {1, 0, 0}},
        {{4, 2, 3}, {1, 1, 1}},
        {{6, 4, 5}, {2, 1, 1}},
    };

    for (auto &testCase : testCases)
    {
        Gradation grd;
        Init(grd, false);
        for (auto &curve : curves)
            ImportPoints(grd, curve.channel, curve.points, curve.count, curve.drawMode);
        auto &in = testCase.input;
        auto actual = procModeRgb::processInt(grd, in.r, in.g, in.b);
        expectMatchingResult(actual, testCase);
    }
}

TEST(Gradation, ShouldProcessFull)
{
    static constexpr Curve curves[] =
    {
        {CHANNEL_RGB, 2, {{0, 0}, {252, 63}}}, // y = x/4.
        {CHANNEL_RED, 2, {{0, 0}, {254, 127}}}, // y = x/2.
        {CHANNEL_GREEN, 2, {{0, 0}, {255, 255}}}, // y = x.
        {CHANNEL_BLUE, 2, {{0, 0}, {127, 254}}}, // y = x*2.
    };
    static constexpr TestCase<RGB<double>> testCases[] =
    {
        {{0, 0, 0}, {0, 0, 0}},
        {{1, 0, 0}, {0.125, 0, 0}},
        {{2, 1, 0}, {0.25, 0.25, 0}},
        {{4, 2, 3}, {0.5, 0.5, 1.5}},
        {{6, 4, 5}, {0.75, 1, 2.5}},
    };

    for (auto &testCase : testCases)
    {
        Gradation grd;
        Init(grd, false);
        for (auto &curve : curves)
            ImportPoints(grd, curve.channel, curve.points, curve.count, curve.drawMode);
        auto &in = testCase.input;
        auto actual = procModeFull::processDouble(grd, in.r, in.g, in.b);
        expectMatchingResult(actual, testCase);
    }
}

TEST(Gradation, ShouldProcessFullInt)
{
    static constexpr Curve curves[] =
    {
        {CHANNEL_RGB, 2, {{0, 0}, {252, 63}}}, // y = x/4.
        {CHANNEL_RED, 2, {{0, 0}, {254, 127}}}, // y = x/2.
        {CHANNEL_GREEN, 2, {{0, 0}, {255, 255}}}, // y = x.
        {CHANNEL_BLUE, 2, {{0, 0}, {127, 254}}}, // y = x*2.
    };
    static constexpr TestCase<RGB<uint8_t>> testCases[] =
    {
        {{0, 0, 0}, {0, 0, 0}},
        {{1, 0, 0}, {0, 0, 0}},
        {{2, 1, 0}, {0, 0, 0}},
        {{4, 2, 3}, {1, 1, 2}},
        {{6, 4, 5}, {1, 1, 3}},
    };

    for (auto &testCase : testCases)
    {
        Gradation grd;
        Init(grd, false);
        for (auto &curve : curves)
            ImportPoints(grd, curve.channel, curve.points, curve.count, curve.drawMode);
        auto &in = testCase.input;
        auto actual = procModeFull::processInt(grd, in.r, in.g, in.b);
        expectMatchingResult(actual, testCase);
    }
}
