#pragma once

namespace FloorPlan::Dxf
{
    enum class DxfValueType
    {
        Text,
        Real,
        Int16,
        Int32,
        Int64,
        Boolean,
        BinaryChunk
    };

    /// Maps a group code to the value type that follows it, per the DXF group code ranges.
    constexpr DxfValueType ClassifyGroupCode(int code)
    {
        if ((code >= 10 && code <= 59) || (code >= 110 && code <= 149) ||
            (code >= 210 && code <= 239) || (code >= 460 && code <= 469) ||
            (code >= 1010 && code <= 1059))
        {
            return DxfValueType::Real;
        }
        if ((code >= 60 && code <= 79) || (code >= 170 && code <= 179) ||
            (code >= 270 && code <= 289) || (code >= 370 && code <= 389) ||
            (code >= 400 && code <= 409) || (code >= 1060 && code <= 1070))
        {
            return DxfValueType::Int16;
        }
        if ((code >= 90 && code <= 99) || (code >= 420 && code <= 429) ||
            (code >= 440 && code <= 459) || code == 1071)
        {
            return DxfValueType::Int32;
        }
        if (code >= 160 && code <= 169)
        {
            return DxfValueType::Int64;
        }
        if (code >= 290 && code <= 299)
        {
            return DxfValueType::Boolean;
        }
        if ((code >= 310 && code <= 319) || code == 1004)
        {
            return DxfValueType::BinaryChunk;
        }
        return DxfValueType::Text;
    }
}
