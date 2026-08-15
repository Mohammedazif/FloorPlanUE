#include "FloorPlanImporter.h"

#include "Engine/World.h"
#include "FloorPlanContactShadows.h"
#include "FloorPlanElementActors.h"
#include "FloorPlanElementSpawner.h"
#include "FloorPlanMeshBuilder.h"
#include "FloorPlanStaticMeshBaker.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

THIRD_PARTY_INCLUDES_START
#include "Dxf/DxfParser.h"
#include "Dxf/DxfSource.h"
#include "Model/BuildingJson.h"
#include "Model/FloorPlanCompiler.h"
#include "Model/RoomGraph.h"
#include "Model/StoreyLink.h"
THIRD_PARTY_INCLUDES_END

using FloorPlan::Model::BuildingModel;
using FloorPlan::Model::RoomGraph;
using FloorPlan::Model::RoomLink;
using FloorPlan::Model::StoreyConnection;
using FloorPlan::Model::StoreyLink;

namespace
{
    FString ToUnreal(const std::string& Text)
    {
        return FString(UTF8_TO_TCHAR(Text.c_str()));
    }

    std::string ToStd(const FString& Text)
    {
        return std::string(TCHAR_TO_UTF8(*Text));
    }

    FloorPlan::Model::CompilerOptions MakeOptions(const UFloorPlanImportOptions& Source,
                                                  const FString& StoreyName)
    {
        FloorPlan::Model::CompilerOptions Options;
        Options.Convention = Source.Convention == EFloorPlanWallConvention::SingleLine
                                 ? FloorPlan::Model::WallConvention::SingleLine
                                 : FloorPlan::Model::WallConvention::DoubleLine;
        Options.MillimetresPerUnit = Source.MillimetresPerDrawingUnit;
        Options.WallHeightMm = Source.WallHeightMm;
        Options.StoreyKey = ToStd(StoreyName);

        Options.WallLayers.clear();
        for (const FString& Layer : Source.WallLayers)
        {
            Options.WallLayers.push_back(ToStd(Layer));
        }
        Options.DoorBlockPrefixes.clear();
        for (const FString& Prefix : Source.DoorBlockPrefixes)
        {
            Options.DoorBlockPrefixes.push_back(ToStd(Prefix));
        }
        Options.WindowBlockPrefixes.clear();
        for (const FString& Prefix : Source.WindowBlockPrefixes)
        {
            Options.WindowBlockPrefixes.push_back(ToStd(Prefix));
        }
        return Options;
    }

    bool CompilePlan(const FString& FilePath, const UFloorPlanImportOptions& Options,
                     const FString& StoreyName, BuildingModel& Model, FString& Failure)
    {
        const FloorPlan::Dxf::DxfSource Source =
            FloorPlan::Dxf::DxfSource::FromFile(ToStd(FilePath));
        if (!Source.IsValid())
        {
            Failure = ToUnreal(FloorPlan::Format(Source.Failure()));
            return false;
        }

        const auto Reader = Source.OpenReader();
        FloorPlan::Dxf::DxfDocument Document;
        FloorPlan::Dxf::DxfParser Parser(*Reader);
        if (!Parser.Parse(Document))
        {
            Failure = ToUnreal(FloorPlan::Format(Parser.Failure()));
            return false;
        }
        if (Options.bOverrideDeclaredUnits)
        {
            Document.HasInsertUnits = false;
        }

        FloorPlan::Diagnostic Diagnostic;
        if (!FloorPlan::Model::FloorPlanCompiler::Compile(
                Document, MakeOptions(Options, StoreyName), Model, Diagnostic))
        {
            Failure = ToUnreal(FloorPlan::Format(Diagnostic));
            return false;
        }
        return true;
    }

    AFloorPlanStoreyActor* SpawnStorey(UWorld& World, const FFloorPlanStorey& Plan,
                                       AFloorPlanBuildingActor& Building)
    {
        AFloorPlanStoreyActor* Storey = World.SpawnActor<AFloorPlanStoreyActor>(
            FVector(0.0, 0.0, Plan.ElevationMm * FFloorPlanMeshBuilder::MillimetreToUnreal),
            FRotator::ZeroRotator);
        if (Storey == nullptr)
        {
            return nullptr;
        }
        Storey->StoreyName = Plan.Name;
        Storey->SourceFile = Plan.FilePath;
        Storey->ElevationMm = Plan.ElevationMm;
#if WITH_EDITOR
        Storey->SetActorLabel(Plan.Name);
#endif
        Storey->AttachToActor(&Building,
                              FAttachmentTransformRules(EAttachmentRule::KeepWorld, false));
        return Storey;
    }

