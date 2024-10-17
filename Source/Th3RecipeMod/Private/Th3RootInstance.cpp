/* SPDX-License-Identifier: MPL-2.0 */

#include "Th3RootInstance.h"
#include "Th3Utilities.h"

#include <Containers/EnumAsByte.h>
#include <Reflection/ClassGenerator.h>
#include <Registry/ModContentRegistry.h>
#include <Resources/FGBuildingDescriptor.h>
#include <Buildables/FGBuildableGeneratorFuel.h>
#include <FGCustomizationRecipe.h>
#include <FGRecipe.h>
#include <FGRecipeManager.h>
#include <FGSchematic.h>
#include <Logging/LogMacros.h>
#include <Logging/StructuredLog.h>
#include <UObject/UObjectGlobals.h>
#include <Algo/Accumulate.h>
#include <Algo/AllOf.h>
#include <Algo/AnyOf.h>
#include <Algo/ForEach.h>
#include <Algo/Reverse.h>
#include <Algo/Transform.h>
#include <AssetRegistry/AssetRegistryModule.h>
#include <Engine/AssetManager.h>

DEFINE_LOG_CATEGORY(LogTh3RootInstance);

UTh3RootInstance::UTh3RootInstance()
{
	UE_LOG(LogTh3RootInstance, Display, TEXT("Hello Game Instance %s"), *this->GetPathName());
}

UTh3RootInstance::~UTh3RootInstance()
{
	UE_LOG(LogTh3RootInstance, Display, TEXT("Goodbye Cruel Game Instance"));
}

TSubclassOf<UFGCategory> UTh3RootInstance::CompressCategory(const TSubclassOf<UFGCategory>& OrigCat)
{
	/* Garbage in, garbage out */
	if (not OrigCat) {
		return OrigCat;
	}
	TSubclassOf<UFGCategory>* NewCatPtr = CategoryToCompressedMap.Find(OrigCat);
	if (NewCatPtr) {
		return *NewCatPtr;
	}
	UE_LOG(LogTh3RootInstance, Verbose, TEXT("Compressing Category %s"), *OrigCat->GetPathName());
	TSubclassOf<UFGCategory> NewCat = Th3Utilities::CopyClassWithPrefix(OrigCat, MOD_TRANSIENT_ROOT / TEXT("Categories"), TEXT("Compressed"));

	UFGCategory* OrigCDO = OrigCat.GetDefaultObject();
	UFGCategory* NewCDO = NewCat.GetDefaultObject();

	NewCDO->mDisplayName = CompressDisplayName(OrigCDO->mDisplayName);
	NewCDO->mMenuPriority += CAT_PRIORITY_DELTA;

	CategoryToCompressedMap.Add(OrigCat, NewCat);
	return NewCat;
}

void UTh3RootInstance::MakeConversionRecipe(const FItemAmount& Ingredients, const FItemAmount& Products)
{
	const bool IsCompressionRecipe = Ingredients.Amount > Products.Amount;
	const UFGItemDescriptor* BaseItem = (IsCompressionRecipe ? Products : Ingredients).ItemClass.GetDefaultObject();
	const TSubclassOf<UFGItemCategory> Category = IsCompressionRecipe ? CompressionCategory : DecompressionCategory;

	const FString RecipeType = FString(IsCompressionRecipe ? TEXT("Compression") : TEXT("Decompression"));
	const FString PackagePath = MOD_TRANSIENT_ROOT / TEXT("Recipes") / RecipeType / BaseItem->GetPackage()->GetName();
	const FString ClassName = FString::Printf(TEXT("Recipe_%s_%s"), *RecipeType, *BaseItem->GetName());
	TSubclassOf<UFGRecipe> Recipe = Th3Utilities::GenerateNewClass(PackagePath, ClassName, UFGRecipe::StaticClass());
	if (not Recipe) {
		UE_LOG(LogTh3RootInstance, Fatal, TEXT("Failed to generate %s recipe for %s %s"), *RecipeType, *PackagePath, *ClassName);
		return;
	}
	UFGRecipe* CDO = Recipe.GetDefaultObject();
	CDO->mManufactoringDuration = CompressionRatio / 10.0;
	CDO->mManufacturingMenuPriority = CompressionMenuPriority++;
	CDO->mIngredients.Add(Ingredients);
	CDO->mProduct.Add(Products);
	CDO->mProducedIn.Add(CompressingMachine);
	CDO->mOverriddenCategory = Category;
	RecipesToRegister.Add(Recipe);
}

