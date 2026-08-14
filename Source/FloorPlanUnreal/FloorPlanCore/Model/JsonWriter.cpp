#include "Model/JsonWriter.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace FloorPlan::Model
{
    std::string JsonWriter::Number(double value)
    {
        if (!std::isfinite(value))
        {
            return "0";
        }
        char buffer[64] = {};
        for (int digits = 15; digits <= 17; ++digits)
        {
            std::snprintf(buffer, sizeof(buffer), "%.*g", digits, value);
            double parsed = 0.0;
            const char* end = buffer + std::strlen(buffer);
            const std::from_chars_result read = std::from_chars(buffer, end, parsed);
            if (read.ec == std::errc{} && parsed == value)
            {
                break;
            }
        }
        return std::string(buffer);
    }

    void JsonWriter::AppendText(std::string& out, const std::string& text)
    {
        out += '"';
        for (const char character : text)
        {
            const unsigned char code = static_cast<unsigned char>(character);
            switch (character)
            {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (code < 0x20)
                {
                    char escape[8] = {};
                    std::snprintf(escape, sizeof(escape), "\\u%04x", code);
                    out += escape;
                }
                else
                {
                    out += character;
                }
                break;
            }
        }
        out += '"';
    }

    void JsonWriter::AppendIndent(std::string& out, int depth)
    {
        out.append(static_cast<std::size_t>(depth) * 2, ' ');
    }
}
