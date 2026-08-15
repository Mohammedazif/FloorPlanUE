#include "FloorPlanMeshPlacer.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMeshActor.h"
#include "FloorPlanImportOptions.h"
#include "FloorPlanMeshBuilder.h"
#include "FloorPlanStaticMeshBaker.h"
#include "Materials/MaterialInterface.h"
#include "UDynamicMesh.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
    void ApplyMaterial(UPrimitiveComponent* Component, UMaterialInterface* Material)
    {
        if (Component != nullptr && Material != nullptr)
        {
            Component->SetMaterial(0, Material);
        }
    }

    bool AttachBakedMesh(ADynamicMeshActor* Actor, UStaticMesh* Baked,
                         UMaterialInterface* Material)
    {
        if (Actor == nullptr || Baked == nullptr)
        {
            return false;
        }
        UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Actor);
        Component->SetStaticMesh(Baked);
        Component->SetupAttachment(Actor->GetRootComponent());
        Component->RegisterComponent();
        Actor->AddInstanceComponent(Component);
        // An override would hide asset edits; set one only when the bake swapped the slot.
        if (Material != nullptr && Material != Baked->GetMaterial(0))
        {
            ApplyMaterial(Component, Material);
        }
        if (UDynamicMeshComponent* Dynamic = Actor->GetDynamicMeshComponent())
        {
            Dynamic->SetVisibility(false);
        }
        return true;
    }
}

bool FFloorPlanMeshPlacer::Place(const UFloorPlanImportOptions& Options,
                                  const FString& AssetFolder, const FString& AssetName,
                                  UMaterialInterface* Material, const FDynamicMesh3& Mesh,
                                  ADynamicMeshActor* Actor)
{
    if (Options.bBakeToStaticMesh)
    {
        UStaticMesh* Baked = FFloorPlanStaticMeshBaker::Bake(Mesh, AssetFolder / AssetName,
                                                             Options.bEnableNanite, Material);
        if (AttachBakedMesh(Actor, Baked, Material))
        {
            return true;
        }
    }
    UDynamicMeshComponent* Component = Actor->GetDynamicMeshComponent();
    FFloorPlanMeshBuilder::CopyToDynamicMesh(Mesh, Component->GetDynamicMesh());
    ApplyMaterial(Component, Material);
    return false;
}

bool FFloorPlanMeshPlacer::PlaceHiddenCaster(const UFloorPlanImportOptions& Options,
                                              const FString& AssetFolder,
                                              const FString& AssetName, const FDynamicMesh3& Mesh,
                                              ADynamicMeshActor* Actor)
{
    if (!Options.bBakeToStaticMesh || Actor == nullptr)
    {
        return false;
    }
    UStaticMesh* Baked = FFloorPlanStaticMeshBaker::Bake(Mesh, AssetFolder / AssetName,
                                                         Options.bEnableNanite, nullptr);
    if (Baked == nullptr)
    {
        return false;
    }
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Actor);
    Component->SetStaticMesh(Baked);
    Component->SetVisibility(false);
    Component->bCastHiddenShadow = true;
    Component->SetupAttachment(Actor->GetRootComponent());
    Component->RegisterComponent();
    Actor->AddInstanceComponent(Component);
    return true;
}
