#include "FloorPlanSolid.h"

using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FIndex3i;

namespace
{
    constexpr double MinimumExtentMm = 0.5;
    constexpr double Scale = FFloorPlanMeshBuilder::MillimetreToUnreal;

    // Corners run anticlockwise in plan, which is what makes this face table point outward.
    constexpr int32 BoxFaces[12][3] = {
        {0, 3, 2}, {0, 2, 1},
        {4, 5, 6}, {4, 6, 7},
        {0, 1, 5}, {0, 5, 4},
        {3, 7, 6}, {3, 6, 2},
        {0, 4, 7}, {0, 7, 3},
        {1, 2, 6}, {1, 6, 5}};

    void AppendHull(FDynamicMesh3& Mesh, const FVector2D (&Corners)[4], double UpMin, double UpMax,
                    FFloorPlanMeshReport& Report)
    {
        const int32 Base = Mesh.MaxVertexID();
        for (const FVector2D& Corner : Corners)
        {
            Mesh.AppendVertex(FVector3d(Corner.X * Scale, Corner.Y * Scale, UpMin * Scale));
        }
        for (const FVector2D& Corner : Corners)
        {
            Mesh.AppendVertex(FVector3d(Corner.X * Scale, Corner.Y * Scale, UpMax * Scale));
        }
        for (const int32(&Face)[3] : BoxFaces)
        {
            FFloorPlanSolid::AppendTriangle(Mesh, Base + Face[0], Base + Face[1], Base + Face[2],
                                            Report);
        }
        ++Report.Boxes;
    }
}

void FFloorPlanSolid::AppendTriangle(FDynamicMesh3& Mesh, int32 A, int32 B, int32 C,
                                      FFloorPlanMeshReport& Report)
{
    Mesh.AppendTriangle(FIndex3i(A, C, B));
    ++Report.Triangles;
}

void FFloorPlanSolid::AppendBox(FDynamicMesh3& Mesh, double AlongMin, double AlongMax,
                                 double AcrossMin, double AcrossMax, double UpMin, double UpMax,
                                 FFloorPlanMeshReport& Report)
{
    if (AlongMax - AlongMin < MinimumExtentMm || UpMax - UpMin < MinimumExtentMm ||
        AcrossMax - AcrossMin < MinimumExtentMm)
    {
        return;
    }

    const FVector2D Corners[4] = {FVector2D(AlongMin, AcrossMin), FVector2D(AlongMax, AcrossMin),
                                  FVector2D(AlongMax, AcrossMax), FVector2D(AlongMin, AcrossMax)};
    AppendHull(Mesh, Corners, UpMin, UpMax, Report);
}

void FFloorPlanSolid::AppendSegmentBox(FDynamicMesh3& Mesh, const FVector2D& StartMm,
                                        const FVector2D& EndMm, double HalfWidthMm, double UpMin,
                                        double UpMax, FFloorPlanMeshReport& Report)
{
    const FVector2D Span = EndMm - StartMm;
    const double Length = Span.Size();
    if (Length < MinimumExtentMm || HalfWidthMm + HalfWidthMm < MinimumExtentMm ||
        UpMax - UpMin < MinimumExtentMm)
    {
        return;
    }

    const FVector2D Across = FVector2D(-Span.Y, Span.X) * (HalfWidthMm / Length);
    const FVector2D Corners[4] = {StartMm - Across, EndMm - Across, EndMm + Across,
                                  StartMm + Across};
    AppendHull(Mesh, Corners, UpMin, UpMax, Report);
}

int32 FFloorPlanSolid::CountOpenBoundaryEdges(const FDynamicMesh3& Mesh)
{
    int32 Open = 0;
    for (const int32 EdgeId : Mesh.EdgeIndicesItr())
    {
        if (Mesh.IsBoundaryEdge(EdgeId))
        {
            ++Open;
        }
    }
    return Open;
}
