#include "FloorPlanStoreyMeshMerger.h"

#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMeshEditor.h"
#include "Engine/StaticMesh.h"
#include "FloorPlanElementActors.h"
#include "FloorPlanImportOptions.h"
#include "FloorPlanStaticMeshBaker.h"
#include "Materials/MaterialInterface.h"

using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FDynamicMeshEditor;
using UE::Geometry::FDynamicMeshMaterialAttribute;
using UE::Geometry::FMeshIndexMappings;

FFloorPlanStoreyMeshMerger::FFloorPlanStoreyMeshMerger()
{
    Merged.EnableAttributes();
    Merged.Attributes()->EnableMaterialID();
}

void FFloorPlanStoreyMeshMerger::Append(const FDynamicMesh3& Mesh, const FTransform& Placement,
                                        int32 MaterialSlot)
{
    const int32 FirstAddedTriangle = Merged.MaxTriangleID();
    FMeshIndexMappings Mappings;
    FDynamicMeshEditor Editor(&Merged);
    Editor.AppendMesh(
        &Mesh, Mappings,
        [&Placement](int32, const FVector3d& Position)
        { return FVector3d(Placement.TransformPosition(FVector(Position))); },
        [&Placement](int32, const FVector3d& Normal)
        { return FVector3d(Placement.TransformVectorNoScale(FVector(Normal))); });

    FDynamicMeshMaterialAttribute* MaterialIDs = Merged.Attributes()->GetMaterialID();
    for (int32 TriangleID = FirstAddedTriangle; TriangleID < Merged.MaxTriangleID(); ++TriangleID)
    {
        if (Merged.IsTriangle(TriangleID))
        {
            MaterialIDs->SetValue(TriangleID, MaterialSlot);
        }
    }
}

bool FFloorPlanStoreyMeshMerger::Attach(const UFloorPlanImportOptions& Options,
                                        const FString& AssetFolder,
                                        AFloorPlanStoreyActor& Storey) const
{
    if (Merged.TriangleCount() == 0)
    {
        return false;
    }

    const FString AssetName = FFloorPlanStaticMeshBaker::SanitiseAssetName(
        FString::Printf(TEXT("Storey_%s"), *Storey.StoreyName));
    TArray<UMaterialInterface*> Materials;
    Materials.Add(Options.WallMaterial);
    Materials.Add(Options.FloorMaterial);
    UStaticMesh* Baked = FFloorPlanStaticMeshBaker::Bake(Merged, AssetFolder / AssetName,
                                                         Options.bEnableNanite, Materials);
    if (Baked == nullptr)
    {
        return false;
    }

    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(&Storey);
    Component->SetStaticMesh(Baked);
    Component->SetupAttachment(Storey.GetRootComponent());
    Component->RegisterComponent();
    Storey.AddInstanceComponent(Component);
    return true;
}