void UTh3RootInstance::MakeCompressionRecipes(const TSubclassOf<UFGItemDescriptor>& OrigItem, const TSubclassOf<UFGItemDescriptor>& NewItem)
{
	const int32 BaseAmount = UFGItemDescriptor::GetStackSize(OrigItem) / UFGItemDescriptor::GetStackSizeConverted(OrigItem);
	const FItemAmount OrigAmount = FItemAmount(OrigItem, BaseAmount * CompressionRatio);
	const FItemAmount NewAmount = FItemAmount(NewItem, BaseAmount * 1);
	MakeConversionRecipe(OrigAmount, NewAmount);
	MakeConversionRecipe(NewAmount, OrigAmount);
}

UTexture2D* UTh3RootInstance::GetItemIcon(UFGItemDescriptor* OrigCDO)
{
	if (UTexture2D* BigM = OrigCDO->mPersistentBigIcon) {
		return BigM;
	} else if (UTexture2D* BigF = OrigCDO->GetBigIconFromInstance()) {
		return BigF;
	} else if (UTexture2D* SmallM = OrigCDO->mSmallIcon) {
		return SmallM;
	} else if (UTexture2D* SmallF = OrigCDO->GetSmallIconFromInstance()) {
		return SmallF;
	} else {
		UE_LOG(LogTh3RootInstance, Warning, TEXT("Could not find a valid Icon for Item %s"), *OrigCDO->GetClass()->GetPathName());
		return nullptr;
	}
}

TSubclassOf<UFGItemDescriptor> UTh3RootInstance::CompressedFormOf(const TSubclassOf<UFGItemDescriptor>& OrigItem)
{
	TSubclassOf<UFGItemDescriptor>* NewItemPtr = ItemToCompressedMap.Find(OrigItem);
	if (NewItemPtr) {
		return *NewItemPtr;
	}
	UE_LOG(LogTh3RootInstance, Verbose, TEXT("Compressing Item %s"), *OrigItem->GetPathName());
	TSubclassOf<UFGItemDescriptor> NewItem = Th3Utilities::CopyClassWithPrefix(OrigItem, MOD_TRANSIENT_ROOT / TEXT("Items"), TEXT("Compressed"));

	UFGItemDescriptor* OrigCDO = OrigItem.GetDefaultObject();
	UFGItemDescriptor* NewCDO = NewItem.GetDefaultObject();

	NewCDO->mDisplayName = CompressDisplayName(OrigCDO->mDisplayName);
	NewCDO->mEnergyValue *= CompressionRatio;
	NewCDO->mRadioactiveDecay *= CompressionRatio;
	NewCDO->mCategory = CompressCategory(OrigCDO->mCategory);

	UE_LOG(LogTh3RootInstance, Error, TEXT("ENERGY VALUE IS %f ---> %f FOR %s"), OrigCDO->mEnergyValue, NewCDO->mEnergyValue, *OrigItem->GetPathName());

	UE_LOG(LogTh3RootInstance, Log, TEXT(" -  Compressing Item Icon for %s"), *OrigItem->GetPathName());

	UTexture2D* OrigIcon = OrigCDO->mPersistentBigIcon ? OrigCDO->mPersistentBigIcon : OrigCDO->mSmallIcon;

	NewCDO->mPersistentBigIcon = Th3Tex2DUtils::OverlayTextures(GetItemIcon(OrigCDO), CompressedIconOverlay);
	NewCDO->mSmallIcon = NewCDO->mPersistentBigIcon;

	UE_LOG(LogTh3RootInstance, Verbose, TEXT(" -  Successfully compressed Item Icon for %s"), *OrigItem->GetPathName());

	MakeCompressionRecipes(OrigItem, NewItem);

	ItemToCompressedMap.Add(OrigItem, NewItem);
	return NewItem;
}

static TFunction<bool(const TSoftClassPtr<UObject>)> SoftPtrAssetNameContains(const TCHAR* ClassName)
{
	return [ClassName](const TSoftClassPtr<UObject>& Obj) {
		return Obj.GetAssetName().Contains(ClassName);
	};
}

static int32 GetStackSize(const FItemAmount& Amount)
{
	return Amount.ItemClass.GetDefaultObject()->GetStackSize(Amount.ItemClass);
}

