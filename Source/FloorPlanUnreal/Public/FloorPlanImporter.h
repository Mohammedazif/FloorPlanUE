#pragma once

#include "CoreMinimal.h"
#include "FloorPlanImportOptions.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "FloorPlanImporter.generated.h"

/// One plan file and where it sits in the building.
USTRUCT(BlueprintType)
struct FLOORPLANUNREAL_API FFloorPlanStorey
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Plan")
    FString FilePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Plan")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Plan")
    double ElevationMm = 0.0;
};

USTRUCT(BlueprintType)
struct FLOORPLANUNREAL_API FFloorPlanImportResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    bool bSucceeded = false;

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    FString Diagnostic;

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 StoreyCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 RoomCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 WallCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 StairCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 ColumnCount = 0;

    /// Blocks placed with a name and transform for you to attach your own meshes to.
    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 FixtureCount = 0;

    /// Dimensions whose stated measurement contradicts the geometry they span.
    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 ContradictedDimensions = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 OpeningCount = 0;

    /// Stairs and lifts that continue from one storey into the plan above it.
    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 VerticalLinks = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    double TotalFloorAreaSquareMetres = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    double MillimetresPerDrawingUnit = 1.0;

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    bool bUnitsWereDeclared = false;

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    TArray<FString> UnassignedLabels;

    /// Elements whose mesh has an open boundary. A solid wall must never appear here.
    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 UnsealedMeshes = 0;

    /// Elements written out as StaticMesh assets. Zero when baking was off or unavailable.
    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 BakedMeshes = 0;

    /// Directional lights whose contact shadow trace the import switched on.
    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 ContactShadowLightsChanged = 0;

    /// Pairs of spaces separated by wall material, counting the outside as one space.
    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 AdjacencyLinks = 0;

    /// Links you can actually walk or see through because an opening sits in the wall.
    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    int32 TraversableLinks = 0;

    /// Where the JSON went, empty when no export was asked for or the write failed.
    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    FString DataExportPath;

    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    TArray<AActor*> SpawnedActors;

    /// Parent of every spawned element; move this to move the whole building.
    UPROPERTY(BlueprintReadOnly, Category = "Floor Plan")
    AActor* BuildingActor = nullptr;
};

/// Entry point: reads a DXF and spawns one actor per room and per wall, each with a stable id.
UCLASS()
class FLOORPLANUNREAL_API UFloorPlanImporter : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Floor Plan",
              meta = (WorldContext = "WorldContextObject"))
    static FFloorPlanImportResult ImportDxf(UObject* WorldContextObject, const FString& FilePath,
                                            UFloorPlanImportOptions* Options);

    /// Stacks one plan per storey under a single building; each storey keeps its own ids.
    UFUNCTION(BlueprintCallable, Category = "Floor Plan",
              meta = (WorldContext = "WorldContextObject"))
    static FFloorPlanImportResult ImportBuilding(UObject* WorldContextObject,
                                                 const TArray<FFloorPlanStorey>& Storeys,
                                                 UFloorPlanImportOptions* Options);

    /// One-node entry point; pass an empty WallLayer to accept every layer.
    UFUNCTION(BlueprintCallable, Category = "Floor Plan",
              meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "3"))
    static FFloorPlanImportResult ImportDxfSimple(
        UObject* WorldContextObject, const FString& FilePath, const FString& WallLayer,
        EFloorPlanWallConvention Convention = EFloorPlanWallConvention::DoubleLine,
        double MillimetresPerDrawingUnit = 0.0, double WallHeightMm = 2700.0,
        bool bBakeToStaticMesh = false);
};
