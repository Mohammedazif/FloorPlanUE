using System.IO;
using UnrealBuildTool;

public class FloorPlanUnreal : ModuleRules
{
    public FloorPlanUnreal(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "FloorPlanCore"));
        PrivateDefinitions.Add("_CRT_SECURE_NO_WARNINGS=1");

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GeometryCore",
            "GeometryFramework",
            "GeometryScriptingCore"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "AssetTools",
                "GeometryScriptingEditor",
                "UnrealEd"
            });
        }
    }
}
