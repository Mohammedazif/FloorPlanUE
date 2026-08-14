#include "FloorPlanStairMesh.h"

#include "FloorPlanMeshUVs.h"
#include "FloorPlanSolid.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
    constexpr double MinimumExtentMm = 0.5;
    constexpr double Scale = FFloorPlanMeshBuilder::MillimetreToUnreal;
}

bool FFloorPlanStairMesh::Build(const FFloorPlanStairShape& Shape, FDynamicMesh3& Mesh,
                                 FTransform& OutTransform, FFloorPlanMeshReport& Report)
{
    const FVector2D Span = Shape.EndMm - Shape.StartMm;
    const double RunLength = Span.Size();
    if (Shape.StepCount < 1 || RunLength < MinimumExtentMm ||
        Shape.WidthMm < MinimumExtentMm || Shape.RiserHeightMm < MinimumExtentMm)
    {
        Report.bDegenerate = true;
        return false;
    }

    const FVector2D Midpoint = (Shape.StartMm + Shape.EndMm) * 0.5;
    const double YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Span.Y, Span.X));
    OutTransform = FTransform(FRotator(0.0, YawDegrees, 0.0),
                              FVector(Midpoint.X * Scale, Midpoint.Y * Scale, 0.0));

    const double Half = Shape.WidthMm * 0.5;
    const double Origin = RunLength * 0.5;

    for (int32 Step = 0; Step < Shape.StepCount; ++Step)
    {
        const double AlongMin = Shape.TreadDepthMm * static_cast<double>(Step);
        const double AlongMax = AlongMin + Shape.TreadDepthMm;
        const double Top = Shape.RiserHeightMm * static_cast<double>(Step + 1);
        FFloorPlanSolid::AppendBox(Mesh, AlongMin - Origin, AlongMax - Origin, -Half, Half,
                                   Shape.BaseMm, Top, Report);
    }

    FFloorPlanMeshUVs::Project(Mesh, FFloorPlanSweptArc{});
    Report.LengthMm = RunLength;
    Report.OpenBoundaryEdges = FFloorPlanSolid::CountOpenBoundaryEdges(Mesh);
    return Report.Triangles > 0;
}