    void CountLinks(const RoomGraph& Graph, FFloorPlanImportResult& Result)
    {
        for (const RoomLink& Link : Graph.Links())
        {
            ++Result.AdjacencyLinks;
            if (Link.IsTraversable())
            {
                ++Result.TraversableLinks;
            }
        }
    }

    void WriteExport(const UFloorPlanImportOptions& Options,
                     const std::vector<std::string>& Storeys,
                     const std::vector<std::string>& Connections,
                     FFloorPlanImportResult& Result)
    {
        if (Options.DataExportPath.IsEmpty())
        {
            return;
        }
        const FString Json =
            ToUnreal(FloorPlan::Model::BuildingJson::Building(Storeys, Connections));
        if (FFileHelper::SaveStringToFile(Json, *Options.DataExportPath))
        {
            Result.DataExportPath = Options.DataExportPath;
        }
    }
}

FFloorPlanImportResult UFloorPlanImporter::ImportDxfSimple(
    UObject* WorldContextObject, const FString& FilePath, const FString& WallLayer,
    EFloorPlanWallConvention Convention, double MillimetresPerDrawingUnit, double WallHeightMm,
    bool bBakeToStaticMesh)
{
    UFloorPlanImportOptions* Options = NewObject<UFloorPlanImportOptions>();
    Options->Convention = Convention;
    Options->WallHeightMm = WallHeightMm;
    Options->bBakeToStaticMesh = bBakeToStaticMesh;
    if (!WallLayer.IsEmpty())
    {
        Options->WallLayers.Add(WallLayer);
    }
    if (MillimetresPerDrawingUnit > 0.0)
    {
        Options->MillimetresPerDrawingUnit = MillimetresPerDrawingUnit;
        Options->bOverrideDeclaredUnits = true;
    }
    return ImportDxf(WorldContextObject, FilePath, Options);
}

FFloorPlanImportResult UFloorPlanImporter::ImportDxf(UObject* WorldContextObject,
                                                      const FString& FilePath,
                                                      UFloorPlanImportOptions* Options)
{
    FFloorPlanStorey Plan;
    Plan.FilePath = FilePath;
    Plan.Name = Options != nullptr ? Options->StoreyName : FString();
    Plan.ElevationMm = Options != nullptr ? Options->StoreyElevationMm : 0.0;
    if (Plan.Name.IsEmpty())
    {
        Plan.Name = FPaths::GetBaseFilename(FilePath);
    }
    return ImportBuilding(WorldContextObject, {Plan}, Options);
}

