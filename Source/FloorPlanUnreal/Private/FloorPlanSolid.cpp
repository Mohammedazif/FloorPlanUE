#include "FloorPlanSolid.h"

using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FIndex3i;

namespace
{
    constexpr double MinimumExtentMm = 0.5;
    constexpr double Scale = FFloorPlanMeshBuilder::MillimetreToUnreal;
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

    const int32 Base = Mesh.MaxVertexID();
    const double X0 = AlongMin * Scale;
    const double X1 = AlongMax * Scale;
    const double Y0 = AcrossMin * Scale;
    const double Y1 = AcrossMax * Scale;
    const double Z0 = UpMin * Scale;
    const double Z1 = UpMax * Scale;

    Mesh.AppendVertex(FVector3d(X0, Y0, Z0));
    Mesh.AppendVertex(FVector3d(X1, Y0, Z0));
    Mesh.AppendVertex(FVector3d(X1, Y1, Z0));
    Mesh.AppendVertex(FVector3d(X0, Y1, Z0));
    Mesh.AppendVertex(FVector3d(X0, Y0, Z1));
    Mesh.AppendVertex(FVector3d(X1, Y0, Z1));
    Mesh.AppendVertex(FVector3d(X1, Y1, Z1));
    Mesh.AppendVertex(FVector3d(X0, Y1, Z1));

    static const int32 Faces[12][3] = {
        {0, 3, 2}, {0, 2, 1},
        {4, 5, 6}, {4, 6, 7},
        {0, 1, 5}, {0, 5, 4},
        {3, 7, 6}, {3, 6, 2},
        {0, 4, 7}, {0, 7, 3},
        {1, 2, 6}, {1, 6, 5}};

    for (const int32(&Face)[3] : Faces)
    {
        AppendTriangle(Mesh, Base + Face[0], Base + Face[1], Base + Face[2], Report);
    }
    ++Report.Boxes;
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
