#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

class AActor;
class AFloorPlanElementActor;
class UFloorPlanImportOptions;
class UMaterialInterface;

/// Fills an element actor's static mesh component when baking, or previews it dynamically.
class FFloorPlanMeshPlacer
{
public:
    /// True only when a StaticMesh asset was written onto the actor.
    static bool Place(const UFloorPlanImportOptions& Options, const FString& AssetFolder,
                      const FString& AssetName, UMaterialInterface* Material,
                      const UE::Geometry::FDynamicMesh3& Mesh, AFloorPlanElementActor* Actor);

    /// Adds an invisible mesh that still casts shadows, standing where Placement puts it.
    static bool PlaceHiddenCaster(const UFloorPlanImportOptions& Options,
                                  const FString& AssetFolder, const FString& AssetName,
                                  const UE::Geometry::FDynamicMesh3& Mesh, AActor* Host,
                                  const FTransform& Placement);
};
