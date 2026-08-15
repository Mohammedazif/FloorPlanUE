#include "FloorPlanMeshBuilder.h"

#include "DynamicMesh/MeshNormals.h"
#include "FloorPlanMeshUVs.h"
#include "FloorPlanPolygonTriangulator.h"
#include "FloorPlanSolid.h"
#include "UDynamicMesh.h"

THIRD_PARTY_INCLUDES_START
#include "FloorPlanLimits.h"
#include "Geometry/Bulge.h"
THIRD_PARTY_INCLUDES_END

using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FIndex3i;

namespace
{
    constexpr double MinimumExtentMm = 0.5;
    constexpr double Scale = FFloorPlanMeshBuilder::MillimetreToUnreal;
    constexpr int32 MaxArcStepsPerSpan = 256;

    struct FArcFrame
    {
        FVector2D Centre = FVector2D::ZeroVector;
        double Radius = 0.0;
        double StartAngle = 0.0;
        double IncludedAngle = 0.0;
        double ArcLengthMm = 0.0;
    };

    /// Resolves the bulge in the wall's own frame, where the chord runs along local X.
    bool ResolveArcFrame(const FFloorPlanWallShape& Shape, double ChordLengthMm,
                         FArcFrame& OutFrame)
    {
        if (FloorPlan::Geometry::Bulge::IsStraight(Shape.Bulge))
        {
            return false;
        }
        const double HalfChord = 0.5 * ChordLengthMm;
        const FloorPlan::Geometry::BulgeArc Arc = FloorPlan::Geometry::Bulge::Resolve(
            FloorPlan::Geometry::Vec2{-HalfChord, 0.0},
            FloorPlan::Geometry::Vec2{HalfChord, 0.0}, Shape.Bulge);
        if (Arc.IsStraight || Arc.Radius <= 0.5 * Shape.ThicknessMm + MinimumExtentMm)
        {
            return false;
        }

        OutFrame.Centre = FVector2D(Arc.Center.X, Arc.Center.Y);
        OutFrame.Radius = Arc.Radius;
        OutFrame.StartAngle = Arc.StartAngle;
        OutFrame.IncludedAngle = Arc.IncludedAngle;
        OutFrame.ArcLengthMm = Arc.Radius * FMath::Abs(Arc.IncludedAngle);
        return OutFrame.ArcLengthMm >= MinimumExtentMm;
    }

    int32 ArcStepCount(double RadiusMm, double SweptAngle)
    {
        FloorPlan::Geometry::BulgeArc Probe;
        Probe.IsStraight = false;
        Probe.Radius = RadiusMm;
        Probe.IncludedAngle = SweptAngle;
        const double Steps = static_cast<double>(FloorPlan::Geometry::Bulge::SegmentCount(
            Probe, FloorPlan::Limits::ArcTessellationSagittaMm));
        return static_cast<int32>(
            FMath::Clamp(Steps, 1.0, static_cast<double>(MaxArcStepsPerSpan)));
    }

    constexpr int32 RightBottomSlot = 0;
    constexpr int32 LeftBottomSlot = 1;
    constexpr int32 RightTopSlot = 2;
    constexpr int32 LeftTopSlot = 3;
    constexpr int32 RingVertexCount = 4;

    /// Appends the four vertices of one cross-section, in the order the slot constants name.
    void AppendRing(FDynamicMesh3& Mesh, const FArcFrame& Frame, double RightRadius,
                    double LeftRadius, double Angle, double UpMin, double UpMax)
    {
        const double Cosine = FMath::Cos(Angle);
        const double Sine = FMath::Sin(Angle);
        const double RightX = (Frame.Centre.X + RightRadius * Cosine) * Scale;
        const double RightY = (Frame.Centre.Y + RightRadius * Sine) * Scale;
        const double LeftX = (Frame.Centre.X + LeftRadius * Cosine) * Scale;
        const double LeftY = (Frame.Centre.Y + LeftRadius * Sine) * Scale;

        Mesh.AppendVertex(FVector3d(RightX, RightY, UpMin * Scale));
        Mesh.AppendVertex(FVector3d(LeftX, LeftY, UpMin * Scale));
        Mesh.AppendVertex(FVector3d(RightX, RightY, UpMax * Scale));
        Mesh.AppendVertex(FVector3d(LeftX, LeftY, UpMax * Scale));
    }

