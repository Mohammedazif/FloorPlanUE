#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

class UStaticMesh;

/// Writes a generated mesh out as a StaticMesh asset so Nanite, Lumen and baked lighting apply.
class FFloorPlanStaticMeshBaker
{
public:
    /// Null outside the editor, and null when the asset could not be created.
    static UStaticMesh* Bake(const UE::Geometry::FDynamicMesh3& Source,
                             const FString& AssetPathAndName, bool bEnableNanite);

    /// Strips whatever a source filename contains that a package path cannot.
    static FString SanitiseAssetName(const FString& Name);
};
