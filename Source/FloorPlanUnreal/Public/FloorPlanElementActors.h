#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"

#include "FloorPlanElementActors.generated.h"

/// Parent of everything one DXF produced; moving it moves the whole building.
UCLASS(BlueprintType)
class FLOORPLANUNREAL_API AFloorPlanBuildingActor : public AActor
{
    GENERATED_BODY()

public:
    AFloorPlanBuildingActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    FString SourceFile;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 StoreyCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 RoomCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 WallCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 OpeningCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 StairCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 ColumnCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 FixtureCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    double TotalFloorAreaSquareMetres = 0.0;

    UFUNCTION(BlueprintCallable, Category = "Floor Plan")
    FString Describe() const;
};

/// One plan of a building, holding the rooms and walls drawn at a single elevation.
UCLASS(BlueprintType)
class FLOORPLANUNREAL_API AFloorPlanStoreyActor : public AActor
{
    GENERATED_BODY()

public:
    AFloorPlanStoreyActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    FString StoreyName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    FString SourceFile;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    double ElevationMm = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 RoomCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 WallCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 StairCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 ColumnCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 FixtureCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    double TotalFloorAreaSquareMetres = 0.0;

    UFUNCTION(BlueprintCallable, Category = "Floor Plan")
    FString Describe() const;
};

/// A storey's invisible shadow casters, kept off the actors they shade so those keep one
/// material slot each.
UCLASS(BlueprintType)
class FLOORPLANUNREAL_API AFloorPlanShadowActor : public AActor
{
    GENERATED_BODY()

public:
    AFloorPlanShadowActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    FString StoreyName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 CasterCount = 0;

    UFUNCTION(BlueprintCallable, Category = "Floor Plan")
    FString Describe() const;
};

/// One drawn element. It is a static mesh actor, so its mesh and material sit in the Details
/// panel exactly where they do on any static mesh you place by hand.
UCLASS(Abstract)
class FLOORPLANUNREAL_API AFloorPlanElementActor : public AStaticMeshActor
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    FString ElementId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    FString StoreyName;
};

/// A room, carrying the stable identity that makes the import data rather than a picture.
UCLASS(BlueprintType)
class FLOORPLANUNREAL_API AFloorPlanRoomActor : public AFloorPlanElementActor
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    FString RoomName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    double FloorAreaSquareMetres = 0.0;

    /// Rooms sharing a wall with this one, whether or not you can walk between them.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    TArray<FString> AdjacentRoomIds;

    /// Rooms reachable from this one through a door or window.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    TArray<FString> ConnectedRoomIds;

    /// True when at least one of this room's walls faces the outside of the building.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    bool bTouchesOutside = false;

    UFUNCTION(BlueprintCallable, Category = "Floor Plan")
    FString Describe() const;
};

/// The slab capping a room of the topmost storey, so the sun stays outside the building.
UCLASS(BlueprintType)
class FLOORPLANUNREAL_API AFloorPlanRoofActor : public AFloorPlanElementActor
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Floor Plan")
    FString Describe() const;
};

/// A structural column, extruded from the profile it was drawn as.
UCLASS(BlueprintType)
class FLOORPLANUNREAL_API AFloorPlanColumnActor : public AFloorPlanElementActor
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    double WidthMm = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    double DepthMm = 0.0;

    UFUNCTION(BlueprintCallable, Category = "Floor Plan")
    FString Describe() const;
};

/// A block the drawing places but this plugin does not model: furniture, fittings, equipment.
/// It carries the name and transform so you can attach your own mesh to it.
UCLASS(BlueprintType)
class FLOORPLANUNREAL_API AFloorPlanFixtureActor : public AFloorPlanElementActor
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    FString BlockName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    FString SourceLayer;

    UFUNCTION(BlueprintCallable, Category = "Floor Plan")
    FString Describe() const;
};

/// A flight of steps climbing from one storey to the next.
UCLASS(BlueprintType)
class FLOORPLANUNREAL_API AFloorPlanStairActor : public AFloorPlanElementActor
{
    GENERATED_BODY()

public:
    /// Storey this flight arrives at, empty when nothing was found stacked above it.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    FString ArrivesAtStorey;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 StepCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    double RiserHeightMm = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    double TreadDepthMm = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    double TotalRiseMm = 0.0;

    /// True when the step count came from treads drawn in the plan rather than from a rule.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    bool bFromDrawnTreads = false;

    UFUNCTION(BlueprintCallable, Category = "Floor Plan")
    FString Describe() const;
};

/// A wall segment with its measured thickness and the identity of the run it came from.
UCLASS(BlueprintType)
class FLOORPLANUNREAL_API AFloorPlanWallActor : public AFloorPlanElementActor
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    double ThicknessMm = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    double LengthMm = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    int32 OpeningCount = 0;

    /// True when this wall has the outside of the building on one of its faces.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor Plan")
    bool bIsExterior = false;

    UFUNCTION(BlueprintCallable, Category = "Floor Plan")
    FString Describe() const;
};