    void AppendCurvedSlab(FDynamicMesh3& Mesh, const FArcFrame& Frame, double HalfThicknessMm,
                          double AlongMin, double AlongMax, double UpMin, double UpMax,
                          FFloorPlanMeshReport& Report)
    {
        if (AlongMax - AlongMin < MinimumExtentMm || UpMax - UpMin < MinimumExtentMm)
        {
            return;
        }

        const double AnglePerMm = Frame.IncludedAngle / Frame.ArcLengthMm;
        const double FirstAngle = Frame.StartAngle + AnglePerMm * AlongMin;
        const double SweptAngle = AnglePerMm * (AlongMax - AlongMin);
        const double Handedness = Frame.IncludedAngle >= 0.0 ? 1.0 : -1.0;
        const double RightRadius = Frame.Radius + HalfThicknessMm * Handedness;
        const double LeftRadius = Frame.Radius - HalfThicknessMm * Handedness;

        const int32 Steps = ArcStepCount(FMath::Max(RightRadius, LeftRadius), SweptAngle);
        const int32 Base = Mesh.MaxVertexID();
        for (int32 Step = 0; Step <= Steps; ++Step)
        {
            const double Fraction = static_cast<double>(Step) / static_cast<double>(Steps);
            AppendRing(Mesh, Frame, RightRadius, LeftRadius, FirstAngle + SweptAngle * Fraction,
                       UpMin, UpMax);
        }

        const auto Face = [&Mesh, &Report](int32 A, int32 B, int32 C) {
            FFloorPlanSolid::AppendTriangle(Mesh, A, B, C, Report);
        };

        for (int32 Step = 0; Step < Steps; ++Step)
        {
            const int32 Near = Base + Step * RingVertexCount;
            const int32 Far = Near + RingVertexCount;
            const int32 RightBottomNear = Near + RightBottomSlot;
            const int32 LeftBottomNear = Near + LeftBottomSlot;
            const int32 RightTopNear = Near + RightTopSlot;
            const int32 LeftTopNear = Near + LeftTopSlot;
            const int32 RightBottomFar = Far + RightBottomSlot;
            const int32 LeftBottomFar = Far + LeftBottomSlot;
            const int32 RightTopFar = Far + RightTopSlot;
            const int32 LeftTopFar = Far + LeftTopSlot;

            Face(RightBottomNear, LeftBottomNear, LeftBottomFar);
            Face(RightBottomNear, LeftBottomFar, RightBottomFar);
            Face(RightTopNear, RightTopFar, LeftTopFar);
            Face(RightTopNear, LeftTopFar, LeftTopNear);
            Face(RightBottomNear, RightBottomFar, RightTopFar);
            Face(RightBottomNear, RightTopFar, RightTopNear);
            Face(LeftBottomNear, LeftTopNear, LeftTopFar);
            Face(LeftBottomNear, LeftTopFar, LeftBottomFar);
        }

        const int32 Tail = Base + Steps * RingVertexCount;
        Face(Base + RightBottomSlot, Base + RightTopSlot, Base + LeftTopSlot);
        Face(Base + RightBottomSlot, Base + LeftTopSlot, Base + LeftBottomSlot);
        Face(Tail + RightBottomSlot, Tail + LeftBottomSlot, Tail + LeftTopSlot);
        Face(Tail + RightBottomSlot, Tail + LeftTopSlot, Tail + RightTopSlot);
        ++Report.Boxes;
    }

    void GatherCuts(const FFloorPlanWallShape& Shape, double LengthMm,
                    TArray<FFloorPlanOpeningCut>& OutCuts)
    {
        for (const FFloorPlanOpeningCut& Opening : Shape.Openings)
        {
            FFloorPlanOpeningCut Clamped;
            Clamped.AlongStartMm = FMath::Clamp(Opening.AlongStartMm, 0.0, LengthMm);
            Clamped.AlongEndMm = FMath::Clamp(Opening.AlongEndMm, 0.0, LengthMm);
            Clamped.SillMm = FMath::Clamp(Opening.SillMm, 0.0, Shape.HeightMm);
            Clamped.HeadMm = FMath::Clamp(Opening.HeadMm, 0.0, Shape.HeightMm);
            if (Clamped.AlongEndMm - Clamped.AlongStartMm < MinimumExtentMm ||
                Clamped.HeadMm - Clamped.SillMm < MinimumExtentMm)
            {
                continue;
            }
            OutCuts.Add(Clamped);
        }
        OutCuts.Sort([](const FFloorPlanOpeningCut& A, const FFloorPlanOpeningCut& B) {
            return A.AlongStartMm < B.AlongStartMm;
        });
    }

    FVector2D BoundaryCentre(const TArray<FVector2D>& BoundaryMm)
    {
        FVector2D Minimum = BoundaryMm[0];
        FVector2D Maximum = BoundaryMm[0];
        for (const FVector2D& Point : BoundaryMm)
        {
            Minimum.X = FMath::Min(Minimum.X, Point.X);
            Minimum.Y = FMath::Min(Minimum.Y, Point.Y);
            Maximum.X = FMath::Max(Maximum.X, Point.X);
            Maximum.Y = FMath::Max(Maximum.Y, Point.Y);
        }
        return (Minimum + Maximum) * 0.5;
    }