bool UTh3RootInstance::InvokeRecipePredicate(const TSubclassOf<UFGRecipe>& Recipe, const TFunction<bool(const UFGRecipe*)> InPredicate)
{
	/* Do not compress invalid recipe classes */
	if (not Recipe) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("Someone registered a nullptr recipe"));
		return false;
	}
	UE_LOG(LogTh3RootInstance, Warning, TEXT("Considering Recipe %s"), *Recipe->GetPathName());
	const UFGRecipe* RecipeCDO = Recipe.GetDefaultObject();
	/* Do not compress invalid recipes */
	if (not RecipeCDO) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("%s has a nullptr CDO"), *Recipe->GetPathName());
		return false;
	}
	/* Do not compress recipes that cannot be produced anywhere */
	if (RecipeCDO->mProducedIn.IsEmpty()) {
		return false;
	}
	/* Do not compress our own (de)compression recipes */
	if (RecipesToRegister.Contains(Recipe)) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("[MOD BUG] Attempted to re-compress Recipe %s"), *Recipe->GetPathName());
		return false;
	}
	return Invoke(InPredicate, RecipeCDO);
}

bool UTh3RootInstance::IsCraftingRecipeCompressible(const TSubclassOf<UFGRecipe>& Recipe)
{
	return InvokeRecipePredicate(Recipe, [this](const UFGRecipe* RecipeCDO) {
		/* Do not compress Upgradeable Machines' upgrade packs */
		if (RecipeCDO->GetClass()->GetPackage()->GetName().StartsWith(TEXT("/UpgradeableMachines/"))) {
			return false;
		}
		/* Do not compress Build Gun recipes */
		const auto IsBuildGunRecipe = SoftPtrAssetNameContains(TEXT("BuildGun"));
		if (Algo::AnyOf(RecipeCDO->mProducedIn, IsBuildGunRecipe)) {
			return false;
		}
		/* Do not compress Customizer recipes */
		if (RecipeCDO->mMaterialCustomizationRecipe.Get()) {
			return false;
		}
		/*
		 * Do not compress recipes involving items whose stack size is
		 * too small to be compressed continuously (2x in the check).
		 */
		const auto IsStackSizeEnough = [this](const FItemAmount& Amount) {
			return ItemToCompressedMap.Contains(Amount.ItemClass) or GetStackSize(Amount) >= 2 * CompressionRatio;
		};
		if (not Algo::AllOf(RecipeCDO->mIngredients, IsStackSizeEnough)) {
			return false;
		}
		if (not Algo::AllOf(RecipeCDO->mProduct, IsStackSizeEnough)) {
			return false;
		}
		return true;
	});
}

TSubclassOf<UFGRecipe> UTh3RootInstance::CompressCraftingRecipe(const TSubclassOf<UFGRecipe>& OrigRecipe)
{
	TSubclassOf<UFGRecipe>* NewRecipePtr = RecipeToCompressedMap.Find(OrigRecipe);
	if (NewRecipePtr) {
		return *NewRecipePtr;
	}
	UE_LOG(LogTh3RootInstance, Verbose, TEXT("Compressing Recipe %s"), *OrigRecipe->GetPathName());

	UFGRecipe* OrigCDO = OrigRecipe.GetDefaultObject();
	const TSubclassOf<UFGRecipe> NewRecipe = Th3Utilities::CopyClassWithPrefix(OrigRecipe, MOD_TRANSIENT_ROOT / TEXT("Recipes"), TEXT("Compressed"));
	UFGRecipe* NewCDO = NewRecipe.GetDefaultObject();
	if (NewCDO->mDisplayNameOverride) {
		NewCDO->mDisplayName = CompressDisplayName(OrigCDO->mDisplayName);
	}
	const auto CompressItemAmounts = [this](const FItemAmount& Amount) {
		return FItemAmount(CompressedFormOf(Amount.ItemClass), Amount.Amount);
	};
	NewCDO->mIngredients.Empty();
	NewCDO->mProduct.Empty();
	NewCDO->mManufactoringDuration *= CompressionRatio;
	NewCDO->mOverriddenCategory = CompressCategory(OrigCDO->mOverriddenCategory);
	Algo::Transform(OrigCDO->mIngredients, NewCDO->mIngredients, CompressItemAmounts);
	Algo::Transform(OrigCDO->mProduct, NewCDO->mProduct, CompressItemAmounts);

	//Th3Utilities::SaveObjectProperties(OrigCDO, TEXT("OrigRecipes"));

	RecipeToCompressedMap.Add(OrigRecipe, NewRecipe);
	return NewRecipe;
}

