#pragma once

#include <string>

namespace FloorPlan::Model
{
    /// The text mechanics of writing JSON: escaping, indenting, and lossless numbers.
    class JsonWriter
    {
    public:
        /// Shortest form that reads back as the same double, so exports stay lossless.
        static std::string Number(double value);

        static void AppendText(std::string& out, const std::string& text);

        static void AppendIndent(std::string& out, int depth);
    };
}
