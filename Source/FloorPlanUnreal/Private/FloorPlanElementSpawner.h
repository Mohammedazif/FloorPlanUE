#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "Model/BuildingModel.h"
#include "Model/RoomGraph.h"
THIRD_PARTY_INCLUDES_END

class AFloorPlanStoreyActor;
class UFloorPlanImportOptions;
class UWorld;

/// What one storey's worth of spawning produced.
struct FFloorPlanSpawnReport
{
    int32 Rooms = 0;
    int32 Walls = 0;
    int32 Stairs = 0;
    int32 Columns = 0;
    int32 Fixtures = 0;
    int32 BakedMeshes = 0;
    int32 UnsealedMeshes = 0;
    double FloorAreaSquareMetres = 0.0;
    TArray<AActor*> Actors;
};

/// How far a flight has to climb, and what it arrives at.
struct FFloorPlanStoreyRise
{
    double RiseMm = 0.0;
    FString ArrivesAtStorey;
};

/// Turns a compiled plan into room, wall and stair actors beneath one storey.
class FFloorPlanElementSpawner
{
public:
    static void Spawn(UWorld& World, const FloorPlan::Model::BuildingModel& Model,
                      const FloorPlan::Model::RoomGraph& Graph,
                      const UFloorPlanImportOptions& Options, const FString& AssetFolder,
                      const FFloorPlanStoreyRise& Rise, AFloorPlanStoreyActor& Storey,
                      FFloorPlanSpawnReport& Report);
};
