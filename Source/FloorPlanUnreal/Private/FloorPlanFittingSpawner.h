#pragma once

#include "CoreMinimal.h"
#include "FloorPlanElementSpawner.h"

class AFloorPlanStoreyActor;
class UFloorPlanImportOptions;
class UWorld;

/// Spawns the fittings a storey carries: its flights of steps, columns and placed blocks.
class FFloorPlanFittingSpawner
{
public:
    static void Spawn(UWorld& World, const FloorPlan::Model::BuildingModel& Model,
                      const UFloorPlanImportOptions& Options, const FString& AssetFolder,
                      const FFloorPlanStoreyRise& Rise, AFloorPlanStoreyActor& Storey,
                      const FVector& Lift, const FAttachmentTransformRules& AttachRules,
                      FFloorPlanSpawnReport& Report);
};
