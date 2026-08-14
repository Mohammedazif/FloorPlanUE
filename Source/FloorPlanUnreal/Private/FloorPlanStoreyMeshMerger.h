#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

class AFloorPlanStoreyActor;
class UFloorPlanImportOptions;

/// Collects every element mesh of one storey and bakes them as a single seamless asset.
class FFloorPlanStoreyMeshMerger
{
public:
    static constexpr int32 WallSlot = 0;
    static constexpr int32 FloorSlot = 1;

    FFloorPlanStoreyMeshMerger();

    void Append(const UE::Geometry::FDynamicMesh3& Mesh, const FTransform& Placement,
                int32 MaterialSlot);

    /// Bakes the collected mesh and mounts it on the storey; false when nothing was collected
    /// or the asset could not be created.
    bool Attach(const UFloorPlanImportOptions& Options, const FString& AssetFolder,
                AFloorPlanStoreyActor& Storey) const;

private:
    UE::Geometry::FDynamicMesh3 Merged;
};
