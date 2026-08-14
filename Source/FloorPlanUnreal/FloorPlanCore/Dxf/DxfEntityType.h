#pragma once

namespace FloorPlan::Dxf
{
    enum class DxfEntityType
    {
        Unknown,
        Line,
        LwPolyline,
        Polyline,
        Arc,
        Circle,
        Insert,
        Text,
        MText,
        Dimension,
        Hatch
    };

    const char* ToString(DxfEntityType type);
}
