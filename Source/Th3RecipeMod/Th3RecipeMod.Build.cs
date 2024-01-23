/* SPDX-License-Identifier: MPL-2.0 */

using UnrealBuildTool;

public class Th3RecipeMod : ModuleRules
{
    public Th3RecipeMod(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new[] {
            "Core",
            "CoreUObject",
            "Engine",
            "AssetRegistry",
			"SML"
        });

        PrivateDependencyModuleNames.AddRange(new[] {
            "CoreUObject",
            "Engine",
            "DummyHeaders",
            "SML"
		});

        PublicDependencyModuleNames.AddRange(new string[] { "FactoryGame", "SML" });
    }
}
