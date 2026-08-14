#include "FloorPlanStaticMeshBaker.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "FloorPlanMeshBuilder.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "UDynamicMesh.h"
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
                                              const FString& AssetPathAndName, bool bEnableNanite)
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
    return Outcome == EGeometryScriptOutcomePins::Success ? Baked : nullptr;
#else
    (void)Source;
    (void)AssetPathAndName;
    (void)bEnableNanite;
    return nullptr;
#endif
}
