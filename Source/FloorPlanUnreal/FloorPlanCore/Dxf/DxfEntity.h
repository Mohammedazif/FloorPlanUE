#pragma once

#include "Dxf/DxfEntityType.h"
#include "Dxf/DxfTag.h"

#include <cstddef>
#include <string>
#include <vector>

namespace FloorPlan::Dxf
{
    struct DxfPolylineVertex
    {
        double X = 0.0;
        double Y = 0.0;
        double Bulge = 0.0;
    };

    /// One closed boundary path of a hatch, reduced to vertices whatever its edges were.
    struct DxfHatchLoop
    {
        std::vector<DxfPolylineVertex> Vertices;

        /// Set for the paths DXF marks external or outermost, which bound rather than subtract.
        bool IsOutermost = false;
    };

    /// A parsed DXF entity. Fields not relevant to the entity's type keep their defaults.
    struct DxfEntity
    {
        DxfEntityType Type = DxfEntityType::Unknown;
        std::string Layer;
        std::size_t LineNumber = 0;
        bool FromBlock = false;

        double ExtrusionZ = 1.0;
        double Elevation = 0.0;

        double StartX = 0.0;
        double StartY = 0.0;
        double StartZ = 0.0;
        double EndX = 0.0;
        double EndY = 0.0;
        double EndZ = 0.0;

        double CenterX = 0.0;
        double CenterY = 0.0;
        double Radius = 0.0;
        double StartAngleDegrees = 0.0;
        double EndAngleDegrees = 0.0;

        std::vector<DxfPolylineVertex> Vertices;
        bool Closed = false;

        std::string BlockName;
        double InsertX = 0.0;
        double InsertY = 0.0;
        double ScaleX = 1.0;
        double ScaleY = 1.0;
        double ScaleZ = 1.0;
        double RotationDegrees = 0.0;
        int ColumnCount = 1;
        int RowCount = 1;
        double ColumnSpacing = 0.0;
        double RowSpacing = 0.0;

        /// Value the draughtsman's dimension asserts, in drawing units.
        double MeasurementMm = 0.0;
        int DimensionType = 0;

        std::string PatternName;
        bool SolidFill = false;

        /// Every closed path of a hatch boundary, outermost and inner alike.
        std::vector<DxfHatchLoop> BoundaryLoops;

        /// Hatch boundary paths this reader could not build, so callers can say areas may differ.
        std::size_t SkippedBoundaryPaths = 0;

        /// Raw hatch tags, held only until Finish resolves them and then released.
        std::vector<DxfTag> HatchTags;

        std::string Text;
        double TextHeight = 0.0;
        double AlignX = 0.0;
        double AlignY = 0.0;
        bool HasAlignmentPoint = false;
        int HorizontalJustification = 0;
        int VerticalJustification = 0;

        /// The point a room label should be tested against, honouring TEXT justification.
        double AnchorX() const
        {
            const bool justified = HorizontalJustification != 0 || VerticalJustification != 0;
            return (justified && HasAlignmentPoint) ? AlignX : StartX;
        }

        double AnchorY() const
        {
            const bool justified = HorizontalJustification != 0 || VerticalJustification != 0;
            return (justified && HasAlignmentPoint) ? AlignY : StartY;
        }
    };
}