    /// Straddles every boundary edge by HalfWidthMm so the slab reaches past its outline
    /// instead of stopping dead against it — a burial skirt or an eaves ring by width.
    void AppendBoundaryRing(FDynamicMesh3& Mesh, const TArray<FVector2D>& BoundaryMm,
                            double HalfWidthMm, double BaseMm, double TopMm,
                            FFloorPlanMeshReport& Report)
    {
        const FVector2D Centre = BoundaryCentre(BoundaryMm);
        const int32 Count = BoundaryMm.Num();
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const FVector2D Start = BoundaryMm[Index] - Centre;
            const FVector2D End = BoundaryMm[(Index + 1) % Count] - Centre;
            const FVector2D Span = End - Start;
            const double Length = Span.Size();
            if (Length < MinimumExtentMm)
            {
                continue;
            }
            const FVector2D Extension = Span * (HalfWidthMm / Length);
            FFloorPlanSolid::AppendSegmentBox(Mesh, Start - Extension, End + Extension,
                                              HalfWidthMm, BaseMm, TopMm, Report);
        }
    }
}

bool FFloorPlanMeshBuilder::BuildWall(const FFloorPlanWallShape& Shape, FDynamicMesh3& Mesh,
                                       FTransform& OutTransform, FFloorPlanMeshReport& Report)
{
    const FVector2D Span = Shape.EndMm - Shape.StartMm;
    const double ChordLength = Span.Size();
    if (ChordLength < MinimumExtentMm || Shape.ThicknessMm < MinimumExtentMm ||
        Shape.HeightMm - Shape.BaseMm < MinimumExtentMm)
    {
        Report.bDegenerate = true;
        return false;
    }

    const FVector2D Midpoint = (Shape.StartMm + Shape.EndMm) * 0.5;
    const double YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Span.Y, Span.X));
    OutTransform = FTransform(FRotator(0.0, YawDegrees, 0.0),
                              FVector(Midpoint.X * Scale, Midpoint.Y * Scale, 0.0));

    FArcFrame Arc;
    const bool bCurved = ResolveArcFrame(Shape, ChordLength, Arc);
    const double Length = bCurved ? Arc.ArcLengthMm : ChordLength;
    const double Half = Shape.ThicknessMm * 0.5;
    const double Origin = Length * 0.5;
    Report.LengthMm = Length;

    const auto AppendSpan = [&](double AlongMin, double AlongMax, double UpMin, double UpMax) {
        if (bCurved)
        {
            AppendCurvedSlab(Mesh, Arc, Half, AlongMin, AlongMax, UpMin, UpMax, Report);
        }
        else
        {
            FFloorPlanSolid::AppendBox(Mesh, AlongMin - Origin, AlongMax - Origin, -Half, Half,
                                       UpMin, UpMax, Report);
        }
    };

    TArray<FFloorPlanOpeningCut> Cuts;
    if (!bCurved)
    {
        GatherCuts(Shape, Length, Cuts);
    }

    double Cursor = 0.0;
    for (const FFloorPlanOpeningCut& Cut : Cuts)
    {
        if (Cut.AlongStartMm < Cursor)
        {
            continue;
        }
        AppendSpan(Cursor, Cut.AlongStartMm, Shape.BaseMm, Shape.HeightMm);
        AppendSpan(Cut.AlongStartMm, Cut.AlongEndMm, Shape.BaseMm, Cut.SillMm);
        AppendSpan(Cut.AlongStartMm, Cut.AlongEndMm, Cut.HeadMm, Shape.HeightMm);
        Cursor = Cut.AlongEndMm;
        ++Report.OpeningsApplied;
    }
    AppendSpan(Cursor, Length, Shape.BaseMm, Shape.HeightMm);

    FFloorPlanSweptArc Sweep;
    if (bCurved)
    {
        Sweep.bCurved = true;
        Sweep.Centre = Arc.Centre * Scale;
        Sweep.Radius = Arc.Radius * Scale;
        Sweep.StartAngle = Arc.StartAngle;
        Sweep.Sweep = FMath::Abs(Arc.IncludedAngle);
        Sweep.Handedness = Arc.IncludedAngle >= 0.0 ? 1.0 : -1.0;
    }
    FFloorPlanMeshUVs::Project(Mesh, Sweep);

    Report.OpenBoundaryEdges = FFloorPlanSolid::CountOpenBoundaryEdges(Mesh);
    return Report.Triangles > 0;
}

