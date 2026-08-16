#include "Engine/World.h"
#include "FloorPlanImporter.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, FloorPlanUnreal)

namespace
{
    const TCHAR* ImportUsage =
        TEXT("Usage: FloorPlan.Import <path.dxf> [WallLayer|*] [MmPerUnit] [single|double], "
             "plus bake, roof, wallmat=<asset>, floormat=<asset> and/or json=<path> anywhere");

    const TCHAR* BuildingUsage =
        TEXT("Usage: FloorPlan.ImportBuilding <name>=<path.dxf>@<elevationMm> ... , plus bake, "
             "roof, blockers, wallmat=<asset>, floormat=<asset> and/or json=<path> anywhere");

    UMaterialInterface* LoadMaterialFlag(const FString& Path, FOutputDevice& Output)
    {
        FString ObjectPath = Path;
        if (!ObjectPath.Contains(TEXT(".")))
        {
            ObjectPath += TEXT(".") + FPackageName::GetShortName(ObjectPath);
        }
        UMaterialInterface* Material =
            LoadObject<UMaterialInterface>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn);
        if (Material == nullptr)
        {
            Output.Logf(ELogVerbosity::Warning, TEXT("Material not found: %s"), *Path);
        }
        return Material;
    }

    /// Pulls the flags that may appear in any position, leaving the ordered settings behind.
    TArray<FString> TakeFlags(const TArray<FString>& Args, int32 First,
                              UFloorPlanImportOptions& Options, FOutputDevice& Output)
    {
        TArray<FString> Settings;
        for (int32 Index = First; Index < Args.Num(); ++Index)
        {
            if (Args[Index].Equals(TEXT("bake"), ESearchCase::IgnoreCase))
            {
                Options.bBakeToStaticMesh = true;
                continue;
            }
            if (Args[Index].Equals(TEXT("roof"), ESearchCase::IgnoreCase))
            {
                Options.bGenerateRoof = true;
                continue;
            }
            if (Args[Index].Equals(TEXT("blockers"), ESearchCase::IgnoreCase))
            {
                Options.bGenerateShadowBlockers = true;
                continue;
            }
            if (Args[Index].StartsWith(TEXT("json="), ESearchCase::IgnoreCase))
            {
                Options.DataExportPath = Args[Index].RightChop(5);
                continue;
            }
            if (Args[Index].StartsWith(TEXT("wallmat="), ESearchCase::IgnoreCase))
            {
                Options.WallMaterial = LoadMaterialFlag(Args[Index].RightChop(8), Output);
                continue;
            }
            if (Args[Index].StartsWith(TEXT("floormat="), ESearchCase::IgnoreCase))
            {
                Options.FloorMaterial = LoadMaterialFlag(Args[Index].RightChop(9), Output);
                continue;
            }
            Settings.Add(Args[Index]);
        }
        return Settings;
    }

    void ReportOutcome(const FFloorPlanImportResult& Result,
                       const UFloorPlanImportOptions& Options, FOutputDevice& Output)
    {
        if (!Result.bSucceeded)
        {
            Output.Logf(ELogVerbosity::Error, TEXT("FloorPlan import failed: %s"),
                        *Result.Diagnostic);
            return;
        }
        Output.Logf(ELogVerbosity::Display,
                    TEXT("Imported %d storey(s), %d room(s), %d wall(s), %d stair(s), "
                         "%d opening(s), %.2f m2 total. Units %s (%.4f mm/unit)."),
                    Result.StoreyCount, Result.RoomCount, Result.WallCount, Result.StairCount,
                    Result.OpeningCount, Result.TotalFloorAreaSquareMetres,
                    Result.bUnitsWereDeclared ? TEXT("declared") : TEXT("supplied"),
                    Result.MillimetresPerDrawingUnit);
        Output.Logf(ELogVerbosity::Display, TEXT("Sealed solids: %s"),
                    Result.UnsealedMeshes == 0
                        ? TEXT("all watertight")
                        : *FString::Printf(TEXT("%d WITH OPEN BOUNDARIES"),
                                           Result.UnsealedMeshes));
        Output.Logf(ELogVerbosity::Display,
                    TEXT("Adjacency: %d link(s), %d traversable, %d vertical."),
                    Result.AdjacencyLinks, Result.TraversableLinks, Result.VerticalLinks);
        Output.Logf(ELogVerbosity::Display, TEXT("Also placed %d column(s) and %d fixture(s)."),
                    Result.ColumnCount, Result.FixtureCount);
        if (Result.ContactShadowLightsChanged > 0)
        {
            Output.Logf(ELogVerbosity::Display,
                        TEXT("Enabled contact shadows on %d directional light(s) so sealed "
                             "interiors shade cleanly (bEnsureSunContactShadows opts out)."),
                        Result.ContactShadowLightsChanged);
        }
        if (Result.ContradictedDimensions > 0)
        {
            Output.Logf(ELogVerbosity::Warning,
                        TEXT("%d dimension(s) state a measurement the geometry contradicts. "
                             "The drawing may have been stretched without them being updated."),
                        Result.ContradictedDimensions);
        }
        if (Options.bBakeToStaticMesh)
        {
            Output.Logf(Result.BakedMeshes > 0 ? ELogVerbosity::Display : ELogVerbosity::Warning,
                        TEXT("Baked %d static mesh asset(s) into %s — save them to keep them."),
                        Result.BakedMeshes, *Options.StaticMeshFolder);
        }
        if (!Options.DataExportPath.IsEmpty())
        {
            Output.Logf(Result.DataExportPath.IsEmpty() ? ELogVerbosity::Warning
                                                        : ELogVerbosity::Display,
                        TEXT("Data export: %s"),
                        Result.DataExportPath.IsEmpty() ? TEXT("FAILED TO WRITE")
                                                        : *Result.DataExportPath);
        }
        for (const FString& Label : Result.UnassignedLabels)
        {
            Output.Logf(ELogVerbosity::Warning, TEXT("Unassigned label: %s"), *Label);
        }
    }

    void ImportCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
    {
        if (World == nullptr || Args.Num() < 1)
        {
            Output.Log(ELogVerbosity::Error, ImportUsage);
            return;
        }

        UFloorPlanImportOptions* Options = NewObject<UFloorPlanImportOptions>();
        const TArray<FString> Settings = TakeFlags(Args, 1, *Options, Output);

        if (Settings.Num() >= 1 && Settings[0] != TEXT("*"))
        {
            Options->WallLayers.Add(Settings[0]);
        }
        if (Settings.Num() >= 2)
        {
            // Zero is the documented way to say "keep what the file declares", not a scale.
            const double Millimetres = FCString::Atod(*Settings[1]);
            if (Millimetres > 0.0)
            {
                Options->MillimetresPerDrawingUnit = Millimetres;
                Options->bOverrideDeclaredUnits = true;
            }
        }
        if (Settings.Num() >= 3 && Settings[2].StartsWith(TEXT("single")))
        {
            Options->Convention = EFloorPlanWallConvention::SingleLine;
        }

        ReportOutcome(UFloorPlanImporter::ImportDxf(World, Args[0], Options), *Options, Output);
    }

    bool ParseStorey(const FString& Argument, FFloorPlanStorey& Storey)
    {
        FString Name;
        FString Remainder;
        if (!Argument.Split(TEXT("="), &Name, &Remainder))
        {
            return false;
        }
        FString Path;
        FString Elevation;
        if (Remainder.Split(TEXT("@"), &Path, &Elevation))
        {
            Storey.ElevationMm = FCString::Atod(*Elevation);
        }
        else
        {
            Path = Remainder;
        }
        Storey.Name = Name;
        Storey.FilePath = Path;
        return !Storey.Name.IsEmpty() && !Storey.FilePath.IsEmpty();
    }

    void ImportBuildingCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
    {
        if (World == nullptr || Args.Num() < 1)
        {
            Output.Log(ELogVerbosity::Error, BuildingUsage);
            return;
        }

        UFloorPlanImportOptions* Options = NewObject<UFloorPlanImportOptions>();
        const TArray<FString> Settings = TakeFlags(Args, 0, *Options, Output);

        TArray<FFloorPlanStorey> Storeys;
        for (const FString& Argument : Settings)
        {
            FFloorPlanStorey Storey;
            if (!ParseStorey(Argument, Storey))
            {
                Output.Logf(ELogVerbosity::Error, TEXT("Could not read storey '%s'. %s"),
                            *Argument, BuildingUsage);
                return;
            }
            Storeys.Add(Storey);
        }

        ReportOutcome(UFloorPlanImporter::ImportBuilding(World, Storeys, Options), *Options,
                      Output);
    }
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GFloorPlanImportCommand(
    TEXT("FloorPlan.Import"), TEXT("Import one DXF floor plan as a single storey."),
    FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ImportCommand));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GFloorPlanImportBuildingCommand(
    TEXT("FloorPlan.ImportBuilding"),
    TEXT("Import several DXF plans as stacked storeys of one building."),
    FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ImportBuildingCommand));
