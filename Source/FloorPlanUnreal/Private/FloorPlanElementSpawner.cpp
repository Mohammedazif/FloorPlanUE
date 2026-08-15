#include "FloorPlanElementSpawner.h"

#include "FloorPlanFittingSpawner.h"

#include "Components/DynamicMeshComponent.h"
#include "Engine/World.h"
#include "FloorPlanElementActors.h"
#include "FloorPlanImportOptions.h"
#include "FloorPlanLimits.h"
#include "FloorPlanMeshBuilder.h"
#include "FloorPlanMeshPlacer.h"
#include "Materials/MaterialInterface.h"
#include "UDynamicMesh.h"

using FloorPlan::Model::BuildingModel;
using FloorPlan::Model::RoomGraph;
using FloorPlan::Model::RoomLink;
using UE::Geometry::FDynamicMesh3;

namespace
{
    constexpr double FloorSlabThicknessMm = FloorPlan::Limits::FloorSlabThicknessMm;
    constexpr double SolidEmbedMm = FloorPlan::Limits::SolidEmbedMm;
    constexpr double ShadowBlockerMarginMm = FloorPlan::Limits::ShadowBlockerMarginMm;
    constexpr double RoofSlabThicknessMm = FloorPlan::Limits::RoofSlabThicknessMm;
    constexpr double RoofOverhangMm = FloorPlan::Limits::RoofOverhangMm;
    constexpr double MillimetreToUnreal = FFloorPlanMeshBuilder::MillimetreToUnreal;

    FString ToUnreal(const std::string& Text)
    {
        return FString(UTF8_TO_TCHAR(Text.c_str()));
    }

    FVector CentreOf(const TArray<FVector2D>& BoundaryMm)
    {
        if (BoundaryMm.Num() == 0)
        {
            return FVector::ZeroVector;
        }
        FVector2D Minimum = BoundaryMm[0];
        FVector2D Maximum = BoundaryMm[0];
        for (const FVector2D& Point : BoundaryMm)
        {
            Minimum.X = FMath::Min(Minimum.X, Point.X);
            Minimum.Y = FMath::Min(Minimum.Y, Point.Y);
            Maximum.X = FMath::Max(Maximum.X, Point.X);
            Maximum.Y = FMath::Max(Maximum.Y, Point.Y);
        }
        const FVector2D Centre = (Minimum + Maximum) * 0.5;
        return FVector(Centre.X * MillimetreToUnreal, Centre.Y * MillimetreToUnreal, 0.0);
    }

    void CollectOpenings(const BuildingModel& Model, const FloorPlan::Model::Wall& Wall,
                         FFloorPlanWallShape& Shape)
    {
        const FVector2D Start(Wall.Start.X, Wall.Start.Y);
        const FVector2D End(Wall.End.X, Wall.End.Y);
        const FVector2D Span = End - Start;
        const double Length = Span.Size();
        if (Length <= 0.0)
        {
            return;
        }
        const FVector2D Direction = Span / Length;

        for (const FloorPlan::Model::Opening& Opening : Model.Openings)
        {
            if (Opening.HostWallId != Wall.Id)
            {
                continue;
            }
            const FVector2D Position(Opening.Position.X, Opening.Position.Y);
            const double Along = FVector2D::DotProduct(Position - Start, Direction);
            const double Half = Opening.WidthMm * 0.5;

            FFloorPlanOpeningCut Cut;
            Cut.AlongStartMm = Along - Half;
            Cut.AlongEndMm = Along + Half;
            Cut.SillMm = Opening.SillHeightMm;
            Cut.HeadMm = Opening.HeadHeightMm;
            Shape.Openings.Add(Cut);
        }
    }

