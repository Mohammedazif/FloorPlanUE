#include "Dxf/DxfBinaryTagReader.h"

#include "FloorPlanLimits.h"

#include <cmath>
#include <cstring>

namespace FloorPlan::Dxf
{
    namespace
    {
        constexpr char Sentinel[] = "AutoCAD Binary DXF\r\n\x1a";

        template <typename T>
        T LoadLittleEndian(const std::uint8_t* source)
        {
            T value{};
            std::memcpy(&value, source, sizeof(T));
            return value;
        }
    }

    bool DxfBinaryTagReader::HasSentinel(const std::uint8_t* data, std::size_t size)
    {
        if (data == nullptr || size < Limits::BinarySentinelBytes)
        {
            return false;
        }
        if (std::memcmp(data, Sentinel, Limits::BinarySentinelBytes - 1) != 0)
        {
            return false;
        }
        return data[Limits::BinarySentinelBytes - 1] == 0;
    }

    DxfBinaryTagReader::DxfBinaryTagReader(const std::uint8_t* data, std::size_t size)
        : Data(data), Size(size)
    {
        if (!HasSentinel(data, size))
        {
            Fail(DiagnosticCode::NotDxf, "missing binary DXF sentinel", 0, 0);
            return;
        }
        Offset = Limits::BinarySentinelBytes;
        CodeWidth = 1;
        if (Size >= Limits::BinaryNarrowGroupCodeProbeBytes && Data[Offset] == 0 &&
            Data[Offset + 1] == 0)
        {
            CodeWidth = 2;
        }
    }

    bool DxfBinaryTagReader::Require(std::size_t bytes, std::size_t at)
    {
        if (at > Size || bytes > Size - at)
        {
            return Fail(DiagnosticCode::UnexpectedEndOfInput,
                        "needed " + std::to_string(bytes) + " bytes", 0, at);
        }
        return true;
    }

    bool DxfBinaryTagReader::ReadCode(int& code)
    {
        if (!Require(static_cast<std::size_t>(CodeWidth), Offset))
        {
            return false;
        }
        if (CodeWidth == 1)
        {
            const std::uint8_t narrow = Data[Offset];
            Offset += 1;
            if (narrow == Limits::BinaryExtendedDataEscape)
            {
                if (!Require(2, Offset))
                {
                    return false;
                }
                code = static_cast<int>(LoadLittleEndian<std::uint16_t>(Data + Offset));
                Offset += 2;
                return true;
            }
            code = static_cast<int>(narrow);
            return true;
        }
        code = static_cast<int>(LoadLittleEndian<std::uint16_t>(Data + Offset));
        Offset += 2;
        return true;
    }

    bool DxfBinaryTagReader::ReadText(DxfTag& tag)
    {
        const void* terminator = std::memchr(Data + Offset, 0, Size - Offset);
        if (terminator == nullptr)
        {
            return Fail(DiagnosticCode::UnexpectedEndOfInput, "unterminated string", 0, Offset);
        }
        const std::size_t length =
            static_cast<std::size_t>(static_cast<const std::uint8_t*>(terminator) - (Data + Offset));
        if (length > Limits::MaxStringValueBytes)
        {
            return Fail(DiagnosticCode::StringTooLong, std::to_string(length) + " bytes", 0,
                        Offset);
        }
        tag.Text.assign(reinterpret_cast<const char*>(Data + Offset), length);
        Offset += length + 1;
        return true;
    }

    bool DxfBinaryTagReader::ReadChunk(DxfTag& tag)
    {
        if (!Require(1, Offset))
        {
            return false;
        }
        const std::size_t declared = Data[Offset];
        Offset += 1;
        if (declared > Limits::BinaryChunkLengthMax)
        {
            return Fail(DiagnosticCode::BinaryChunkTooLong, std::to_string(declared), 0, Offset);
        }
        if (!Require(declared, Offset))
        {
            return false;
        }
        tag.Binary.assign(Data + Offset, Data + Offset + declared);
        Offset += declared;
        return true;
    }

    bool DxfBinaryTagReader::ReadValue(DxfTag& tag)
    {
        switch (tag.Type)
        {
        case DxfValueType::Real:
        {
            if (!Require(sizeof(double), Offset))
            {
                return false;
            }
            const double value = LoadLittleEndian<double>(Data + Offset);
            Offset += sizeof(double);
            if (!std::isfinite(value))
            {
                return Fail(DiagnosticCode::NonFiniteValue, "non-finite double", 0, Offset);
            }
            tag.Real = value;
            return true;
        }
        case DxfValueType::Int16:
        {
            if (!Require(sizeof(std::int16_t), Offset))
            {
                return false;
            }
            tag.Integer = LoadLittleEndian<std::int16_t>(Data + Offset);
            Offset += sizeof(std::int16_t);
            tag.Real = static_cast<double>(tag.Integer);
            return true;
        }
        case DxfValueType::Int32:
        {
            if (!Require(sizeof(std::int32_t), Offset))
            {
                return false;
            }
            tag.Integer = LoadLittleEndian<std::int32_t>(Data + Offset);
            Offset += sizeof(std::int32_t);
            tag.Real = static_cast<double>(tag.Integer);
            return true;
        }
        case DxfValueType::Int64:
        {
            if (!Require(sizeof(std::int64_t), Offset))
            {
                return false;
            }
            tag.Integer = LoadLittleEndian<std::int64_t>(Data + Offset);
            Offset += sizeof(std::int64_t);
            tag.Real = static_cast<double>(tag.Integer);
            return true;
        }
        case DxfValueType::Boolean:
        {
            if (!Require(1, Offset))
            {
                return false;
            }
            tag.Integer = Data[Offset];
            Offset += 1;
            tag.Real = static_cast<double>(tag.Integer);
            return true;
        }
        case DxfValueType::BinaryChunk:
            return ReadChunk(tag);
        case DxfValueType::Text:
            return ReadText(tag);
        }
        return Fail(DiagnosticCode::UnsupportedBinaryEncoding, "unknown value type", 0, Offset);
    }

    bool DxfBinaryTagReader::Next(DxfTag& tag)
    {
        if (Failed() || EndOfFileSeen)
        {
            return false;
        }
        if (Offset >= Size)
        {
            return Fail(DiagnosticCode::MissingEndOfFileMarker, "input ended before (0, EOF)", 0,
                        Offset);
        }

        const std::size_t start = Offset;
        int code = 0;
        if (!ReadCode(code))
        {
            return false;
        }
        if (code < Limits::LowestValidGroupCode || code > Limits::HighestValidGroupCode)
        {
            return Fail(DiagnosticCode::GroupCodeOutOfRange, std::to_string(code), 0, start);
        }

        tag = DxfTag{};
        tag.Code = code;
        tag.Type = ClassifyGroupCode(code);
        tag.ByteOffset = start;

        if (!ReadValue(tag))
        {
            return false;
        }

        if (++TagsEmitted > Limits::MaxTagCount)
        {
            return Fail(DiagnosticCode::TagLimitExceeded, std::to_string(TagsEmitted), 0, start);
        }

        if (tag.IsStartOf("EOF"))
        {
            EndOfFileSeen = true;
        }
        return true;
    }
}
