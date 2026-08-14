#pragma once

#include "Dxf/DxfTagReader.h"

#include <cstdint>
#include <string>

namespace FloorPlan::Dxf
{
    /// Reads tags from an ASCII DXF buffer of group-code and value line pairs.
    class DxfAsciiTagReader final : public DxfTagReader
    {
    public:
        DxfAsciiTagReader(const std::uint8_t* data, std::size_t size);

        bool Next(DxfTag& tag) override;

        bool ConsumedEntireInput() const override { return Offset >= Size; }

    private:
        bool ReadLine(std::string& out);
        bool ConvertValue(DxfTag& tag, const std::string& raw);
        bool ConvertReal(DxfTag& tag, const std::string& raw);
        bool ConvertInteger(DxfTag& tag, const std::string& raw);
        bool ConvertBinaryChunk(DxfTag& tag, const std::string& raw);

        const std::uint8_t* Data;
        std::size_t Size;
        std::size_t Offset = 0;
        std::size_t Line = 0;
        std::size_t TagsEmitted = 0;
    };
}
