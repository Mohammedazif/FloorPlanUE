#include "FloorPlanStaticMeshBaker.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSourceData.h"
#include "FloorPlanMeshBuilder.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "IAssetTools.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Modules/ModuleManager.h"
#include "UDynamicMesh.h"
#endif

#if WITH_EDITOR
namespace
{
    constexpr float BakedDistanceFieldResolutionScale = 2.0f;

    void FillEmptyMaterialSlots(UStaticMesh& Baked, UMaterialInterface* Material)
    {
        UMaterialInterface* Opaque =
            Material != nullptr ? Material : UMaterial::GetDefaultMaterial(MD_Surface);
        TArray<FStaticMaterial> Slots = Baked.GetStaticMaterials();
        if (Slots.IsEmpty())
        {
            Slots.AddDefaulted();
        }
        bool bChanged = Slots.Num() != Baked.GetStaticMaterials().Num();
        for (FStaticMaterial& Slot : Slots)
        {
            if (Slot.MaterialInterface == nullptr)
            {
                Slot.MaterialInterface = Opaque;
                bChanged = true;
            }
        }
        if (bChanged)
        {
            Baked.SetStaticMaterials(Slots);
        }
    }

    void RebuildWithDistanceField(UStaticMesh& Baked, UMaterialInterface* Material)
    {
        // The distance-field builder drops every triangle whose material slot is empty.
        FillEmptyMaterialSlots(Baked, Material);
        Baked.bGenerateMeshDistanceField = true;
        if (Baked.GetNumSourceModels() > 0)
        {
            // A non-default scale re-keys the cached distance field so a bad one is never reused.
            Baked.GetSourceModel(0).BuildSettings.DistanceFieldResolutionScale =
                BakedDistanceFieldResolutionScale;
        }
        TArray<FText> BuildErrors;
        // A non-null error array makes the rebuild run synchronously.
        Baked.Build(true, &BuildErrors);
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

    UDynamicMesh* Carrier = NewObject<UDynamicMesh>();
    FFloorPlanMeshBuilder::CopyToDynamicMesh(Source, Carrier);

    FAssetToolsModule& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    FString PackageName;
    FString AssetName;
    AssetTools.Get().CreateUniqueAssetName(AssetPathAndName, FString(), PackageName, AssetName);

    FGeometryScriptCreateNewStaticMeshAssetOptions Options;
    Options.bEnableNanite = bEnableNanite;

    EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
    UStaticMesh* Baked =
        UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
            Carrier, PackageName, Options, Outcome);
    if (Outcome != EGeometryScriptOutcomePins::Success || Baked == nullptr)
    {
        return nullptr;
    }
    RebuildWithDistanceField(*Baked, Material);
    return Baked;
#else
    (void)Source;
    (void)AssetPathAndName;
    (void)bEnableNanite;
    (void)Material;
    return nullptr;
#endif
}
