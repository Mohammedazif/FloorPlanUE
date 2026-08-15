#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "FloorPlanImportOptions.generated.h"

class UMaterialInterface;

UENUM(BlueprintType)
enum class EFloorPlanWallConvention : uint8
{
    DoubleLine UMETA(DisplayName = "Double line (walls drawn as paired faces)"),
    SingleLine UMETA(DisplayName = "Single line (walls drawn as centrelines)")
};

/// Import settings for a DXF floor plan. None of these can be inferred reliably from the file.
UCLASS(BlueprintType)
class FLOORPLANUNREAL_API UFloorPlanImportOptions : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    EFloorPlanWallConvention Convention = EFloorPlanWallConvention::DoubleLine;

    /// Layers that may contribute wall geometry. Leave empty to accept every layer.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    TArray<FString> WallLayers;

    /// Applied when the file declares no units, and when OverrideDeclaredUnits is set.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units")
    double MillimetresPerDrawingUnit = 1.0;

    /// Real files do declare the wrong unit; wipeout_door.dxf claims inches for millimetres.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units")
    bool bOverrideDeclaredUnits = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry",
              meta = (ClampMin = "1.0"))
    double WallHeightMm = 2700.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    bool bGenerateFloors = true;

    /// Caps the topmost storey with a slab; an open-top building has the sun inside it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    bool bGenerateRoof = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    bool bCutOpenings = true;

    /// UVs are one unit per metre, so a tiling material keeps its scale on every element.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
    TObjectPtr<UMaterialInterface> WallMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
    TObjectPtr<UMaterialInterface> FloorMaterial;

    /// Name for the storey this plan represents; also what separates its ids from other floors.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storey")
    FString StoreyName = TEXT("Ground");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storey")
    double StoreyElevationMm = 0.0;

    /// Absolute path for a JSON dump of rooms, walls, openings and the adjacency graph.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    FString DataExportPath;

    /// Editor only. Dynamic meshes support neither Nanite nor Lumen; static mesh assets do.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Mesh")
    bool bBakeToStaticMesh = false;

    /// Worth it only for the triangle counts a detailed plan reaches, not for plain boxes.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Mesh")
    bool bEnableNanite = false;

    /// Baked assets land in a subfolder of this named after the DXF.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Mesh")
    FString StaticMeshFolder = TEXT("/Game/FloorPlan");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocks")
    TArray<FString> DoorBlockPrefixes{TEXT("DOOR"), TEXT("DR_"), TEXT("D_")};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blocks")
    TArray<FString> WindowBlockPrefixes{TEXT("WIN"), TEXT("WINDOW"), TEXT("W_")};
};
