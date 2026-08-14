#include "FloorPlanStaticMeshBaker.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMeshToMeshDescription.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSourceData.h"
#include "IAssetTools.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

namespace
{
    constexpr float BakedDistanceFieldResolutionScale = 2.0f;

    UMaterialInterface* DistanceFieldSafeMaterial(UMaterialInterface* Material)
    {
        if (Material != nullptr)
        {
            const EBlendMode Blend = Material->GetBlendMode();
            if (Blend == BLEND_Opaque || Blend == BLEND_Masked)
            {
                return Material;
            }
        }
        return UMaterial::GetDefaultMaterial(MD_Surface);
    }
}
#endif

FString FFloorPlanStaticMeshBaker::SanitiseAssetName(const FString& Name)
{
    FString Clean;
    Clean.Reserve(Name.Len());
    for (const TCHAR Character : Name)
    {
        const bool bAllowed = FChar::IsAlnum(Character) || Character == TEXT('_');
        Clean.AppendChar(bAllowed ? Character : TEXT('_'));
    }
    return Clean.IsEmpty() ? TEXT("FloorPlan") : Clean;
}

UStaticMesh* FFloorPlanStaticMeshBaker::Bake(const UE::Geometry::FDynamicMesh3& Source,
                                              const FString& AssetPathAndName, bool bEnableNanite,
                                              UMaterialInterface* Material)
{
    return Bake(Source, AssetPathAndName, bEnableNanite,
                TArray<UMaterialInterface*>{Material});
}

UStaticMesh* FFloorPlanStaticMeshBaker::Bake(const UE::Geometry::FDynamicMesh3& Source,
                                              const FString& AssetPathAndName, bool bEnableNanite,
                                              const TArray<UMaterialInterface*>& Materials)
{
#if WITH_EDITOR
    if (Source.TriangleCount() == 0 || AssetPathAndName.IsEmpty())
    {
        return nullptr;
    }

    FAssetToolsModule& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    FString PackageName;
    FString AssetName;
    AssetTools.Get().CreateUniqueAssetName(AssetPathAndName, FString(), PackageName, AssetName);

    UPackage* Package = CreatePackage(*PackageName);
    if (Package == nullptr)
    {
        return nullptr;
    }

    UStaticMesh* Baked =
        NewObject<UStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);
    if (Baked == nullptr)
    {
        return nullptr;
    }

    FStaticMeshSourceModel& Model = Baked->AddSourceModel();
    Model.BuildSettings.bRecomputeNormals = false;
    Model.BuildSettings.bRecomputeTangents = true;
    Model.BuildSettings.bGenerateLightmapUVs = false;
    // A non-default scale re-keys the cached distance field so a bad one is never reused.
    Model.BuildSettings.DistanceFieldResolutionScale = BakedDistanceFieldResolutionScale;

    UE::Geometry::FDynamicMesh3 Prepared = Source;
    if (!Prepared.HasAttributes())
    {
        Prepared.EnableAttributes();
    }
    // mirrors FloorPlanMeshBuilder.cpp:336 — boxes need hard per-triangle normals.
    UE::Geometry::FMeshNormals::InitializeMeshToPerTriangleNormals(&Prepared);

    FMeshDescription MeshDescription;
    FStaticMeshAttributes(MeshDescription).Register();
    FDynamicMeshToMeshDescription Converter;
    Converter.Convert(&Prepared, MeshDescription);

    Baked->CreateMeshDescription(0, MoveTemp(MeshDescription));
    Baked->CommitMeshDescription(0);

    TArray<FStaticMaterial> Slots;
    for (UMaterialInterface* Material : Materials)
    {
        FStaticMaterial& Slot = Slots.AddDefaulted_GetRef();
        // The distance-field builder drops triangles whose slot material is empty or translucent.
        Slot.MaterialInterface = DistanceFieldSafeMaterial(Material);
    }
    if (Slots.IsEmpty())
    {
        FStaticMaterial& Slot = Slots.AddDefaulted_GetRef();
        Slot.MaterialInterface = DistanceFieldSafeMaterial(nullptr);
    }
    Baked->SetStaticMaterials(Slots);

    Baked->bGenerateMeshDistanceField = true;
    Baked->NaniteSettings.bEnabled = bEnableNanite;
    Baked->CreateBodySetup();
    if (UBodySetup* Body = Baked->GetBodySetup())
    {
        Body->CollisionTraceFlag = CTF_UseComplexAsSimple;
    }

    TArray<FText> BuildErrors;
    // A non-null error array makes the build run synchronously.
    Baked->Build(true, &BuildErrors);
    for (const FText& Error : BuildErrors)
    {
        UE_LOG(LogTemp, Warning, TEXT("FloorPlan bake %s: %s"), *Baked->GetName(),
               *Error.ToString());
    }

    Baked->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Baked);
    return Baked;
#else
    (void)Source;
    (void)AssetPathAndName;
    (void)bEnableNanite;
    (void)Materials;
    return nullptr;
#endif
}