FFloorPlanImportResult UFloorPlanImporter::ImportBuilding(
    UObject* WorldContextObject, const TArray<FFloorPlanStorey>& Storeys,
    UFloorPlanImportOptions* Options)
{
    FFloorPlanImportResult Result;

    UWorld* World = GEngine != nullptr
                        ? GEngine->GetWorldFromContextObject(
                              WorldContextObject, EGetWorldErrorMode::ReturnNull)
                        : nullptr;
    if (World == nullptr)
    {
        Result.Diagnostic = TEXT("No world context");
        return Result;
    }
    if (Options == nullptr)
    {
        Result.Diagnostic = TEXT("No import options supplied");
        return Result;
    }
    if (Storeys.Num() == 0)
    {
        Result.Diagnostic = TEXT("No storeys to import");
        return Result;
    }

    AFloorPlanBuildingActor* Building = World->SpawnActor<AFloorPlanBuildingActor>();
    if (Building == nullptr)
    {
        Result.Diagnostic = TEXT("Could not spawn the building root actor");
        return Result;
    }
    Building->SourceFile = Storeys[0].FilePath;
#if WITH_EDITOR
    Building->SetActorLabel(FPaths::GetBaseFilename(Storeys[0].FilePath));
#endif
    Result.BuildingActor = Building;

    TArray<BuildingModel> Models;
    Models.SetNum(Storeys.Num());
    for (int32 Index = 0; Index < Storeys.Num(); ++Index)
    {
        FString Failure;
        if (!CompilePlan(Storeys[Index].FilePath, *Options, Storeys[Index].Name, Models[Index],
                         Failure))
        {
            Result.Diagnostic =
                FString::Printf(TEXT("%s: %s"), *Storeys[Index].Name, *Failure);
            return Result;
        }
    }

    std::vector<std::string> Documents;
    for (int32 Index = 0; Index < Storeys.Num(); ++Index)
    {
        const FFloorPlanStorey& Plan = Storeys[Index];
        const BuildingModel& Model = Models[Index];

        AFloorPlanStoreyActor* Storey = SpawnStorey(*World, Plan, *Building);
        if (Storey == nullptr)
        {
            Result.Diagnostic = TEXT("Could not spawn a storey actor");
            return Result;
        }

        // A flight on the top storey has nothing above it, so it climbs to ceiling height.
        FFloorPlanStoreyRise Rise;
        Rise.RiseMm = Options->WallHeightMm;
        Rise.bSeatedOnStorey = Index > 0;
        if (Index + 1 < Storeys.Num())
        {
            Rise.RiseMm = Storeys[Index + 1].ElevationMm - Plan.ElevationMm;
            Rise.ArrivesAtStorey = Storeys[Index + 1].Name;
        }

        const RoomGraph Graph = RoomGraph::Build(Model);
        const FString AssetFolder =
            Options->StaticMeshFolder /
            FFloorPlanStaticMeshBaker::SanitiseAssetName(
                FPaths::GetBaseFilename(Plan.FilePath)) /
            FFloorPlanStaticMeshBaker::SanitiseAssetName(Plan.Name);

        FFloorPlanSpawnReport Spawned;
        FFloorPlanElementSpawner::Spawn(*World, Model, Graph, *Options, AssetFolder, Rise,
                                        *Storey, Spawned);

        Result.RoomCount += Spawned.Rooms;
        Result.WallCount += Spawned.Walls;
        Result.StairCount += Spawned.Stairs;
        Result.ColumnCount += Spawned.Columns;
        Result.FixtureCount += Spawned.Fixtures;
        for (const FloorPlan::Model::Dimension& Measured : Model.Dimensions)
        {
            if (!Measured.AgreesWithGeometry)
            {
                ++Result.ContradictedDimensions;
            }
        }
        Result.BakedMeshes += Spawned.BakedMeshes;
        Result.UnsealedMeshes += Spawned.UnsealedMeshes;
        Result.TotalFloorAreaSquareMetres += Spawned.FloorAreaSquareMetres;
        Result.SpawnedActors.Append(Spawned.Actors);
        Result.SpawnedActors.Add(Storey);
        Result.OpeningCount += static_cast<int32>(Model.Openings.size());
        Result.MillimetresPerDrawingUnit = Model.MillimetresPerUnit;
        Result.bUnitsWereDeclared = Model.UnitsWereDeclared;
        ++Result.StoreyCount;
        CountLinks(Graph, Result);

        for (const std::string& Label : Model.UnassignedLabels)
        {
            Result.UnassignedLabels.Add(ToUnreal(Label));
        }
        Documents.push_back(FloorPlan::Model::BuildingJson::Storey(
            Model, Graph, ToStd(Plan.Name), Plan.ElevationMm));
    }

    std::vector<std::string> Connections;
    for (int32 Index = 0; Index + 1 < Storeys.Num(); ++Index)
    {
        for (const StoreyConnection& Link : StoreyLink::Between(Models[Index], Models[Index + 1]))
        {
            Connections.push_back(FloorPlan::Model::BuildingJson::Connection(
                Models[Index], Models[Index + 1], ToStd(Storeys[Index].Name),
                ToStd(Storeys[Index + 1].Name), Link));
            ++Result.VerticalLinks;
        }
    }

    Building->StoreyCount = Result.StoreyCount;
    Building->StairCount = Result.StairCount;
    Building->ColumnCount = Result.ColumnCount;
    Building->FixtureCount = Result.FixtureCount;
    Building->RoomCount = Result.RoomCount;
    Building->WallCount = Result.WallCount;
    Building->OpeningCount = Result.OpeningCount;
    Building->TotalFloorAreaSquareMetres = Result.TotalFloorAreaSquareMetres;

    if (Options->bEnsureSunContactShadows)
    {
        Result.ContactShadowLightsChanged =
            FFloorPlanContactShadows::EnsureOnDirectionalLights(*World);
    }

    WriteExport(*Options, Documents, Connections, Result);
    Result.bSucceeded = true;
    return Result;
}
