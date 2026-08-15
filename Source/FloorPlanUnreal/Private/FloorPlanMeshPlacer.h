#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

class ADynamicMeshActor;
class UFloorPlanImportOptions;
class UMaterialInterface;

/// Puts a built mesh on an actor, as a StaticMesh asset when baking or a dynamic mesh otherwise.
class FFloorPlanMeshPlacer
{
public:
    /// True only when a StaticMesh asset was written and attached.
    static bool Place(const UFloorPlanImportOptions& Options, const FString& AssetFolder,
                      const FString& AssetName, UMaterialInterface* Material,
                      const UE::Geometry::FDynamicMesh3& Mesh, ADynamicMeshActor* Actor);

    /// Attaches an invisible mesh that still casts shadows. Baking only; false otherwise.
    static bool PlaceHiddenCaster(const UFloorPlanImportOptions& Options,
                                  const FString& AssetFolder, const FString& AssetName,
                                  const UE::Geometry::FDynamicMesh3& Mesh,
                                  ADynamicMeshActor* Actor);
};
