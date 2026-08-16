#include "FloorPlanElementActors.h"

#include "Components/SceneComponent.h"

AFloorPlanBuildingActor::AFloorPlanBuildingActor()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    // The static element actors below refuse to attach to a more mobile parent than themselves.
    RootComponent->Mobility = EComponentMobility::Static;
}

FString AFloorPlanBuildingActor::Describe() const
{
    return FString::Printf(TEXT("%s - %d storey(s), %d room(s), %d wall(s), %d stair(s), "
                                "%d opening(s), %.2f m2"),
                           *FPaths::GetCleanFilename(SourceFile), StoreyCount, RoomCount,
                           WallCount, StairCount, OpeningCount, TotalFloorAreaSquareMetres);
}

AFloorPlanStoreyActor::AFloorPlanStoreyActor()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->Mobility = EComponentMobility::Static;
}

FString AFloorPlanStoreyActor::Describe() const
{
    return FString::Printf(TEXT("%s at %.0f mm - %d room(s), %d wall(s), %d stair(s), "
                                "%d column(s), %d fixture(s), %.2f m2"),
                           *StoreyName, ElevationMm, RoomCount, WallCount, StairCount,
                           ColumnCount, FixtureCount, TotalFloorAreaSquareMetres);
}

AFloorPlanShadowActor::AFloorPlanShadowActor()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->Mobility = EComponentMobility::Static;
}

FString AFloorPlanShadowActor::Describe() const
{
    return FString::Printf(TEXT("%d invisible shadow caster(s) for %s"), CasterCount,
                           *StoreyName);
}

FString AFloorPlanRoomActor::Describe() const
{
    const FString Label = RoomName.IsEmpty() ? TEXT("Unnamed") : RoomName;
    return FString::Printf(TEXT("%s - %.2f m2 - %d connected, %d adjacent - %s"), *Label,
                           FloorAreaSquareMetres, ConnectedRoomIds.Num(), AdjacentRoomIds.Num(),
                           *ElementId);
}

FString AFloorPlanRoofActor::Describe() const
{
    return FString::Printf(TEXT("Roof over %s - %s"), *StoreyName, *ElementId);
}

FString AFloorPlanColumnActor::Describe() const
{
    return FString::Printf(TEXT("Column %.0f x %.0f mm - %s"), WidthMm, DepthMm, *ElementId);
}

FString AFloorPlanFixtureActor::Describe() const
{
    return FString::Printf(TEXT("%s on %s - %s"), *BlockName, *SourceLayer, *ElementId);
}

FString AFloorPlanStairActor::Describe() const
{
    const FString Arrival =
        ArrivesAtStorey.IsEmpty() ? TEXT("nowhere") : *ArrivesAtStorey;
    return FString::Printf(TEXT("%d steps of %.0f mm rising %.0f mm to %s - %s"), StepCount,
                           RiserHeightMm, TotalRiseMm, *Arrival, *ElementId);
}

FString AFloorPlanWallActor::Describe() const
{
    return FString::Printf(TEXT("%s wall %.0f mm long, %.0f mm thick, %d opening(s) - %s"),
                           bIsExterior ? TEXT("External") : TEXT("Internal"), LengthMm,
                           ThicknessMm, OpeningCount, *ElementId);
}
