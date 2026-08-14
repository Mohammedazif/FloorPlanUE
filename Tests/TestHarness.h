#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace FloorPlan::Testing
{
    using TestFunction = void (*)();

    struct TestCase
    {
        const char* Suite;
        const char* Name;
        TestFunction Run;
    };

    std::vector<TestCase>& Registry();

    struct Registrar
    {
        Registrar(const char* suite, const char* name, TestFunction run);
    };

    void RecordFailure(const char* file, int line, const std::string& message);

    /// Absolute path to a file in TestData, resolved from the executable location.
    std::string DataPath(const std::string& relative);

    std::string Describe(double value);

    bool NearlyEqual(double left, double right, double tolerance);

    int RunAll(int argc, char** argv);
}

#define FLOORPLAN_CONCAT_INNER(a, b) a##b
#define FLOORPLAN_CONCAT(a, b) FLOORPLAN_CONCAT_INNER(a, b)

#define FLOORPLAN_TEST(suite, name)                                                       \
    static void FLOORPLAN_CONCAT(suite##_##name##_body_, __LINE__)();                     \
    static const ::FloorPlan::Testing::Registrar FLOORPLAN_CONCAT(suite##_##name##_reg_,   \
                                                                 __LINE__)(               \
        #suite, #name, &FLOORPLAN_CONCAT(suite##_##name##_body_, __LINE__));              \
    static void FLOORPLAN_CONCAT(suite##_##name##_body_, __LINE__)()

#define CHECK(condition)                                                                  \
    do                                                                                    \
    {                                                                                     \
        if (!(condition))                                                                 \
        {                                                                                 \
            ::FloorPlan::Testing::RecordFailure(__FILE__, __LINE__, #condition);           \
        }                                                                                 \
    } while (false)

#define CHECK_MESSAGE(condition, message)                                                 \
    do                                                                                    \
    {                                                                                     \
        if (!(condition))                                                                 \
        {                                                                                 \
            ::FloorPlan::Testing::RecordFailure(__FILE__, __LINE__,                        \
                                                std::string(#condition) + " | " +          \
                                                    std::string(message));                 \
        }                                                                                 \
    } while (false)

#define CHECK_EQUAL(actual, expected)                                                     \
    do                                                                                    \
    {                                                                                     \
        const auto checkActual = (actual);                                                \
        const auto checkExpected = (expected);                                            \
        if (!(checkActual == checkExpected))                                              \
        {                                                                                 \
            ::FloorPlan::Testing::RecordFailure(                                          \
                __FILE__, __LINE__,                                                       \
                std::string(#actual) + " == " + std::string(#expected));                  \
        }                                                                                 \
    } while (false)

#define CHECK_NEAR(actual, expected, tolerance)                                           \
    do                                                                                    \
    {                                                                                     \
        const double checkActual = static_cast<double>(actual);                           \
        const double checkExpected = static_cast<double>(expected);                       \
        if (!::FloorPlan::Testing::NearlyEqual(checkActual, checkExpected, (tolerance)))  \
        {                                                                                 \
            ::FloorPlan::Testing::RecordFailure(                                          \
                __FILE__, __LINE__,                                                       \
                std::string(#actual) + " = " +                                            \
                    ::FloorPlan::Testing::Describe(checkActual) + ", expected " +         \
                    ::FloorPlan::Testing::Describe(checkExpected));                       \
        }                                                                                 \
    } while (false)
