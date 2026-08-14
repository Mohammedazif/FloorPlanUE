#include "FloorPlanMeshUVs.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"

using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FDynamicMeshUVOverlay;
using UE::Geometry::FIndex3i;

namespace
{
    constexpr double TwoPi = 6.28318530717958647692;

    /// Straightens a point on the swept arc into the along/across/up frame of a plain prism.
    FVector3d Unroll(const FVector3d& Point, const FFloorPlanSweptArc& Sweep)
    {
        if (!Sweep.bCurved)
        {
            return Point;
        }

        const double OffsetX = Point.X - Sweep.Centre.X;
        const double OffsetY = Point.Y - Sweep.Centre.Y;
        double Turned = (FMath::Atan2(OffsetY, OffsetX) - Sweep.StartAngle) * Sweep.Handedness;
        Turned = FMath::Fmod(Turned, TwoPi);
        if (Turned < 0.0)
        {
            Turned += TwoPi;
        }
        // Put the branch cut in the middle of the angles the wall does not occupy.
        if (Turned > 0.5 * (Sweep.Sweep + TwoPi))
        {
            Turned -= TwoPi;
        }

        const double Radius = FMath::Sqrt(OffsetX * OffsetX + OffsetY * OffsetY);
        return FVector3d(Sweep.Radius * Turned, (Sweep.Radius - Radius) * Sweep.Handedness,
                         Point.Z);
    }

    int32 DominantAxis(const FVector3d& Normal)
    {
        const double AlongLength = FMath::Abs(Normal.X);
        const double AcrossLength = FMath::Abs(Normal.Y);
        const double UpLength = FMath::Abs(Normal.Z);
        if (AlongLength >= AcrossLength && AlongLength >= UpLength)
        {
            return 0;
        }
        return AcrossLength >= UpLength ? 1 : 2;
    }

    FVector2f PlanarUv(const FVector3d& Local, int32 Axis)
    {
        const double Tiling = FFloorPlanMeshUVs::UvUnitsPerUnrealUnit;
        const double First = Axis == 0 ? Local.Y : Local.X;
        const double Second = Axis == 2 ? Local.Y : Local.Z;
        return FVector2f(static_cast<float>(First * Tiling), static_cast<float>(Second * Tiling));
    }
}

void FFloorPlanMeshUVs::Project(FDynamicMesh3& Mesh, const FFloorPlanSweptArc& Sweep)
{
    if (!Mesh.HasAttributes())
    {
        Mesh.EnableAttributes();
    }
    FDynamicMeshUVOverlay* UVs = Mesh.Attributes()->PrimaryUV();
    if (UVs == nullptr)
    {
        return;
    }
    UVs->ClearElements();

    for (const int32 TriangleId : Mesh.TriangleIndicesItr())
    {
        const FIndex3i Triangle = Mesh.GetTriangle(TriangleId);
        FVector3d Local[3];
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            Local[Corner] = Unroll(Mesh.GetVertex(Triangle[Corner]), Sweep);
        }

        const int32 Axis = DominantAxis((Local[1] - Local[0]).Cross(Local[2] - Local[0]));
        FIndex3i Elements;
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            Elements[Corner] = UVs->AppendElement(PlanarUv(Local[Corner], Axis));
        }
        UVs->SetTriangle(TriangleId, Elements);
    }
}
