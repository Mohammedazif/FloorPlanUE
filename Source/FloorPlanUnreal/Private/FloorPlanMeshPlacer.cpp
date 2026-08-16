#include "FloorPlanMeshPlacer.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "FloorPlanImportOptions.h"
#include "FloorPlanMeshBuilder.h"
#include "FloorPlanStaticMeshBaker.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "UDynamicMesh.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
    void Attach(AActor& Actor, USceneComponent& Component)
    {
        Component.SetupAttachment(Actor.GetRootComponent());
        Component.RegisterComponent();
        Actor.AddInstanceComponent(&Component);
    }
}

bool FFloorPlanMeshPlacer::Place(const UFloorPlanImportOptions& Options,
                                  const FString& AssetFolder, const FString& AssetName,
                                  UMaterialInterface* Material, const FDynamicMesh3& Mesh,
                                  AActor* Actor)
{
    if (Actor == nullptr)
    {
        return false;
    }
    if (Options.bBakeToStaticMesh)
    {
        UStaticMesh* Baked = FFloorPlanStaticMeshBaker::Bake(Mesh, AssetFolder / AssetName,
                                                             Options.bEnableNanite, Material);
        if (Baked != nullptr)
        {
            UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Actor);
            Component->SetStaticMesh(Baked);
            Attach(*Actor, *Component);
            // An override would hide asset edits; set one only when the bake swapped the slot.
            if (Material != nullptr && Material != Baked->GetMaterial(0))
            {
                Component->SetMaterial(0, Material);
            }
            return true;
        }
    }

    UDynamicMeshComponent* Component = NewObject<UDynamicMeshComponent>(Actor);
    Attach(*Actor, *Component);
    FFloorPlanMeshBuilder::CopyToDynamicMesh(Mesh, Component->GetDynamicMesh());
    Component->NotifyMeshUpdated();
    if (Material != nullptr)
    {
        Component->SetMaterial(0, Material);
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
    Attach(*Host, *Component);
    Component->SetWorldTransform(Placement);
    return true;
}
