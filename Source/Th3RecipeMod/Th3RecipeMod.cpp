/* SPDX-License-Identifier: MPL-2.0 */

#include "Th3RecipeMod.h"
#include "Patching/NativeHookManager.h"

DEFINE_LOG_CATEGORY(LogTh3RecipeMod);

/* TODO: Licensing issues */
void FTh3RecipeModModule::EnableGlobalStaticMeshCPUAccess()
{
	/* Set up hook for forcing all static mesh data to be CPU resident */
	void* UObjectCDO = GetMutableDefault<UObject>();
	SUBSCRIBE_METHOD_EXPLICIT_VIRTUAL_AFTER(void(UObject::*)(FArchive&), UObject::Serialize, UObjectCDO, [](UObject* Object, FArchive& Ar) {
		if (Object->IsA<UStaticMesh>()) {
			CastChecked<UStaticMesh>(Object)->bAllowCPUAccess = true;
		}
	});

	/* Reload all existing static mesh packages to apply the patch */
	TArray<UPackage*> PackagesToReload;

	for (TObjectIterator<UStaticMesh> It; It; ++It) {
		UStaticMesh* StaticMesh = *It;
		if (not StaticMesh->bAllowCPUAccess) {
			UPackage* OwnerPackage = StaticMesh->GetOutermost();
			/* Only interested in non-transient FactoryGame packages */
			//if (OwnerPackage->GetName().StartsWith(TEXT("/Game/FactoryGame/"))) {
				if (!OwnerPackage->HasAnyFlags(RF_Transient)) {
					UE_LOG(LogTh3RecipeMod, Log, TEXT("StaticMesh Package %s has been loaded before CPU access fixup application, attempting to reload"), *OwnerPackage->GetName());
					PackagesToReload.Add(OwnerPackage);
				}
			//}
		}
	}

	if (PackagesToReload.Num()) {
		UE_LOG(LogTh3RecipeMod, Log, TEXT("Reloading %d StaticMesh packages for CPU access fixup..."), PackagesToReload.Num());
		for (UPackage* Package : PackagesToReload) {
			ReloadPackage(Package, LOAD_None);
		}
	}
}

/*
 * This code will execute after your module is loaded into memory
 * The exact timing is specified in the .uplugin file per-module
 */
void FTh3RecipeModModule::StartupModule()
{
	UE_LOG(LogTh3RecipeMod, Log, TEXT("Hello World"));
	if (not GIsEditor) {
		UE_LOG(LogTh3RecipeMod, Log, TEXT("Enabling CPU access for static meshes..."));
		EnableGlobalStaticMeshCPUAccess();
		UE_LOG(LogTh3RecipeMod, Log, TEXT("Enabled CPU access for static meshes"));
	}
}

/* 
 * This function may be called during shutdown to clean up your module.
 * For modules that support dynamic reloading, we call this function before unloading the module.
 */
void FTh3RecipeModModule::ShutdownModule()
{
	UE_LOG(LogTh3RecipeMod, Log, TEXT("Goodbye Cruel World"));
}

//IMPLEMENT_MODULE(FTh3RecipeModModule, Th3RecipeMod);
IMPLEMENT_GAME_MODULE(FTh3RecipeModModule, Th3RecipeMod);