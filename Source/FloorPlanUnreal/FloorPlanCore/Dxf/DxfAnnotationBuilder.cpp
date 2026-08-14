#include "Dxf/DxfAnnotationBuilder.h"

#include "Dxf/DxfEntityBuilder.h"
#include "FloorPlanLimits.h"

#include <cstdint>
#include <string>

namespace FloorPlan::Dxf
{
    namespace
    {
        constexpr std::int64_t DimensionFlagMask = 0xff;
    }

    bool DxfAnnotationBuilder::ApplyDimensionTag(DxfEntity& entity, const DxfTag& tag,
                                              Diagnostic& diagnostic)
    {
        switch (tag.Code)
        {
        case 1:
            entity.Text = tag.Text;
            return true;
        case 2:
            entity.BlockName = tag.Text;
            return true;
        case 10:
            if (!DxfEntityBuilder::CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.CenterX = tag.Real;
            return true;
        case 20:
            if (!DxfEntityBuilder::CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.CenterY = tag.Real;
            return true;
        case 13:
            if (!DxfEntityBuilder::CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.StartX = tag.Real;
            return true;
        case 23:
            if (!DxfEntityBuilder::CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.StartY = tag.Real;
            return true;
        case 14:
            if (!DxfEntityBuilder::CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.EndX = tag.Real;
            return true;
        case 24:
            if (!DxfEntityBuilder::CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.EndY = tag.Real;
            return true;
        case 42:
            if (!DxfEntityBuilder::CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.MeasurementMm = tag.Real;
            return true;
        case 50:
            entity.RotationDegrees = tag.Real;
            return true;
        case 70:
            entity.DimensionType = static_cast<int>(tag.Integer & DimensionFlagMask);
            return true;
        default:
            return DxfEntityBuilder::ApplyCommon(entity, tag, diagnostic);
        }
    }

    bool DxfAnnotationBuilder::ApplyHatchTag(DxfEntity& entity, const DxfTag& tag,
                                              Diagnostic& diagnostic)
    {
        if (tag.Code == 2)
        {
            entity.PatternName = tag.Text;
        }
        if (tag.Code == 8 || tag.Code == 38 || tag.Code == 230)
        {
            return DxfEntityBuilder::ApplyCommon(entity, tag, diagnostic);
        }
        // Hatch reuses 10, 20, 40, 42, 72, 73 and 97 for different things at different depths,
        // so the boundary can only be read once the whole record is in hand.
        if (entity.HatchTags.size() >= Limits::MaxHatchTagCount)
        {
            diagnostic.Code = DiagnosticCode::ValueOutOfRange;
            diagnostic.Message = "hatch tag count " + std::to_string(entity.HatchTags.size());
            diagnostic.LineNumber = tag.LineNumber;
            diagnostic.ByteOffset = tag.ByteOffset;
            return false;
        }
        entity.HatchTags.push_back(tag);
        return true;
    }
}
