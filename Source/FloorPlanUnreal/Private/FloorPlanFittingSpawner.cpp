#include "FloorPlanFittingSpawner.h"

#include "Components/DynamicMeshComponent.h"
#include "Engine/World.h"
#include "FloorPlanElementActors.h"
#include "FloorPlanImportOptions.h"
#include "FloorPlanMeshBuilder.h"
#include "FloorPlanMeshPlacer.h"
#include "FloorPlanStairMesh.h"
#include "UDynamicMesh.h"

THIRD_PARTY_INCLUDES_START
#include "Model/StairPlanner.h"
THIRD_PARTY_INCLUDES_END

using FloorPlan::Model::BuildingModel;
using UE::Geometry::FDynamicMesh3;

namespace
{
    constexpr double FloorSlabThicknessMm = 50.0;
    constexpr double MillimetreToUnreal = FFloorPlanMeshBuilder::MillimetreToUnreal;

    FString ToUnreal(const std::string& Text)
    {
        return FString(UTF8_TO_TCHAR(Text.c_str()));
    }

    void SpawnColumnsAndFixtures(UWorld& World, const BuildingModel& Model,
                                 const UFloorPlanImportOptions& Options,
                                 const FString& AssetFolder,
                                 AFloorPlanStoreyActor& Storey, const FVector& Lift,
                                 const FAttachmentTransformRules& AttachRules,
                                 FFloorPlanSpawnReport& Report)
    {
        const double Scale = Model.MillimetresPerUnit;
        const double Base = Options.bGenerateFloors ? -FloorSlabThicknessMm : 0.0;

        for (const FloorPlan::Model::Column& Column : Model.Columns)
        {
            if (!Column.HasProfile || Column.ProfileIndex >= Model.ColumnProfiles.size())
            {
                continue;
            }
            TArray<FVector2D> Boundary;
            for (const auto& Point : Model.ColumnProfiles[Column.ProfileIndex].Tessellated())
            {
                Boundary.Add(FVector2D(Point.X * Scale, Point.Y * Scale));
            }

            FDynamicMesh3 Mesh;
            FFloorPlanMeshReport MeshReport;
            FTransform Placement = FTransform::Identity;
            if (!FFloorPlanMeshBuilder::BuildPrism(Boundary, Base, Options.WallHeightMm, Mesh,
                                                   Placement, MeshReport))
            {
                continue;
            }

            AFloorPlanColumnActor* Actor = World.SpawnActor<AFloorPlanColumnActor>(
                Placement.GetLocation() + Lift, Placement.Rotator());
            if (Actor == nullptr)
            {
                continue;
            }
            Actor->ElementId = ToUnreal(Column.Id);
            Actor->StoreyName = Storey.StoreyName;
            Actor->WidthMm = Column.WidthMm * Scale;
            Actor->DepthMm = Column.DepthMm * Scale;
#if WITH_EDITOR
            Actor->SetActorLabel(FString::Printf(TEXT("Column_%s"), *Actor->ElementId.Left(8)));
#endif
            if (FFloorPlanMeshPlacer::Place(Options, AssetFolder,
                          FString::Printf(TEXT("Column_%s"), *Actor->ElementId.Left(8)),
                          Options.WallMaterial, Mesh, Actor))
            {
                ++Report.BakedMeshes;
            }
            Actor->AttachToActor(&Storey, AttachRules);
            Report.Actors.Add(Actor);
            ++Report.Columns;
        }

        for (const FloorPlan::Model::BlockInstance& Instance : Model.BlockInstances)
        {
            const FVector Location(Instance.Position.X * Scale * MillimetreToUnreal,
                                   Instance.Position.Y * Scale * MillimetreToUnreal, 0.0);
            AFloorPlanFixtureActor* Actor = World.SpawnActor<AFloorPlanFixtureActor>(
                Location + Lift, FRotator(0.0, Instance.RotationDegrees, 0.0));
            if (Actor == nullptr)
            {
                continue;
            }
            Actor->ElementId = ToUnreal(Instance.Id);
            Actor->BlockName = ToUnreal(Instance.BlockName);
            Actor->SourceLayer = ToUnreal(Instance.Layer);
            Actor->StoreyName = Storey.StoreyName;
            Actor->SetActorScale3D(
                FVector(FMath::Abs(Instance.ScaleX), FMath::Abs(Instance.ScaleY), 1.0));
#if WITH_EDITOR
            Actor->SetActorLabel(FString::Printf(TEXT("%s_%s"), *Actor->BlockName,
                                                 *Actor->ElementId.Left(8)));
#endif
            Actor->AttachToActor(&Storey, AttachRules);
            Report.Actors.Add(Actor);
            ++Report.Fixtures;
        }
    }

