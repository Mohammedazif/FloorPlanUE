#include "TestHarness.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace FloorPlan::Testing
{
    namespace
    {
        std::vector<std::string>& CurrentFailures()
        {
            static std::vector<std::string> failures;
            return failures;
        }

        std::filesystem::path& DataRoot()
        {
            static std::filesystem::path root;
            return root;
        }

        std::filesystem::path LocateDataRoot(const std::filesystem::path& start)
        {
            std::error_code error;
            std::filesystem::path current = std::filesystem::absolute(start, error);
            for (int depth = 0; depth < 8 && !current.empty(); ++depth)
            {
                const std::filesystem::path candidate = current / "TestData";
                if (std::filesystem::is_directory(candidate, error))
                {
                    return candidate;
                }
                if (!current.has_parent_path() || current.parent_path() == current)
                {
                    break;
                }
                current = current.parent_path();
            }
            return {};
        }
    }

    std::vector<TestCase>& Registry()
    {
        static std::vector<TestCase> registry;
        return registry;
    }

    Registrar::Registrar(const char* suite, const char* name, TestFunction run)
    {
        Registry().push_back(TestCase{suite, name, run});
    }

    void RecordFailure(const char* file, int line, const std::string& message)
    {
        std::ostringstream stream;
        const char* trimmed = std::strrchr(file, '\\');
        if (trimmed == nullptr)
        {
            trimmed = std::strrchr(file, '/');
        }
        stream << (trimmed != nullptr ? trimmed + 1 : file) << ':' << line << "  " << message;
        CurrentFailures().push_back(stream.str());
    }

    std::string DataPath(const std::string& relative)
    {
        return (DataRoot() / relative).string();
    }

    std::string Describe(double value)
    {
        std::ostringstream stream;
        stream.precision(15);
        stream << value;
        return stream.str();
    }

    bool NearlyEqual(double left, double right, double tolerance)
    {
        if (std::isnan(left) || std::isnan(right))
        {
            return false;
        }
        return std::fabs(left - right) <= tolerance;
    }

    int RunAll(int argc, char** argv)
    {
        DataRoot() = LocateDataRoot(argc > 0 ? std::filesystem::path(argv[0]).parent_path()
                                             : std::filesystem::current_path());
        if (DataRoot().empty())
        {
            DataRoot() = LocateDataRoot(std::filesystem::current_path());
        }
        if (DataRoot().empty())
        {
            std::cout << "TestData directory not found; data-driven tests will fail.\n";
        }

        const std::string filter = argc > 1 ? argv[1] : std::string{};

        auto& registry = Registry();
        std::stable_sort(registry.begin(), registry.end(),
                         [](const TestCase& left, const TestCase& right) {
                             const int suite = std::strcmp(left.Suite, right.Suite);
                             return suite != 0 ? suite < 0
                                               : std::strcmp(left.Name, right.Name) < 0;
                         });

        std::size_t passed = 0;
        std::size_t failed = 0;
        std::size_t skipped = 0;
        const char* suite = nullptr;

        for (const TestCase& test : registry)
        {
            const std::string label = std::string(test.Suite) + "." + test.Name;
            if (!filter.empty() && label.find(filter) == std::string::npos)
            {
                ++skipped;
                continue;
            }
            if (suite == nullptr || std::strcmp(suite, test.Suite) != 0)
            {
                suite = test.Suite;
                std::cout << "\n" << suite << "\n";
            }

            CurrentFailures().clear();
            test.Run();

            if (CurrentFailures().empty())
            {
                ++passed;
                std::cout << "  pass  " << test.Name << "\n";
            }
            else
            {
                ++failed;
                std::cout << "  FAIL  " << test.Name << "\n";
                for (const std::string& failure : CurrentFailures())
                {
                    std::cout << "          " << failure << "\n";
                }
            }
        }

        std::cout << "\n" << passed << " passed, " << failed << " failed";
        if (skipped > 0)
        {
            std::cout << ", " << skipped << " filtered out";
        }
        std::cout << "  (" << registry.size() << " registered)\n";
        return failed == 0 ? 0 : 1;
    }
}

int main(int argc, char** argv)
{
    return FloorPlan::Testing::RunAll(argc, argv);
}
