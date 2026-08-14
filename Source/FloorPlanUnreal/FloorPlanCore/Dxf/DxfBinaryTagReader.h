#pragma once

#include "Dxf/DxfTagReader.h"

#include <cstdint>

namespace FloorPlan::Dxf
{
    /// Reads tags from a binary DXF buffer, after its 22-byte sentinel.
    class DxfBinaryTagReader final : public DxfTagReader
    {
    public:
        DxfBinaryTagReader(const std::uint8_t* data, std::size_t size);

        bool Next(DxfTag& tag) override;

        bool ConsumedEntireInput() const override { return Offset == Size; }

        /// 1 for R12 and earlier, 2 for R13 and later. Autodesk documents R14; the files say R13.
        int GroupCodeWidth() const { return CodeWidth; }

        static bool HasSentinel(const std::uint8_t* data, std::size_t size);

    private:
        bool Require(std::size_t bytes, std::size_t at);
        bool ReadCode(int& code);
        bool ReadValue(DxfTag& tag);
        bool ReadText(DxfTag& tag);
        bool ReadChunk(DxfTag& tag);

        const std::uint8_t* Data;
        std::size_t Size;
        std::size_t Offset = 0;
        std::size_t TagsEmitted = 0;
        int CodeWidth = 1;
    };
}