bool UTh3RootInstance::IsBuildingRecipeCompressible(const TSubclassOf<UFGRecipe>& Recipe)
{
	/* Do not compress invalid recipe classes */
	if (not Recipe) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("Someone registered a nullptr recipe"));
		return false;
	}
	UE_LOG(LogTh3RootInstance, Warning, TEXT("Considering Recipe %s"), *Recipe->GetPathName());
	const UFGRecipe* RecipeCDO = Recipe.GetDefaultObject();
	/* Do not compress invalid recipes */
	if (not RecipeCDO) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("%s has a nullptr CDO"), *Recipe->GetPathName());
		return false;
	}
	/* Do not compress recipes that cannot be produced anywhere */
	if (RecipeCDO->mProducedIn.IsEmpty()) {
		return false;
	}
	/* Do not compress our own (de)compression recipes */
	if (RecipesToRegister.Contains(Recipe)) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("[MOD BUG] Attempted to re-compress Recipe %s"), *Recipe->GetPathName());
		return false;
	}
	/* Only compress Build Gun recipes */
	const auto IsBuildGunRecipe = SoftPtrAssetNameContains(TEXT("BuildGun"));
	if (Algo::NoneOf(RecipeCDO->mProducedIn, IsBuildGunRecipe)) {
		return false;
	}
	/* Do not compress Customizer recipes */
	if (RecipeCDO->mMaterialCustomizationRecipe.Get()) {
		return false;
	}
	if (RecipeCDO->mProduct.Num() != 1) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("Build Gun recipe %s has %d products"),
			   *RecipeCDO->GetPathName(), RecipeCDO->mProduct.Num());
		return false;
	}
	const TSubclassOf<UFGItemDescriptor> BuildingItem = RecipeCDO->mProduct[0].ItemClass;
	if (not BuildingItem) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("Build Gun recipe %s produces an invalid item descriptor"),
			   *RecipeCDO->GetPathName());
		return false;
	}
	if (RecipeCDO->mProduct[0].Amount != 1) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("Build Gun recipe %s produces %d of %s"),
			   *RecipeCDO->GetPathName(), RecipeCDO->mProduct[0].Amount, *BuildingItem->GetPathName());
		return false;
	}
	const TSubclassOf<UFGBuildingDescriptor> BuildingDesc = *BuildingItem;
	if (not BuildingDesc) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("Build Gun recipe %s produces non-UFGBuildingDescriptor %s"),
			   *RecipeCDO->GetPathName(), *BuildingItem->GetPathName());
		return false;
	}
	const TSubclassOf<AFGBuildable> BuildableClass = UFGBuildingDescriptor::GetBuildableClass(BuildingDesc);
	if (not BuildableClass) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("Building Descriptor %s has invalid buildable class"),
			   *BuildingItem->GetPathName());
		return false;
	}
	const TSubclassOf<AFGBuildableGeneratorFuel> BuildableGen = *BuildableClass;
	if (not BuildableGen) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("Building Descriptor %s has buildable %s which is not a fuel generator"),
			   *BuildingItem->GetPathName(), *BuildableClass->GetPathName());
		return false;
	}
	UE_LOG(LogTh3RootInstance, Warning, TEXT("Considering Fuel Generator %s"), *BuildableGen->GetPathName());
	UE_LOG(LogTh3RootInstance, Warning, TEXT("Available Fuel Classes:"));
	const TArray<TSubclassOf<UFGItemDescriptor>>& AvailableFuels = BuildableGen.GetDefaultObject()->GetAvailableFuelClasses(nullptr);
	Algo::ForEach(AvailableFuels, [](const TSubclassOf<UFGItemDescriptor> FuelClass) {
		UE_LOG(LogTh3RootInstance, Warning, TEXT("  - %s (Energy = %f)"), *FuelClass->GetPathName(), UFGItemDescriptor::GetEnergyValue(FuelClass));
	});
	const TArray<TSoftClassPtr<UFGItemDescriptor>>& DefaultFuels = BuildableGen.GetDefaultObject()->GetDefaultFuelClasses();
	Algo::ForEach(DefaultFuels, [](const TSoftClassPtr<UFGItemDescriptor> FuelClassPtr) {
		const TSubclassOf<UFGItemDescriptor> FuelClass = FuelClassPtr.LoadSynchronous();
		UE_LOG(LogTh3RootInstance, Warning, TEXT("  - %s (Energy = %f)"), *FuelClass->GetPathName(), UFGItemDescriptor::GetEnergyValue(FuelClass));
	});
	UE_LOG(LogTh3RootInstance, Warning, TEXT("Supplemental Resource Class:"));
	const TSubclassOf<UFGItemDescriptor> SupplementalRes = BuildableGen.GetDefaultObject()->GetSupplementalResourceClass();
	if (SupplementalRes) {
		UE_LOG(LogTh3RootInstance, Warning, TEXT("  - %s (Energy = %f)"), *SupplementalRes->GetPathName(), UFGItemDescriptor::GetEnergyValue(SupplementalRes));
	}
	return true;
}

