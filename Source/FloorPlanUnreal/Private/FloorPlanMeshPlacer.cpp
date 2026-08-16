#include "FloorPlanMeshPlacer.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "FloorPlanElementActors.h"
#include "FloorPlanImportOptions.h"
#include "FloorPlanMeshBuilder.h"
#include "FloorPlanStaticMeshBaker.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "UDynamicMesh.h"

using UE::Geometry::FDynamicMesh3;

bool FFloorPlanMeshPlacer::Place(const UFloorPlanImportOptions& Options,
                                  const FString& AssetFolder, const FString& AssetName,
                                  UMaterialInterface* Material, const FDynamicMesh3& Mesh,
                                  AFloorPlanElementActor* Actor)
{
    UStaticMeshComponent* MeshComponent =
        Actor != nullptr ? Actor->GetStaticMeshComponent() : nullptr;
    if (MeshComponent == nullptr)
    {
        return false;
    }
    if (Options.bBakeToStaticMesh)
    {
        UStaticMesh* Baked = FFloorPlanStaticMeshBaker::Bake(Mesh, AssetFolder / AssetName,
                                                             Options.bEnableNanite, Material);
        if (Baked != nullptr)
        {
            MeshComponent->SetStaticMesh(Baked);
            // An override would hide asset edits; set one only when the bake swapped the slot.
            if (Material != nullptr && Material != Baked->GetMaterial(0))
            {
                MeshComponent->SetMaterial(0, Material);
            }
            return true;
        }
    }

    // With no mesh on the root the actor still shows one material slot, this one.
    UDynamicMeshComponent* Preview = NewObject<UDynamicMeshComponent>(Actor);
    Preview->SetupAttachment(MeshComponent);
    Preview->RegisterComponent();
    Actor->AddInstanceComponent(Preview);
    FFloorPlanMeshBuilder::CopyToDynamicMesh(Mesh, Preview->GetDynamicMesh());
    Preview->NotifyMeshUpdated();
    if (Material != nullptr)
    {
        Preview->SetMaterial(0, Material);
    }
    return false;
}

bool FFloorPlanMeshPlacer::PlaceHiddenCaster(const UFloorPlanImportOptions& Options,
                                              const FString& AssetFolder,
                                              const FString& AssetName, const FDynamicMesh3& Mesh,
                                              AActor* Host, const FTransform& Placement)
{
    if (!Options.bBakeToStaticMesh || Host == nullptr)
    {
        return false;
    }
    UStaticMesh* Baked = FFloorPlanStaticMeshBaker::Bake(Mesh, AssetFolder / AssetName,
                                                         Options.bEnableNanite, nullptr);
    if (Baked == nullptr)
    {
        return false;
    }
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Host);
    Component->SetStaticMesh(Baked);
    Component->SetVisibility(false);
    Component->bCastHiddenShadow = true;
    Component->SetupAttachment(Host->GetRootComponent());
    Component->RegisterComponent();
    Component->SetWorldTransform(Placement);
    Host->AddInstanceComponent(Component);
    return true;
}