    void SpawnStairs(UWorld& World, const BuildingModel& Model,
                     const UFloorPlanImportOptions& Options, const FString& AssetFolder,
                     const FFloorPlanStoreyRise& Rise, AFloorPlanStoreyActor& Storey,
                     const FVector& Lift, const FAttachmentTransformRules& AttachRules,
                     FFloorPlanSpawnReport& Report)
    {
        const double Scale = Model.MillimetresPerUnit;
        for (const FloorPlan::Model::CirculationRegion& Region : Model.Circulation)
        {
            FloorPlan::Model::StairFlight Flight;
            if (!FloorPlan::Model::StairPlanner::Plan(Region, Rise.RiseMm, Flight))
            {
                continue;
            }

            FFloorPlanStairShape Shape;
            Shape.StartMm = FVector2D(Flight.Start.X * Scale, Flight.Start.Y * Scale);
            Shape.EndMm = FVector2D(Flight.End.X * Scale, Flight.End.Y * Scale);
            Shape.WidthMm = Flight.WidthMm * Scale;
            Shape.TreadDepthMm = Flight.TreadDepthMm * Scale;
            Shape.RiserHeightMm = Flight.RiserHeightMm;
            Shape.StepCount = static_cast<int32>(Flight.StepCount);
            Shape.BaseMm = Options.bGenerateFloors ? -FloorSlabThicknessMm : 0.0;

            FDynamicMesh3 Mesh;
            FFloorPlanMeshReport MeshReport;
            FTransform Placement = FTransform::Identity;
            if (!FFloorPlanStairMesh::Build(Shape, Mesh, Placement, MeshReport))
            {
                continue;
            }

            AFloorPlanStairActor* Actor = World.SpawnActor<AFloorPlanStairActor>(
                Placement.GetLocation() + Lift, Placement.Rotator());
            if (Actor == nullptr)
            {
                continue;
            }
            Actor->ElementId = ToUnreal(Model.Rooms[Region.RoomIndex].Id);
            Actor->StoreyName = Storey.StoreyName;
            Actor->ArrivesAtStorey = Rise.ArrivesAtStorey;
            Actor->StepCount = Shape.StepCount;
            Actor->RiserHeightMm = Flight.RiserHeightMm;
            Actor->TreadDepthMm = Shape.TreadDepthMm;
            Actor->TotalRiseMm = Rise.RiseMm;
            Actor->bFromDrawnTreads = Flight.bFromDrawnTreads;
#if WITH_EDITOR
            Actor->SetActorLabel(FString::Printf(TEXT("Stair_%s"), *Actor->ElementId.Left(8)));
#endif
            if (FFloorPlanMeshPlacer::Place(Options, AssetFolder,
                          FString::Printf(TEXT("Stair_%s"), *Actor->ElementId.Left(8)),
                          Options.FloorMaterial, Mesh, Actor))
            {
                ++Report.BakedMeshes;
            }
            if (MeshReport.OpenBoundaryEdges > 0)
            {
                ++Report.UnsealedMeshes;
            }

            Actor->AttachToActor(&Storey, AttachRules);
            Report.Actors.Add(Actor);
            ++Report.Stairs;
        }
    }
}

void FFloorPlanFittingSpawner::Spawn(UWorld& World, const BuildingModel& Model,
                                      const UFloorPlanImportOptions& Options,
                                      const FString& AssetFolder,
                                      const FFloorPlanStoreyRise& Rise,
                                      AFloorPlanStoreyActor& Storey, const FVector& Lift,
                                      const FAttachmentTransformRules& AttachRules,
                                      FFloorPlanSpawnReport& Report)
{
    SpawnStairs(World, Model, Options, AssetFolder, Rise, Storey, Lift, AttachRules, Report);
    SpawnColumnsAndFixtures(World, Model, Options, AssetFolder, Storey, Lift, AttachRules,
                            Report);
}
