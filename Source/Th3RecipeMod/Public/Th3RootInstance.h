/* SPDX-License-Identifier: MPL-2.0 */

#pragma once

#include <CoreMinimal.h>
#include <Th3Utilities.h>
#include <Th3Tex2DUtils.h>
#include <Module/GameInstanceModule.h>
#include <Resources/FGItemDescriptor.h>
#include <FGResourceSinkSettings.h>
#include <FGItemCategory.h>
#include <FGRecipe.h>
#include <FGSchematic.h>
#include <Engine/DataTable.h>
#include <Engine/Texture2D.h>
#include <Unlocks/FGUnlock.h>
#include <Unlocks/FGUnlockInfoOnly.h>
#include <Unlocks/FGUnlockRecipe.h>
#include <Unlocks/FGUnlockSchematic.h>

#include "Th3RootInstance.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTh3RootInstance, Log, All);

UCLASS(Abstract)
class TH3RECIPEMOD_API UTh3RootInstance : public UGameInstanceModule
{
	GENERATED_BODY()
	friend class UTh3RootGame;
private:
	/* All generated classes are somewhere in here */
	const FString MOD_TRANSIENT_ROOT = TEXT("/Th3RecipeMod");

	const int32 CAT_PRIORITY_DELTA = 100;

	int32 CompressionMenuPriority = 1;

	/* Marked as UPROPERTY because it holds CDO edits */
	UPROPERTY();
	TArray<UFGUnlockRecipe*> ModifiedUnlockRecipes;

	TArray<UFGSchematic*> VisitedSchematics;
	TArray<TSubclassOf<UFGRecipe>> RecipesToRegister;

	TMap<TSubclassOf<UFGRecipe>, TSubclassOf<UFGRecipe>> RecipeToCompressedMap;
	TMap<TSubclassOf<UFGItemDescriptor>, TSubclassOf<UFGItemDescriptor>> ItemToCompressedMap;
	TMap<TSubclassOf<UFGCategory>, TSubclassOf<UFGCategory>> CategoryToCompressedMap;

	const FTextFormat CompressedDisplayNameFmt = NSLOCTEXT("FTh3RecipeMod", "CompressedItemFmt", "{CompressedPrefix} {DisplayName}");
	FORCEINLINE FText CompressDisplayName(const FText& DisplayName)
	{
		return FText::Format(CompressedDisplayNameFmt, { { TEXT("CompressedPrefix"), CompressedPrefixText }, { TEXT("DisplayName"), DisplayName } });
	}
public:
	UTh3RootInstance();
	~UTh3RootInstance();
	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
protected:
	void MakeConversionRecipe(const FItemAmount& Ingredients, const FItemAmount& Products);
	void MakeCompressionRecipes(const TSubclassOf<UFGItemDescriptor>& OrigItem, const TSubclassOf<UFGItemDescriptor>& NewItem);
	UTexture2D* GetItemIcon(UFGItemDescriptor* OrigCDO);
	TSubclassOf<UFGItemDescriptor> CompressedFormOf(const TSubclassOf<UFGItemDescriptor>& OrigItem);
	bool CanRecipeBeCompressed(const TSubclassOf<UFGRecipe>& Recipe);
	TSubclassOf<UFGCategory> CompressCategory(const TSubclassOf<UFGCategory>& OrigCat);
	TSubclassOf<UFGRecipe> CompressRecipe(const TSubclassOf<UFGRecipe>& OrigRecipe);
	void CompressAllRecipes();
	void ProcUnlockRecipe(UFGUnlock* InUnlock);
	void ProcUnlockSchematic(UFGUnlock* InUnlock);
	void CompressSchematicUnlocks(const TSubclassOf<UFGSchematic>& Schematic);
	void CompressAllSchematics();
public:
	UPROPERTY(EditDefaultsOnly, Category = "Mod Configuration")
	const TSubclassOf<UFGItemCategory> CompressionCategory;

	UPROPERTY(EditDefaultsOnly, Category = "Mod Configuration")
	const TSubclassOf<UFGItemCategory> DecompressionCategory;

	UPROPERTY(EditDefaultsOnly, Category = "Mod Configuration")
	UTexture2D* CompressedIconOverlay;

	UPROPERTY(EditDefaultsOnly, Category = "Mod Configuration")
	FText CompressedPrefixText;

	UPROPERTY(EditDefaultsOnly, Category = "Mod Configuration", Meta = (ClampMin = 1))
	int32 CompressionRatio;

	UPROPERTY(EditDefaultsOnly, Category = "Mod Configuration", Meta = (MustImplement = "/Script/FactoryGame.FGRecipeProducerInterface"))
	const TSoftClassPtr<UObject> CompressingMachine;
};
