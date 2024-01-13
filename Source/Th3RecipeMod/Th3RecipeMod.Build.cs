/* SPDX-License-Identifier: MPL-2.0 */

using UnrealBuildTool;

public class Th3RecipeMod : ModuleRules
{
    public Th3RecipeMod(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = false;

        PublicIncludePaths.AddRange(new string[] {
			// ... add public include paths required here ...
		});


        PrivateIncludePaths.AddRange(new string[] {
			// ... add other private include paths required here ...
		});

        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "PhysicsCore",
            "InputCore",
            "GeometryCollectionEngine",
            "ChaosVehiclesCore", "ChaosSolverEngine",
            "AnimGraphRuntime",
            "AssetRegistry",
            "NavigationSystem",
            "AIModule",
            "GameplayTasks",
            "SlateCore", "Slate", "UMG",
            "RenderCore",
            "CinematicCamera",
            "Foliage",
            "EnhancedInput",
            "NetCore",
            "GameplayTags",
            "RHI",
			"SML"
        });

        // Header stubs
        PublicDependencyModuleNames.AddRange(new[] {
            "DummyHeaders",
        });

        PrivateDependencyModuleNames.AddRange(new[] {
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
			"SML"
		});

        DynamicallyLoadedModuleNames.AddRange(new string[] {
			// ... add any modules that your module loads dynamically here ...
        });

        PublicDependencyModuleNames.AddRange(new string[] { "FactoryGame", "SML" });
    }
}
