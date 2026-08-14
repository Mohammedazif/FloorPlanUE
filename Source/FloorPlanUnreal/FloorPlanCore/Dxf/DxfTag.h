#pragma once

#include "Dxf/DxfValueType.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace FloorPlan::Dxf
{
    /// One DXF group-code/value pair, with the value already converted to its declared type.
    struct DxfTag
    {
        int Code = 0;
        DxfValueType Type = DxfValueType::Text;
        std::string Text;
        double Real = 0.0;
        std::int64_t Integer = 0;
        std::vector<std::uint8_t> Binary;
        std::size_t LineNumber = 0;
        std::size_t ByteOffset = 0;

        bool IsStart() const { return Code == 0; }

        bool IsStartOf(const char* name) const { return Code == 0 && Text == name; }
    };
}
