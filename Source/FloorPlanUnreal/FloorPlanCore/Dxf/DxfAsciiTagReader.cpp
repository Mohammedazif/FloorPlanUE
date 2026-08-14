#include "Dxf/DxfAsciiTagReader.h"

#include "FloorPlanLimits.h"

#include <charconv>
#include <cmath>
#include <limits>

namespace FloorPlan::Dxf
{
    namespace
    {
        bool IsSpace(char value)
        {
            return value == ' ' || value == '\t' || value == '\v' || value == '\f' ||
                   value == '\r';
        }

        std::string Trim(const std::string& text)
        {
            std::size_t first = 0;
            while (first < text.size() && IsSpace(text[first]))
            {
                ++first;
            }
            std::size_t last = text.size();
            while (last > first && IsSpace(text[last - 1]))
            {
                --last;
            }
            return text.substr(first, last - first);
        }

        bool HexDigit(char value, std::uint8_t& out)
        {
            if (value >= '0' && value <= '9')
            {
                out = static_cast<std::uint8_t>(value - '0');
                return true;
            }
            if (value >= 'a' && value <= 'f')
            {
                out = static_cast<std::uint8_t>(value - 'a' + 10);
                return true;
            }
            if (value >= 'A' && value <= 'F')
            {
                out = static_cast<std::uint8_t>(value - 'A' + 10);
                return true;
            }
            return false;
        }
    }

    DxfAsciiTagReader::DxfAsciiTagReader(const std::uint8_t* data, std::size_t size)
        : Data(data), Size(size)
    {
    }

    bool DxfAsciiTagReader::ReadLine(std::string& out)
    {
        if (Offset >= Size)
        {
            return false;
        }
        const std::size_t start = Offset;
        while (Offset < Size && Data[Offset] != '\n')
        {
            ++Offset;
            if (Offset - start > Limits::MaxGroupCodeLineBytes)
            {
                return false;
            }
        }
        std::size_t end = Offset;
        if (end > start && Data[end - 1] == '\r')
        {
            --end;
        }
        out.assign(reinterpret_cast<const char*>(Data + start), end - start);
        if (Offset < Size)
        {
            ++Offset;
        }
        ++Line;
        return true;
    }

    bool DxfAsciiTagReader::ConvertReal(DxfTag& tag, const std::string& raw)
    {
        const std::string text = Trim(raw);
        if (text.empty())
        {
            return Fail(DiagnosticCode::MalformedReal, "empty value", tag.LineNumber,
                        tag.ByteOffset);
        }
        double value = 0.0;
        const char* begin = text.data();
        const char* end = begin + text.size();
        const std::from_chars_result result = std::from_chars(begin, end, value);
        if (result.ec != std::errc{} || result.ptr != end)
        {
            return Fail(DiagnosticCode::MalformedReal, text, tag.LineNumber, tag.ByteOffset);
        }
        if (!std::isfinite(value))
        {
            return Fail(DiagnosticCode::NonFiniteValue, text, tag.LineNumber, tag.ByteOffset);
        }
        tag.Real = value;
        tag.Integer = 0;
        return true;
    }

    bool DxfAsciiTagReader::ConvertInteger(DxfTag& tag, const std::string& raw)
    {
        const std::string text = Trim(raw);
        if (text.empty())
        {
            return Fail(DiagnosticCode::MalformedInteger, "empty value", tag.LineNumber,
                        tag.ByteOffset);
        }
        std::int64_t value = 0;
        const char* begin = text.data();
        const char* end = begin + text.size();
        const std::from_chars_result result = std::from_chars(begin, end, value);
        if (result.ec != std::errc{} || result.ptr != end)
        {
            return Fail(DiagnosticCode::MalformedInteger, text, tag.LineNumber, tag.ByteOffset);
        }
        tag.Integer = value;
        tag.Real = static_cast<double>(value);
        return true;
    }

