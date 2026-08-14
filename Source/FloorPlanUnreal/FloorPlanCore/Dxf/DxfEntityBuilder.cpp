#include "Dxf/DxfEntityBuilder.h"

#include "Dxf/DxfAnnotationBuilder.h"
#include "Dxf/DxfHatchReader.h"

#include "FloorPlanLimits.h"

#include <cmath>

namespace FloorPlan::Dxf
{
    namespace
    {
        constexpr int ClosedPolylineFlag = 1;

        bool Fail(Diagnostic& diagnostic, DiagnosticCode code, const DxfTag& tag,
                  std::string message)
        {
            diagnostic.Code = code;
            diagnostic.Message = std::move(message);
            diagnostic.LineNumber = tag.LineNumber;
            diagnostic.ByteOffset = tag.ByteOffset;
            return false;
        }

        bool CarriesInlineVertices(DxfEntityType type)
        {
            return type == DxfEntityType::LwPolyline;
        }
    }

    DxfEntityType DxfEntityBuilder::Classify(const std::string& name)
    {
        if (name == "LINE") return DxfEntityType::Line;
        if (name == "LWPOLYLINE") return DxfEntityType::LwPolyline;
        if (name == "POLYLINE") return DxfEntityType::Polyline;
        if (name == "ARC") return DxfEntityType::Arc;
        if (name == "CIRCLE") return DxfEntityType::Circle;
        if (name == "INSERT") return DxfEntityType::Insert;
        if (name == "TEXT") return DxfEntityType::Text;
        if (name == "MTEXT") return DxfEntityType::MText;
        if (name == "DIMENSION") return DxfEntityType::Dimension;
        if (name == "HATCH") return DxfEntityType::Hatch;
        return DxfEntityType::Unknown;
    }

    bool DxfEntityBuilder::CheckCoordinate(double value, const DxfTag& tag,
                                           Diagnostic& diagnostic)
    {
        if (!std::isfinite(value))
        {
            return Fail(diagnostic, DiagnosticCode::NonFiniteValue, tag, "coordinate");
        }
        if (std::fabs(value) > Limits::MaxCoordinateMm)
        {
            return Fail(diagnostic, DiagnosticCode::ValueOutOfRange, tag,
                        "coordinate magnitude " + std::to_string(value));
        }
        return true;
    }

    bool DxfEntityBuilder::ApplyCommon(DxfEntity& entity, const DxfTag& tag,
                                        Diagnostic& diagnostic)
    {
        switch (tag.Code)
        {
        case 8:
            entity.Layer = tag.Text;
            return true;
        case 38:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.Elevation = tag.Real;
            return true;
        case 230:
            entity.ExtrusionZ = tag.Real;
            return true;
        default:
            return true;
        }
    }

    bool DxfEntityBuilder::ApplyPolylineVertexTag(DxfEntity& entity, const DxfTag& tag,
                                                   Diagnostic& diagnostic)
    {
        switch (tag.Code)
        {
        case 10:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            if (entity.Vertices.size() >= Limits::MaxVerticesPerPolyline)
            {
                return Fail(diagnostic, DiagnosticCode::VertexLimitExceeded, tag,
                            std::to_string(entity.Vertices.size()));
            }
            entity.Vertices.push_back(DxfPolylineVertex{tag.Real, 0.0, 0.0});
            return true;
        case 20:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            if (entity.Vertices.empty())
            {
                return Fail(diagnostic, DiagnosticCode::UnterminatedEntity, tag,
                            "vertex Y before any X");
            }
            entity.Vertices.back().Y = tag.Real;
            return true;
        case 42:
            if (!std::isfinite(tag.Real) ||
                std::fabs(tag.Real) > Limits::MaxBulgeMagnitude)
            {
                return Fail(diagnostic, DiagnosticCode::ValueOutOfRange, tag,
                            "bulge " + std::to_string(tag.Real));
            }
            if (entity.Vertices.empty())
            {
                return Fail(diagnostic, DiagnosticCode::UnterminatedEntity, tag,
                            "bulge before any vertex");
            }
            entity.Vertices.back().Bulge = tag.Real;
            return true;
        case 70:
            entity.Closed = (tag.Integer & ClosedPolylineFlag) != 0;
            return true;
        default:
            return ApplyCommon(entity, tag, diagnostic);
        }
    }

    bool DxfEntityBuilder::ApplyInsertTag(DxfEntity& entity, const DxfTag& tag,
                                           Diagnostic& diagnostic)
    {
        switch (tag.Code)
        {
        case 2:
            entity.BlockName = tag.Text;
            return true;
        case 10:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.InsertX = tag.Real;
            return true;
        case 20:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.InsertY = tag.Real;
            return true;
        case 41:
        case 42:
        case 43:
        {
            const double magnitude = std::fabs(tag.Real);
            if (!std::isfinite(tag.Real) || magnitude > Limits::MaxInsertScaleMagnitude ||
                magnitude < Limits::MinInsertScaleMagnitude)
            {
                return Fail(diagnostic, DiagnosticCode::ValueOutOfRange, tag,
                            "insert scale " + std::to_string(tag.Real));
            }
            if (tag.Code == 41) entity.ScaleX = tag.Real;
            else if (tag.Code == 42) entity.ScaleY = tag.Real;
            else entity.ScaleZ = tag.Real;
            return true;
        }
        case 50:
            if (!std::isfinite(tag.Real))
            {
                return Fail(diagnostic, DiagnosticCode::NonFiniteValue, tag, "rotation");
            }
            entity.RotationDegrees = tag.Real;
            return true;
        case 70:
            entity.ColumnCount = static_cast<int>(tag.Integer);
            return true;
        case 71:
            entity.RowCount = static_cast<int>(tag.Integer);
            return true;
        case 44:
            entity.ColumnSpacing = tag.Real;
            return true;
        case 45:
            entity.RowSpacing = tag.Real;
            return true;
        default:
            return ApplyCommon(entity, tag, diagnostic);
        }
    }