    void DescribeConnectivity(const BuildingModel& Model, const RoomGraph& Graph,
                              std::size_t RoomIndex, AFloorPlanRoomActor& Actor)
    {
        const auto Name = [&Model](std::size_t Index) {
            return Index < Model.Rooms.size() ? ToUnreal(Model.Rooms[Index].Id) : FString();
        };

        for (const RoomLink& Link : Graph.Links())
        {
            const bool bMine = Link.FirstRoom == RoomIndex || Link.SecondRoom == RoomIndex;
            if (!bMine)
            {
                continue;
            }
            if (Link.IsExterior())
            {
                Actor.bTouchesOutside = true;
                continue;
            }
            const std::size_t Other =
                Link.FirstRoom == RoomIndex ? Link.SecondRoom : Link.FirstRoom;
            Actor.AdjacentRoomIds.AddUnique(Name(Other));
            if (Link.IsTraversable())
            {
                Actor.ConnectedRoomIds.AddUnique(Name(Other));
            }
        }
    }

    bool WallFacesOutside(const RoomGraph& Graph, std::size_t WallIndex)
    {
        for (const RoomLink& Link : Graph.Links())
        {
            if (!Link.IsExterior())
            {
                continue;
            }
            for (const std::size_t Index : Link.WallIndices)
            {
                if (Index == WallIndex)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool IsRoomLoop(const BuildingModel& Model, std::size_t LoopIndex)
    {
        for (const FloorPlan::Model::Room& Room : Model.Rooms)
        {
            if (Room.LoopIndex == LoopIndex)
            {
                return true;
            }
        }
        return false;
    }

    const FloorPlan::Model::Room* FirstRoomInside(const BuildingModel& Model,
                                                  const FloorPlan::Geometry::Loop& Candidate)
    {
        for (const FloorPlan::Model::Room& Room : Model.Rooms)
        {
            if (Room.LoopIndex >= Model.Loops.size())
            {
                continue;
            }
            const auto& Points = Model.Loops[Room.LoopIndex].Tessellated();
            if (!Points.empty() && Candidate.Contains(Points.front()))
            {
                return &Room;
            }
        }
        return nullptr;
    }

    void SpawnRoofs(UWorld& World, const BuildingModel& Model,
                    const UFloorPlanImportOptions& Options, const FString& AssetFolder,
                    AFloorPlanStoreyActor& Storey, const FVector& Lift,
                    const FAttachmentTransformRules& AttachRules, FFloorPlanSpawnReport& Report)
    {
        const double Scale = Model.MillimetresPerUnit;
        const FVector RoofLift =
            Lift + FVector(0.0, 0.0,
                           (Options.WallHeightMm + RoofSlabThicknessMm) * MillimetreToUnreal);

        struct FRoofSpan
        {
            std::size_t LoopIndex = 0;
            std::string Id;
        };

        // The roof spans the building outline so it bears on the wall tops, the way a flat
        // roof does; room-sized roofs would leave the wall heads as sunlit ledges.
        std::vector<FRoofSpan> Envelopes;
        for (std::size_t Index = 0; Index < Model.Loops.size(); ++Index)
        {
            if (IsRoomLoop(Model, Index))
            {
                continue;
            }
            const FloorPlan::Model::Room* Inside =
                FirstRoomInside(Model, Model.Loops[Index]);
            if (Inside != nullptr)
            {
                Envelopes.push_back({Index, Inside->Id});
            }
        }

        std::vector<FRoofSpan> Spans;
        for (const FRoofSpan& Candidate : Envelopes)
        {
            const auto& Points = Model.Loops[Candidate.LoopIndex].Tessellated();
            bool bNested = false;
            for (const FRoofSpan& Other : Envelopes)
            {
                if (&Other != &Candidate && !Points.empty() &&
                    Model.Loops[Other.LoopIndex].Contains(Points.front()))
                {
                    bNested = true;
                    break;
                }
            }
            if (!bNested)
            {
                Spans.push_back(Candidate);
            }
        }
        // Single-line plans carry no envelope outline; their rooms are capped one by one.
        if (Spans.empty())
        {
            for (const FloorPlan::Model::Room& Room : Model.Rooms)
            {
                Spans.push_back({Room.LoopIndex, Room.Id});
            }
        }

        for (const FRoofSpan& Span : Spans)
        {
            TArray<FVector2D> Boundary;
            if (Span.LoopIndex < Model.Loops.size())
            {
                for (const auto& Point : Model.Loops[Span.LoopIndex].Tessellated())
                {
                    Boundary.Add(FVector2D(Point.X * Scale, Point.Y * Scale));
                }
            }

            FDynamicMesh3 Mesh;
            FFloorPlanMeshReport MeshReport;
            FTransform Placement = FTransform::Identity;
            // The rim lips an embed past the buried wall tops; flush would z-fight their plane.
            if (Boundary.Num() < 3 ||
                !FFloorPlanMeshBuilder::BuildRoof(Boundary, RoofSlabThicknessMm, SolidEmbedMm,
                                                  Mesh, Placement, MeshReport))
            {
                continue;
            }

            AFloorPlanRoofActor* Actor = World.SpawnActor<AFloorPlanRoofActor>(
                Placement.GetLocation() + RoofLift, Placement.Rotator());
            if (Actor == nullptr)
            {
                continue;
            }
            Actor->ElementId = ToUnreal(Span.Id);
            Actor->StoreyName = Storey.StoreyName;
#if WITH_EDITOR
            Actor->SetActorLabel(FString::Printf(TEXT("Roof_%s"), *Actor->ElementId.Left(8)));
#endif
            if (FFloorPlanMeshPlacer::Place(Options, AssetFolder,
                          FString::Printf(TEXT("Roof_%s"), *Actor->ElementId.Left(8)),
                          Options.FloorMaterial, Mesh, Actor))
            {
                ++Report.BakedMeshes;
            }
            if (Options.bGenerateShadowBlockers && Options.bBakeToStaticMesh)
            {
                FDynamicMesh3 BlockerMesh;
                FFloorPlanMeshReport BlockerReport;
                FTransform BlockerPlacement = FTransform::Identity;
                if (FFloorPlanMeshBuilder::BuildRoof(Boundary, RoofSlabThicknessMm,
                                                     RoofOverhangMm, BlockerMesh,
                                                     BlockerPlacement, BlockerReport))
                {
                    FFloorPlanMeshPlacer::PlaceHiddenCaster(
                        Options, AssetFolder,
                        FString::Printf(TEXT("Roof_%s_Shadow"), *Actor->ElementId.Left(8)),
                        BlockerMesh, Actor);
                }
            }
            Actor->AttachToActor(&Storey, AttachRules);
            Report.Actors.Add(Actor);
        }
    }
}

void FFloorPlanElementSpawner::Spawn(UWorld& World, const BuildingModel& Model,
                                      const RoomGraph& Graph,
                                      const UFloorPlanImportOptions& Options,
                                      const FString& AssetFolder,
                                      const FFloorPlanStoreyRise& Rise,
                                      AFloorPlanStoreyActor& Storey,
                                      FFloorPlanSpawnReport& Report)
{
    const double Scale = Model.MillimetresPerUnit;
    const FVector Lift(0.0, 0.0, Storey.ElevationMm * MillimetreToUnreal);
    const FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, false);

    for (std::size_t Index = 0; Index < Model.Rooms.size(); ++Index)
    {
        const FloorPlan::Model::Room& Room = Model.Rooms[Index];
        TArray<FVector2D> Boundary;
        if (Room.LoopIndex < Model.Loops.size())
        {
            for (const auto& Point : Model.Loops[Room.LoopIndex].Tessellated())
            {
                Boundary.Add(FVector2D(Point.X * Scale, Point.Y * Scale));
            }
        }

        FDynamicMesh3 Mesh;
        FFloorPlanMeshReport MeshReport;
        FTransform Placement = FTransform::Identity;
        const bool bHasFloor =
            Options.bGenerateFloors && Boundary.Num() >= 3 &&
            FFloorPlanMeshBuilder::BuildFloor(Boundary, FloorSlabThicknessMm, Mesh, Placement,
                                              MeshReport);
        if (!bHasFloor)
        {
            Placement = FTransform(CentreOf(Boundary));
        }

        AFloorPlanRoomActor* Actor = World.SpawnActor<AFloorPlanRoomActor>(
            Placement.GetLocation() + Lift, Placement.Rotator());
        if (Actor == nullptr)
        {
            continue;
        }
        Actor->ElementId = ToUnreal(Room.Id);
        Actor->RoomName = ToUnreal(Room.Name);
        Actor->StoreyName = Storey.StoreyName;
        Actor->FloorAreaSquareMetres = Room.AreaMm2 * Scale * Scale / 1000000.0;
        DescribeConnectivity(Model, Graph, Index, *Actor);
#if WITH_EDITOR
        Actor->SetActorLabel(Actor->RoomName.IsEmpty()
                                 ? FString::Printf(TEXT("Room_%s"), *Actor->ElementId.Left(8))
                                 : Actor->RoomName);
#endif
        if (bHasFloor &&
            FFloorPlanMeshPlacer::Place(Options, AssetFolder,
                      FString::Printf(TEXT("Room_%s"), *Actor->ElementId.Left(8)),
                      Options.FloorMaterial, Mesh, Actor))
        {
            ++Report.BakedMeshes;
        }

        Actor->AttachToActor(&Storey, AttachRules);
        Report.FloorAreaSquareMetres += Actor->FloorAreaSquareMetres;
        Report.Actors.Add(Actor);
        ++Report.Rooms;
    }

    // The slab or roof above buries each wall top instead of meeting it edge to edge.
    const bool bCarriesRoof = Options.bGenerateRoof && Rise.ArrivesAtStorey.IsEmpty();
    const bool bSlabAbove = !Rise.ArrivesAtStorey.IsEmpty() && Options.bGenerateFloors;

    for (std::size_t Index = 0; Index < Model.Walls.size(); ++Index)
    {
        const FloorPlan::Model::Wall& Wall = Model.Walls[Index];
        double TopExtensionMm = 0.0;
        if (bCarriesRoof)
        {
            TopExtensionMm = RoofSlabThicknessMm - SolidEmbedMm;
        }
        else if (bSlabAbove)
        {
            TopExtensionMm = FMath::Max(0.0, Rise.RiseMm - SolidEmbedMm - Wall.HeightMm);
        }

        FFloorPlanWallShape Shape;
        Shape.StartMm = FVector2D(Wall.Start.X * Scale, Wall.Start.Y * Scale);
        Shape.EndMm = FVector2D(Wall.End.X * Scale, Wall.End.Y * Scale);
        Shape.Bulge = Wall.Bulge;
        Shape.ThicknessMm = Wall.ThicknessMm * Scale;
        Shape.HeightMm = Wall.HeightMm + TopExtensionMm;
        Shape.BaseMm = Options.bGenerateFloors ? -FloorSlabThicknessMm : 0.0;
        if (Options.bCutOpenings)
        {
            CollectOpenings(Model, Wall, Shape);
        }

        FDynamicMesh3 Mesh;
        FFloorPlanMeshReport MeshReport;
        FTransform Placement = FTransform::Identity;
        const bool bHasMesh =
            FFloorPlanMeshBuilder::BuildWall(Shape, Mesh, Placement, MeshReport);

        AFloorPlanWallActor* Actor = World.SpawnActor<AFloorPlanWallActor>(
            Placement.GetLocation() + Lift, Placement.Rotator());
        if (Actor == nullptr)
        {
            continue;
        }
        Actor->ElementId = ToUnreal(Wall.Id);
        Actor->ThicknessMm = Shape.ThicknessMm;
        Actor->LengthMm = bHasMesh ? MeshReport.LengthMm : (Shape.EndMm - Shape.StartMm).Size();
        Actor->StoreyName = Storey.StoreyName;
        Actor->OpeningCount = MeshReport.OpeningsApplied;
        Actor->bIsExterior = WallFacesOutside(Graph, Index);
#if WITH_EDITOR
        Actor->SetActorLabel(FString::Printf(TEXT("Wall_%s"), *Actor->ElementId.Left(8)));
#endif
        if (bHasMesh)
        {
            if (FFloorPlanMeshPlacer::Place(Options, AssetFolder,
                          FString::Printf(TEXT("Wall_%s"), *Actor->ElementId.Left(8)),
                          Options.WallMaterial, Mesh, Actor))
            {
                ++Report.BakedMeshes;
            }
            if (MeshReport.OpenBoundaryEdges > 0)
            {
                ++Report.UnsealedMeshes;
            }
            if (Options.bGenerateShadowBlockers && Options.bBakeToStaticMesh)
            {
                FFloorPlanWallShape Blocker = Shape;
                Blocker.ThicknessMm += ShadowBlockerMarginMm + ShadowBlockerMarginMm;
                // A roof or slab above buries the top; extending further would ridge past it.
                if (!bCarriesRoof && !bSlabAbove)
                {
                    Blocker.HeightMm += ShadowBlockerMarginMm;
                }
                Blocker.BaseMm -= ShadowBlockerMarginMm;
                if (Blocker.Bulge == 0.0)
                {
                    const FVector2D BlockerSpan = Blocker.EndMm - Blocker.StartMm;
                    const double BlockerLength = BlockerSpan.Size();
                    if (BlockerLength > 0.0)
                    {
                        const FVector2D Along =
                            BlockerSpan * (ShadowBlockerMarginMm / BlockerLength);
                        Blocker.StartMm -= Along;
                        Blocker.EndMm += Along;
                        // The along-frame origin moved back by the margin with the new start.
                        for (FFloorPlanOpeningCut& Cut : Blocker.Openings)
                        {
                            Cut.AlongEndMm += ShadowBlockerMarginMm + ShadowBlockerMarginMm;
                            Cut.SillMm -= ShadowBlockerMarginMm;
                            Cut.HeadMm += ShadowBlockerMarginMm;
                        }
                    }
                }

                FDynamicMesh3 BlockerMesh;
                FFloorPlanMeshReport BlockerReport;
                FTransform BlockerPlacement = FTransform::Identity;
                if (FFloorPlanMeshBuilder::BuildWall(Blocker, BlockerMesh, BlockerPlacement,
                                                     BlockerReport))
                {
                    FFloorPlanMeshPlacer::PlaceHiddenCaster(
                        Options, AssetFolder,
                        FString::Printf(TEXT("Wall_%s_Shadow"), *Actor->ElementId.Left(8)),
                        BlockerMesh, Actor);
                }
            }
        }

        Actor->AttachToActor(&Storey, AttachRules);
        Report.Actors.Add(Actor);
        ++Report.Walls;
    }

    if (Options.bGenerateRoof && Rise.ArrivesAtStorey.IsEmpty())
    {
        SpawnRoofs(World, Model, Options, AssetFolder, Storey, Lift, AttachRules, Report);
    }

    FFloorPlanFittingSpawner::Spawn(World, Model, Options, AssetFolder, Rise, Storey, Lift,
                                    AttachRules, Report);

    Storey.RoomCount = Report.Rooms;
    Storey.WallCount = Report.Walls;
    Storey.StairCount = Report.Stairs;
    Storey.ColumnCount = Report.Columns;
    Storey.FixtureCount = Report.Fixtures;
    Storey.TotalFloorAreaSquareMetres = Report.FloorAreaSquareMetres;
}
