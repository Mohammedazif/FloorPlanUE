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

    void SpawnRoofs(UWorld& World, const BuildingModel& Model,
                    const UFloorPlanImportOptions& Options, const FString& AssetFolder,
                    AFloorPlanStoreyActor& Storey, const FVector& Lift,
                    const FAttachmentTransformRules& AttachRules, FFloorPlanSpawnReport& Report)
    {
        const double Scale = Model.MillimetresPerUnit;
        const FVector RoofLift =
            Lift + FVector(0.0, 0.0,
                           (Options.WallHeightMm + FloorSlabThicknessMm) * MillimetreToUnreal);

        for (const FloorPlan::Model::Room& Room : Model.Rooms)
        {
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
            if (Boundary.Num() < 3 ||
                !FFloorPlanMeshBuilder::BuildFloor(Boundary, FloorSlabThicknessMm, Mesh,
                                                   Placement, MeshReport))
            {
                continue;
            }

            AFloorPlanRoofActor* Actor = World.SpawnActor<AFloorPlanRoofActor>(
                Placement.GetLocation() + RoofLift, Placement.Rotator());
            if (Actor == nullptr)
            {
                continue;
            }
            Actor->ElementId = ToUnreal(Room.Id);
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

    for (std::size_t Index = 0; Index < Model.Walls.size(); ++Index)
    {
        const FloorPlan::Model::Wall& Wall = Model.Walls[Index];
        FFloorPlanWallShape Shape;
        Shape.StartMm = FVector2D(Wall.Start.X * Scale, Wall.Start.Y * Scale);
        Shape.EndMm = FVector2D(Wall.End.X * Scale, Wall.End.Y * Scale);
        Shape.Bulge = Wall.Bulge;
        Shape.ThicknessMm = Wall.ThicknessMm * Scale;
        Shape.HeightMm = Wall.HeightMm;
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
