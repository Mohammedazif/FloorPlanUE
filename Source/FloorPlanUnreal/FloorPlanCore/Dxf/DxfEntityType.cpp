#include "Dxf/DxfEntityType.h"

namespace FloorPlan::Dxf
{
    const char* ToString(DxfEntityType type)
    {
        switch (type)
        {
        case DxfEntityType::Unknown: return "Unknown";
        case DxfEntityType::Line: return "LINE";
        case DxfEntityType::LwPolyline: return "LWPOLYLINE";
        case DxfEntityType::Polyline: return "POLYLINE";
        case DxfEntityType::Arc: return "ARC";
        case DxfEntityType::Circle: return "CIRCLE";
        case DxfEntityType::Insert: return "INSERT";
        case DxfEntityType::Text: return "TEXT";
        case DxfEntityType::MText: return "MTEXT";
        case DxfEntityType::Dimension: return "DIMENSION";
        case DxfEntityType::Hatch: return "HATCH";
        }
        return "Unknown";
    }
}