void UTh3RootInstance::ProcUnlockRecipe(UFGUnlock* InUnlock)
{
	UFGUnlockRecipe* Unlock = CastChecked<UFGUnlockRecipe>(InUnlock);
	if (ModifiedUnlockRecipes.Contains(Unlock)) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("REVISITING UNLOCK RECIPE??? %s"), *Unlock->GetPathName());
		return;
	}
	UE_LOG(LogTh3RootInstance, Verbose, TEXT("Processing Recipe Unlock %s"), *Unlock->GetPathName());
	ModifiedUnlockRecipes.Add(Unlock);
	TArray<TSubclassOf<UFGRecipe>> NewRecipes;
	Algo::TransformIf(Unlock->mRecipes, NewRecipes, TH3_PROJECTION_THIS(IsCraftingRecipeCompressible), TH3_PROJECTION_THIS(CompressCraftingRecipe));
	//Algo::ForEach(Unlock->mRecipes, TH3_PROJECTION_THIS(IsBuildingRecipeCompressible));
	Unlock->mRecipes.Append(NewRecipes);
}

void UTh3RootInstance::ProcUnlockSchematic(UFGUnlock* InUnlock)
{
	UFGUnlockSchematic* Unlock = CastChecked<UFGUnlockSchematic>(InUnlock);
	Algo::ForEach(Unlock->mSchematics, TH3_PROJECTION_THIS(CompressSchematicUnlocks));
}

void UTh3RootInstance::CompressSchematicUnlocks(const TSubclassOf<UFGSchematic>& Schematic)
{
	UFGSchematic* CDO = Schematic.GetDefaultObject();
	if (not CDO or VisitedSchematics.Contains(CDO)) {
		return;
	}
	VisitedSchematics.Add(CDO);

	UE_LOG(LogTh3RootInstance, Verbose, TEXT("Processing Schematic %s"), *CDO->GetPathName());
	const TMap<UClass*, TFunction<void(UFGUnlock*)>> DispatchTable = {
		{ UFGUnlockRecipe::StaticClass(),    TH3_PROJECTION_THIS(ProcUnlockRecipe)    },
		{ UFGUnlockSchematic::StaticClass(), TH3_PROJECTION_THIS(ProcUnlockSchematic) },
	};
	Th3Utilities::ForEachDynDispatch(CDO->mUnlocks, DispatchTable);
}

void UTh3RootInstance::CompressOneSchematic(const TSoftClassPtr<UFGSchematic>& SchematicPtr)
{
	CompressSchematicUnlocks(SchematicPtr.Get());
}

template<typename T>
static TSoftClassPtr<T> ToSoftClassPtr(const FSoftObjectPath& Path)
{
	return TSoftClassPtr<T>(Path);
}

void UTh3RootInstance::CompressAllSchematics()
{
	const auto store_paths = [this](const TArray<FSoftObjectPath>& InPaths) {
		Algo::Transform(InPaths, SchematicPtrs, &ToSoftClassPtr<UFGSchematic>);
	};
	const auto process_paths = [this]() {
		Algo::ForEach(SchematicPtrs, TH3_PROJECTION_THIS(CompressOneSchematic));
	};
	Process(UFGSchematic::StaticClass(), store_paths, process_paths);
}

void UTh3RootInstance::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	Super::DispatchLifecycleEvent(Phase);

	UE_LOG(LogTh3RootInstance, Display, TEXT("Dispatching Phase %s on %s"), *LifecyclePhaseToString(Phase), *this->GetPathName());

	if (Phase == ELifecyclePhase::POST_INITIALIZATION) {
		UModContentRegistry* Registry = UModContentRegistry::Get(GetWorld());
		if (not Registry) {
			UE_LOG(LogTh3RootInstance, Error, TEXT("Could not get Mod Content Registry, bailing out"));
			return;
		}
		CompressAllSchematics();
		UE_LOG(LogTh3RootInstance, Display, TEXT("Got %d recipes, %d (de)compression recipes and %d compressed items"), RecipeToCompressedMap.Num(), RecipesToRegister.Num(), ItemToCompressedMap.Num());
		Algo::ForEach(RecipesToRegister, [&Registry](const auto& Recipe) {
			Registry->RegisterRecipe(TEXT("Th3RecipeMod"), Recipe);
		});
		UE_LOG(LogTh3RootInstance, Display, TEXT("Done registering recipes"));
	}
}