    bool DxfAsciiTagReader::ConvertBinaryChunk(DxfTag& tag, const std::string& raw)
    {
        const std::string text = Trim(raw);
        if ((text.size() % 2) != 0)
        {
            return Fail(DiagnosticCode::MalformedInteger, "odd hex length", tag.LineNumber,
                        tag.ByteOffset);
        }
        if (text.size() / 2 > Limits::BinaryChunkLengthMax)
        {
            return Fail(DiagnosticCode::BinaryChunkTooLong, text.substr(0, 32), tag.LineNumber,
                        tag.ByteOffset);
        }
        tag.Binary.clear();
        tag.Binary.reserve(text.size() / 2);
        for (std::size_t index = 0; index + 1 < text.size(); index += 2)
        {
            std::uint8_t high = 0;
            std::uint8_t low = 0;
            if (!HexDigit(text[index], high) || !HexDigit(text[index + 1], low))
            {
                return Fail(DiagnosticCode::MalformedInteger, "bad hex digit", tag.LineNumber,
                            tag.ByteOffset);
            }
            tag.Binary.push_back(static_cast<std::uint8_t>((high << 4) | low));
        }
        return true;
    }

    bool DxfAsciiTagReader::ConvertValue(DxfTag& tag, const std::string& raw)
    {
        switch (tag.Type)
        {
        case DxfValueType::Real:
            return ConvertReal(tag, raw);
        case DxfValueType::Int16:
        case DxfValueType::Int32:
        case DxfValueType::Int64:
        case DxfValueType::Boolean:
            return ConvertInteger(tag, raw);
        case DxfValueType::BinaryChunk:
            return ConvertBinaryChunk(tag, raw);
        case DxfValueType::Text:
            break;
        }
        if (raw.size() > Limits::MaxStringValueBytes)
        {
            return Fail(DiagnosticCode::StringTooLong, std::to_string(raw.size()) + " bytes",
                        tag.LineNumber, tag.ByteOffset);
        }
        tag.Text = raw;
        return true;
    }

    bool DxfAsciiTagReader::Next(DxfTag& tag)
    {
        if (Failed() || EndOfFileSeen)
        {
            return false;
        }

        const std::size_t codeOffset = Offset;
        std::string codeLine;
        if (!ReadLine(codeLine))
        {
            if (Offset < Size)
            {
                return Fail(DiagnosticCode::LineTooLong, "group code line", Line + 1, codeOffset);
            }
            return Fail(DiagnosticCode::MissingEndOfFileMarker, "input ended before (0, EOF)",
                        Line, codeOffset);
        }

        const std::size_t codeLineNumber = Line;
        const std::string codeText = Trim(codeLine);
        if (codeText.empty())
        {
            return Fail(DiagnosticCode::InvalidGroupCode, "empty group code line",
                        codeLineNumber, codeOffset);
        }

        int code = 0;
        const char* begin = codeText.data();
        const char* end = begin + codeText.size();
        const std::from_chars_result parsed = std::from_chars(begin, end, code);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
        {
            return Fail(DiagnosticCode::InvalidGroupCode, codeText, codeLineNumber, codeOffset);
        }
        if (code < Limits::LowestValidGroupCode || code > Limits::HighestValidGroupCode)
        {
            return Fail(DiagnosticCode::GroupCodeOutOfRange, codeText, codeLineNumber,
                        codeOffset);
        }

        const std::size_t valueOffset = Offset;
        std::string valueLine;
        if (!ReadLine(valueLine))
        {
            if (Offset < Size)
            {
                return Fail(DiagnosticCode::LineTooLong, "value line", Line + 1, valueOffset);
            }
            return Fail(DiagnosticCode::UnexpectedEndOfInput, "group code without a value",
                        codeLineNumber, valueOffset);
        }

        tag = DxfTag{};
        tag.Code = code;
        tag.Type = ClassifyGroupCode(code);
        tag.LineNumber = codeLineNumber;
        tag.ByteOffset = codeOffset;

        if (!ConvertValue(tag, valueLine))
        {
            return false;
        }

        if (++TagsEmitted > Limits::MaxTagCount)
        {
            return Fail(DiagnosticCode::TagLimitExceeded, std::to_string(TagsEmitted),
                        codeLineNumber, codeOffset);
        }

        if (tag.IsStartOf("EOF"))
        {
            EndOfFileSeen = true;
        }
        return true;
    }
}
