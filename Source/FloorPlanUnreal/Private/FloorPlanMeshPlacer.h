#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

class AActor;
class UFloorPlanImportOptions;
class UMaterialInterface;

/// Gives an actor its one mesh component: a StaticMesh asset when baking, a dynamic mesh if not.
class FFloorPlanMeshPlacer
{
public:
    /// True only when a StaticMesh asset was written and attached.
    static bool Place(const UFloorPlanImportOptions& Options, const FString& AssetFolder,
                      const FString& AssetName, UMaterialInterface* Material,
                      const UE::Geometry::FDynamicMesh3& Mesh, AActor* Actor);

    /// Adds an invisible mesh that still casts shadows, standing where Placement puts it.
    static bool PlaceHiddenCaster(const UFloorPlanImportOptions& Options,
                                  const FString& AssetFolder, const FString& AssetName,
                                  const UE::Geometry::FDynamicMesh3& Mesh, AActor* Host,
                                  const FTransform& Placement);
};
