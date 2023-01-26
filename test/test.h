#ifndef GRADATION_TEST_H
#define GRADATION_TEST_H

#include <gtest/gtest.h>

template <class T1, class T2 = T1>
struct TestCase
{
    T1 input;
    T2 result;
};

template <class T1, class T2, class T3>
inline void expectMatchingResult(const T1 &actual, const TestCase<T2, T3> &testCase)
{
    auto &expected = testCase.result;
    EXPECT_EQ(actual, expected) << "With test input:\n" << testCase.input;
}

#endif // GRADATION_TEST_H
