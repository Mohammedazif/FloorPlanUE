#include "FloorPlanStaticMeshBaker.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMeshToMeshDescription.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSourceData.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
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
#if WITH_EDITOR
    if (Source.TriangleCount() == 0 || AssetPathAndName.IsEmpty())
    {
        return nullptr;
    }

    const FString AssetName = FPackageName::GetShortName(AssetPathAndName);
    UPackage* Package = CreatePackage(*AssetPathAndName);
    if (Package == nullptr)
    {
        return nullptr;
    }

    // Re-importing rewrites the asset in place, so actors already placed from it follow along
    // instead of being orphaned beside a numbered copy.
    UStaticMesh* Baked =
        LoadObject<UStaticMesh>(Package, *AssetName, nullptr, LOAD_NoWarn | LOAD_Quiet);
    const bool bRewritten = Baked != nullptr;
    if (bRewritten)
    {
        Baked->Modify();
        Baked->PreEditChange(nullptr);
        Baked->SetNumSourceModels(0);
    }
    else
    {
        Baked = NewObject<UStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);
    }
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
    Slots.AddDefaulted();
    // The distance-field builder drops every triangle whose slot material is empty or translucent.
    Slots[0].MaterialInterface = DistanceFieldSafeMaterial(Material);
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

    if (bRewritten)
    {
        Baked->PostEditChange();
    }
    else
    {
        FAssetRegistryModule::AssetCreated(Baked);
    }
    Baked->MarkPackageDirty();
    return Baked;
#else
    (void)Source;
    (void)AssetPathAndName;
    (void)bEnableNanite;
    (void)Material;
    return nullptr;
#endif
}