bool FFloorPlanMeshBuilder::BuildFloor(const TArray<FVector2D>& BoundaryMm, double ThicknessMm,
                                        FDynamicMesh3& Mesh, FTransform& OutTransform,
                                        FFloorPlanMeshReport& Report)
{
    if (!BuildPrism(BoundaryMm, -ThicknessMm, 0.0, Mesh, OutTransform, Report))
    {
        return false;
    }
    AppendBoundaryRing(Mesh, BoundaryMm, FloorPlan::Limits::SolidEmbedMm,
                       -ThicknessMm + FloorPlan::Limits::SolidEmbedMm, 0.0, Report);
    FFloorPlanMeshUVs::Project(Mesh, FFloorPlanSweptArc{});
    Report.OpenBoundaryEdges = FFloorPlanSolid::CountOpenBoundaryEdges(Mesh);
    return true;
}

bool FFloorPlanMeshBuilder::BuildRoof(const TArray<FVector2D>& BoundaryMm, double ThicknessMm,
                                       double OverhangMm, FDynamicMesh3& Mesh,
                                       FTransform& OutTransform, FFloorPlanMeshReport& Report)
{
    if (!BuildPrism(BoundaryMm, -ThicknessMm, 0.0, Mesh, OutTransform, Report))
    {
        return false;
    }
    // Ring top sits an embed below the cap so its inner straddle never coplanes with it.
    AppendBoundaryRing(Mesh, BoundaryMm, OverhangMm, -ThicknessMm,
                       -FloorPlan::Limits::SolidEmbedMm, Report);
    FFloorPlanMeshUVs::Project(Mesh, FFloorPlanSweptArc{});
    Report.OpenBoundaryEdges = FFloorPlanSolid::CountOpenBoundaryEdges(Mesh);
    return true;
}

bool FFloorPlanMeshBuilder::BuildPrism(const TArray<FVector2D>& BoundaryMm, double BaseMm,
                                        double TopMm, FDynamicMesh3& Mesh,
                                        FTransform& OutTransform, FFloorPlanMeshReport& Report)
{
    if (BoundaryMm.Num() < 3 || TopMm - BaseMm < MinimumExtentMm)
    {
        Report.bDegenerate = true;
        return false;
    }

    TArray<FIndex3i> Triangles;
    if (!FFloorPlanPolygonTriangulator::Triangulate(BoundaryMm, Triangles))
    {
        Report.bDegenerate = true;
        return false;
    }

    const FVector2D Centre = BoundaryCentre(BoundaryMm);
    OutTransform = FTransform(FVector(Centre.X * Scale, Centre.Y * Scale, 0.0));

    const int32 Base = Mesh.MaxVertexID();
    for (const FVector2D& Point : BoundaryMm)
    {
        Mesh.AppendVertex(FVector3d((Point.X - Centre.X) * Scale, (Point.Y - Centre.Y) * Scale,
                                    TopMm * Scale));
    }
    const int32 Lower = Mesh.MaxVertexID();
    for (const FVector2D& Point : BoundaryMm)
    {
        Mesh.AppendVertex(FVector3d((Point.X - Centre.X) * Scale, (Point.Y - Centre.Y) * Scale,
                                    BaseMm * Scale));
    }

    for (const FIndex3i& Triangle : Triangles)
    {
        FFloorPlanSolid::AppendTriangle(Mesh, Base + Triangle.A, Base + Triangle.B,
                                        Base + Triangle.C, Report);
        FFloorPlanSolid::AppendTriangle(Mesh, Lower + Triangle.C, Lower + Triangle.B,
                                        Lower + Triangle.A, Report);
    }

    const int32 Count = BoundaryMm.Num();
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const int32 Next = (Index + 1) % Count;
        FFloorPlanSolid::AppendTriangle(Mesh, Base + Index, Lower + Index, Lower + Next, Report);
        FFloorPlanSolid::AppendTriangle(Mesh, Base + Index, Lower + Next, Base + Next, Report);
    }

    ++Report.Boxes;
    FFloorPlanMeshUVs::Project(Mesh, FFloorPlanSweptArc{});
    Report.OpenBoundaryEdges = FFloorPlanSolid::CountOpenBoundaryEdges(Mesh);
    return true;
}

void FFloorPlanMeshBuilder::CopyToDynamicMesh(const FDynamicMesh3& Source, UDynamicMesh* Target)
{
    if (Target == nullptr)
    {
        return;
    }
    Target->Reset();
    Target->EditMesh(
        [&Source](FDynamicMesh3& EditMesh)
        {
            EditMesh = Source;
            // Enabling attributes rebuilds the set, which would discard the UVs already on it.
            if (!EditMesh.HasAttributes())
            {
                EditMesh.EnableAttributes();
            }
            // Boxes need hard edges: per-vertex smoothing rounds every corner into mush.
            UE::Geometry::FMeshNormals::InitializeMeshToPerTriangleNormals(&EditMesh);
        },
        EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
}