    bool DxfEntityBuilder::ApplyTextTag(DxfEntity& entity, const DxfTag& tag,
                                         Diagnostic& diagnostic)
    {
        switch (tag.Code)
        {
        case 1:
            entity.Text += tag.Text;
            return true;
        case 3:
            entity.Text += tag.Text;
            return true;
        case 10:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.StartX = tag.Real;
            return true;
        case 20:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.StartY = tag.Real;
            return true;
        case 11:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.AlignX = tag.Real;
            entity.HasAlignmentPoint = true;
            return true;
        case 21:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.AlignY = tag.Real;
            entity.HasAlignmentPoint = true;
            return true;
        case 40:
            if (!std::isfinite(tag.Real) || tag.Real < 0.0 ||
                tag.Real > Limits::MaxTextHeightMm)
            {
                return Fail(diagnostic, DiagnosticCode::ValueOutOfRange, tag,
                            "text height " + std::to_string(tag.Real));
            }
            entity.TextHeight = tag.Real;
            return true;
        case 50:
            entity.RotationDegrees = tag.Real;
            return true;
        case 71:
            if (entity.Type == DxfEntityType::MText)
            {
                entity.VerticalJustification = static_cast<int>(tag.Integer);
            }
            return true;
        case 72:
            entity.HorizontalJustification = static_cast<int>(tag.Integer);
            return true;
        case 73:
            entity.VerticalJustification = static_cast<int>(tag.Integer);
            return true;
        default:
            return ApplyCommon(entity, tag, diagnostic);
        }
    }

    bool DxfEntityBuilder::Apply(DxfEntity& entity, const DxfTag& tag, Diagnostic& diagnostic)
    {
        if (CarriesInlineVertices(entity.Type))
        {
            return ApplyPolylineVertexTag(entity, tag, diagnostic);
        }
        if (entity.Type == DxfEntityType::Polyline)
        {
            // POLYLINE's own 10/20 is a placeholder the format fixes at zero; 30 is the elevation.
            if (tag.Code == 10 || tag.Code == 20)
            {
                return true;
            }
            if (tag.Code == 30)
            {
                if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
                entity.Elevation = tag.Real;
                return true;
            }
            if (tag.Code == 70)
            {
                entity.Closed = (tag.Integer & ClosedPolylineFlag) != 0;
                return true;
            }
            return ApplyCommon(entity, tag, diagnostic);
        }
        if (entity.Type == DxfEntityType::Insert)
        {
            return ApplyInsertTag(entity, tag, diagnostic);
        }
        if (entity.Type == DxfEntityType::Text || entity.Type == DxfEntityType::MText)
        {
            return ApplyTextTag(entity, tag, diagnostic);
        }
        if (entity.Type == DxfEntityType::Dimension)
        {
            return DxfAnnotationBuilder::ApplyDimensionTag(entity, tag, diagnostic);
        }
        if (entity.Type == DxfEntityType::Hatch)
        {
            return DxfAnnotationBuilder::ApplyHatchTag(entity, tag, diagnostic);
        }

        switch (tag.Code)
        {
        case 30:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.StartZ = tag.Real;
            return true;
        case 31:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.EndZ = tag.Real;
            return true;
        case 10:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            if (entity.Type == DxfEntityType::Line) entity.StartX = tag.Real;
            else entity.CenterX = tag.Real;
            return true;
        case 20:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            if (entity.Type == DxfEntityType::Line) entity.StartY = tag.Real;
            else entity.CenterY = tag.Real;
            return true;
        case 11:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.EndX = tag.Real;
            return true;
        case 21:
            if (!CheckCoordinate(tag.Real, tag, diagnostic)) return false;
            entity.EndY = tag.Real;
            return true;
        case 40:
            if (!std::isfinite(tag.Real) || tag.Real < 0.0 || tag.Real > Limits::MaxRadiusMm)
            {
                return Fail(diagnostic, DiagnosticCode::ValueOutOfRange, tag,
                            "radius " + std::to_string(tag.Real));
            }
            entity.Radius = tag.Real;
            return true;
        case 50:
            entity.StartAngleDegrees = tag.Real;
            return true;
        case 51:
            entity.EndAngleDegrees = tag.Real;
            return true;
        default:
            return ApplyCommon(entity, tag, diagnostic);
        }
    }

    bool DxfEntityBuilder::Finish(DxfEntity& entity, Diagnostic& diagnostic)
    {
        if (entity.Type == DxfEntityType::Circle)
        {
            entity.StartAngleDegrees = 0.0;
            entity.EndAngleDegrees = 360.0;
        }
        if (entity.Type != DxfEntityType::Hatch)
        {
            return true;
        }

        DxfHatchBoundary boundary;
        const bool ok = DxfHatchReader::Read(entity.HatchTags, boundary, diagnostic);
        entity.HatchTags.clear();
        entity.HatchTags.shrink_to_fit();
        if (!ok)
        {
            return false;
        }
        entity.BoundaryLoops = std::move(boundary.Loops);
        entity.SkippedBoundaryPaths = boundary.SkippedPaths;
        return true;
    }
}
