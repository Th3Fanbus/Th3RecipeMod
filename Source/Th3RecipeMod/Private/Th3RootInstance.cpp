/* SPDX-License-Identifier: MPL-2.0 */

#include "Th3RootInstance.h"
#include "Th3Utilities.h"

#include <Containers/EnumAsByte.h>
#include <Reflection/ClassGenerator.h>
#include <Registry/ModContentRegistry.h>
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

void UTh3RootInstance::MakeConversionRecipe(const FItemAmount& Ingredients, const FItemAmount& Products)
{
	const bool IsCompressionRecipe = Ingredients.Amount > Products.Amount;
	const UFGItemDescriptor* BaseItem = (IsCompressionRecipe ? Products : Ingredients).ItemClass.GetDefaultObject();
	const TSubclassOf<UFGItemCategory> Category = IsCompressionRecipe ? CompressionCategory : DecompressionCategory;

	const FString RecipeType = FString(IsCompressionRecipe ? TEXT("Compression") : TEXT("Decompression"));
	const FString PackagePath = MOD_TRANSIENT_ROOT / TEXT("Recipes") / RecipeType / BaseItem->GetPackage()->GetName();
	const FString ClassName = FString::Printf(TEXT("Recipe_%s_%s"), *RecipeType, *BaseItem->GetName());
	TSubclassOf<UFGRecipe> Recipe = Th3Utilities::GenerateNewClass(*PackagePath, ClassName, UFGRecipe::StaticClass());
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

static void ProcessVertexBuffers(FStaticMeshVertexBuffers& Out, const FStaticMeshVertexBuffers& Bot, const FStaticMeshVertexBuffers& Top)
{
#if 0
	//Initialize vertices first
	const uint32 NumVertices = Top.PositionVertexBuffer.GetNumVertices();

	for (uint32 i = 0; i < NumVertices; i++) {
		const FVector3f& SrcPosition = VertexBuffers.PositionVertexBuffer.VertexPosition(i);
		FbxVector4& DestPosition = ControlPoints[i];
		DestPosition = FFbxDataConverter::ConvertToFbxPos(SrcPosition);
	}

	//Initialize vertex colors (if we have any)
	if (VertexBuffers.ColorVertexBuffer.GetNumVertices() > 0) {
		check(VertexBuffers.ColorVertexBuffer.GetNumVertices() == NumVertices);

		FbxGeometryElementVertexColor* VertexColor = FbxMesh->CreateElementVertexColor();
		VertexColor->SetMappingMode(FbxLayerElement::eByControlPoint);
		VertexColor->SetReferenceMode(FbxLayerElement::eDirect);

		FbxLayerElementArrayTemplate<FbxColor>& ColorArray = VertexColor->GetDirectArray();
		ColorArray.AddMultiple(NumVertices);

		for (uint32 i = 0; i < NumVertices; i++) {
			const FColor& SrcColor = VertexBuffers.ColorVertexBuffer.VertexColor(i);
			ColorArray.SetAt(i, FFbxDataConverter::ConvertToFbxColor(SrcColor));
		}
	}

	check(VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == NumVertices);

	//Initialize normals
	FbxGeometryElementNormal* Normal = FbxMesh->CreateElementNormal();
	Normal->SetMappingMode(FbxLayerElement::eByControlPoint);
	Normal->SetReferenceMode(FbxLayerElement::eDirect);

	FbxLayerElementArrayTemplate<FbxVector4>& NormalArray = Normal->GetDirectArray();
	NormalArray.AddMultiple(NumVertices);

	for (uint32 i = 0; i < NumVertices; i++) {
		const FVector3f SrcNormal = VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(i);
		FbxVector4 DestNormal = FFbxDataConverter::ConvertToFbxPos(SrcNormal);
		NormalArray.SetAt(i, DestNormal);
	}

	//Initialize tangents
	FbxGeometryElementTangent* Tangent = FbxMesh->CreateElementTangent();
	Tangent->SetMappingMode(FbxLayerElement::eByControlPoint);
	Tangent->SetReferenceMode(FbxLayerElement::eDirect);

	FbxLayerElementArrayTemplate<FbxVector4>& TangentArray = Tangent->GetDirectArray();
	TangentArray.AddMultiple(NumVertices);

	for (uint32 i = 0; i < NumVertices; i++) {
		const FVector3f SrcTangent = VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(i);
		FbxVector4 DestTangent = FFbxDataConverter::ConvertToFbxPos(SrcTangent);
		TangentArray.SetAt(i, DestTangent);
	}

	//Initialize binormals
	FbxGeometryElementBinormal* Binormal = FbxMesh->CreateElementBinormal();
	Binormal->SetMappingMode(FbxLayerElement::eByControlPoint);
	Binormal->SetReferenceMode(FbxLayerElement::eDirect);

	FbxLayerElementArrayTemplate<FbxVector4>& BinormalArray = Binormal->GetDirectArray();
	BinormalArray.AddMultiple(NumVertices);

	for (uint32 i = 0; i < NumVertices; i++) {
		const FVector3f SrcBinormal = VertexBuffers.StaticMeshVertexBuffer.VertexTangentY(i);
		FbxVector4 DestBinormal = FFbxDataConverter::ConvertToFbxPos(SrcBinormal);
		BinormalArray.SetAt(i, DestBinormal);
	}

	//Initialize UV positions for each channel
	const uint32 NumTexCoords = VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
	TArray<FbxLayerElementArrayTemplate<FbxVector2>*> UVCoordsArray;

	for (uint32 i = 0; i < NumTexCoords; i++) {
		//TODO proper names, can know if texture is lightmap by checking lightmap tex coord index from static mesh
		const FString UVChannelName = GetNameForUVChannel(i);
		FbxGeometryElementUV* UVCoords = FbxMesh->CreateElementUV(FFbxDataConverter::ConvertToFbxString(UVChannelName));
		UVCoords->SetMappingMode(FbxLayerElement::eByControlPoint);
		UVCoords->SetReferenceMode(FbxLayerElement::eDirect);
		FbxLayerElementArrayTemplate<FbxVector2>* TexCoordArray = &UVCoords->GetDirectArray();
		TexCoordArray->AddMultiple(NumVertices);

		UVCoordsArray.Add(TexCoordArray);
	}

	//Populate UV coords for each vertex
	for (uint32 j = 0; j < NumTexCoords; j++) {
		FbxLayerElementArrayTemplate<FbxVector2>* UVArray = UVCoordsArray[j];
		for (uint32 i = 0; i < NumVertices; i++) {
			const FVector2f& SrcTextureCoord = VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(i, j);
			FbxVector2 DestUVCoord(SrcTextureCoord.X, -SrcTextureCoord.Y + 1.0f);
			UVArray->SetAt(i, DestUVCoord);
		}
	}
#endif
}

static void SaveVertexStuff(UStaticMesh* Mesh, FStaticMeshLODResources& MeshLOD)
{
	FStaticMeshVertexBuffers& VertexBuffers = MeshLOD.VertexBuffers;

	FString Data;
	Data += FString::Printf(TEXT("== Vertex Information for %s ==\n"), *Mesh->GetName());

	const uint32 NumPositionVertices = VertexBuffers.PositionVertexBuffer.GetNumVertices();
	Data += FString::Printf(TEXT("- Position Vertex Buffer (%d):\n"), NumPositionVertices);

	for (uint32 i = 0; i < NumPositionVertices; i++) {
		const FVector3f& Pos = VertexBuffers.PositionVertexBuffer.VertexPosition(i);
		Data += FString::Printf(TEXT("  - (%f, %f, %f)\n"), Pos.X, Pos.Y, Pos.Z);
	}

	const uint32 NumColorVertices = VertexBuffers.ColorVertexBuffer.GetNumVertices();
	Data += FString::Printf(TEXT("- Color Vertex Buffer (%d):\n"), NumColorVertices);

	for (uint32 i = 0; i < NumColorVertices; i++) {
		const FColor& Col = VertexBuffers.ColorVertexBuffer.VertexColor(i);
		Data += FString::Printf(TEXT("  - %s\n"), *Col.ToHex());
	}

	const uint32 NumStaticMeshVertices = VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();
	Data += FString::Printf(TEXT("- Static Mesh Tangents (%d):\n"), NumStaticMeshVertices);

	for (uint32 i = 0; i < NumStaticMeshVertices; i++) {
		const FVector4f& X = VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(i); /* Tangent */
		const FVector4f& Y = VertexBuffers.StaticMeshVertexBuffer.VertexTangentY(i); /* Binormal */
		const FVector4f& Z = VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(i); /* Normal */
		Data += FString::Printf(TEXT("  - Tangents:\n"));
		Data += FString::Printf(TEXT("    - X: (%f, %f, %f, %f)\n"), X.X, X.Y, X.Z, X.W);
		Data += FString::Printf(TEXT("    - Y: (%f, %f, %f, %f)\n"), Y.X, Y.Y, Y.Z, Y.W);
		Data += FString::Printf(TEXT("    - Z: (%f, %f, %f, %f)\n"), Z.X, Z.Y, Z.Z, Z.W);
	}

	const uint32 NumStaticMeshTexCoords = VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
	Data += FString::Printf(TEXT("- Static Mesh Tex Coords (%d):\n"), NumStaticMeshTexCoords);

	for (uint32 j = 0; j < NumStaticMeshTexCoords; j++) {
		Data += FString::Printf(TEXT("  - UVs:\n"));
		for (uint32 i = 0; i < NumStaticMeshVertices; i++) {
			const FVector2f& UV = VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(i, j);
			Data += FString::Printf(TEXT("    - (%f, %f)\n"), UV.X, UV.Y);
		}
	}

	const FString FileName = FString::Printf(TEXT("Vertex/%s/%s.txt"), *Mesh->GetPathName(), *Mesh->GetName());
	UE_LOG(LogTh3RootInstance, Display, TEXT("Saving log data to %s"), *FileName);
	FString FilePath = FPaths::ProjectSavedDir() / FileName;
	FFileHelper::SaveStringToFile(Data, *FilePath);
}

static FORCENOINLINE UStaticMesh* CompressStaticMesh(UStaticMesh* Bot, UStaticMesh* Top)
{
	if (not Bot) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("Bot is nullptr"));
		return Bot;
	}
	if (not Top) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("Top is nullptr"));
		return Bot;
	}
	if (Bot == Top) {
		UE_LOG(LogTh3RootInstance, Log, TEXT("Bot == Top"));
	} else {
		UE_LOG(LogTh3RootInstance, Log, TEXT("Bot != Top"));
	}

	FStaticMeshRenderData* BotRenderData = Bot->GetRenderData();
	if (not BotRenderData) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("No render data for Bot"));
		return Bot;
	}
	if (BotRenderData->LODResources.IsEmpty()) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("No LOD resources for Bot"));
		return Bot;
	}
	FStaticMeshLODResources& BotLOD = BotRenderData->LODResources[0];
	const FRawStaticIndexBuffer& BotIndexBuffer = BotLOD.IndexBuffer;
	if (not BotIndexBuffer.GetAllowCPUAccess()) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("No CPU access to Bot index buffer"));
		return Bot;
	}

	UE_LOG(LogTh3RootInstance, Log, TEXT("Saving vertex stuff for %s"), *Bot->GetName());
	SaveVertexStuff(Bot, BotLOD);
	
	return Bot;

#if 0
	FStaticMeshLODResources& TopLOD = Top->GetRenderData()->LODResources[0];
	const FRawStaticIndexBuffer& TopIndexBuffer = TopLOD.IndexBuffer;
	if (not TopIndexBuffer.GetAllowCPUAccess()) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("No CPU access to Top index buffer"));
		return Bot;
	}

	UStaticMesh* Out = NewObject<UStaticMesh>();

	Th3Utilities::DuplicateObjectProperties(Bot, Out);

	FStaticMeshLODResources& OutLOD = Out->GetRenderData()->LODResources[0];
	FRawStaticIndexBuffer& OutIndexBuffer = OutLOD.IndexBuffer;
	if (not OutIndexBuffer.GetAllowCPUAccess()) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("No CPU access to Out index buffer"));
		return Bot;
	}

	const TArray<FStaticMaterial>& BotMaterials = Bot->GetStaticMaterials();
	const TArray<FStaticMaterial>& TopMaterials = Top->GetStaticMaterials();

	TArray<FStaticMaterial> StaticMaterials;
	StaticMaterials.Append(BotMaterials);
	StaticMaterials.Append(TopMaterials);
	Out->SetStaticMaterials(StaticMaterials);

	/* TODO common mesh resources */

	const FStaticMeshSectionArray& BotSections = BotLOD.Sections;
	const FStaticMeshSectionArray& TopSections = TopLOD.Sections;

	OutLOD.Sections.Empty();
	OutLOD.Sections.Append(BotSections);

	const uint32 FirstTopTriangle = BotSections.Last().FirstIndex + BotSections.Last().NumTriangles * 3;

	for (int32 SecIdx = 0; SecIdx < TopSections.Num(); SecIdx++) {
		FStaticMeshSection MeshSection = TopSections[SecIdx];
		MeshSection.MaterialIndex += BotMaterials.Num();
		MeshSection.FirstIndex += FirstTopTriangle;
		const uint32_t SrcFirstIndex = TopSections[SecIdx].FirstIndex;
		const uint32_t DstFirstIndex = MeshSection.FirstIndex;
		const uint32_t NumIndices = MeshSection.NumTriangles * 3;
		if (TopIndexBuffer.Is32Bit()) {
			OutIndexBuffer.InsertIndices(DstFirstIndex, &TopIndexBuffer.AccessStream32()[SrcFirstIndex], NumIndices);
		} else {
			const FIndexArrayView& TopBufView = TopIndexBuffer.GetArrayView();
			for (uint32 TrisIdx = 0; TrisIdx < NumIndices; TrisIdx++) {
				OutIndexBuffer.SetIndex(DstFirstIndex + TrisIdx, TopBufView[SrcFirstIndex + TrisIdx]);
			}
		}
		OutLOD.Sections.Add(MeshSection);
	}

	return Out;
#endif
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

	UE_LOG(LogTh3RootInstance, Log, TEXT(" -  Compressing Item Icon for %s"), *OrigItem->GetPathName());

	UTexture2D* OrigIcon = OrigCDO->mPersistentBigIcon ? OrigCDO->mPersistentBigIcon : OrigCDO->mSmallIcon;

	NewCDO->mPersistentBigIcon = Th3Tex2DUtils::OverlayTextures(GetItemIcon(OrigCDO), CompressedIconOverlay);
	NewCDO->mSmallIcon = NewCDO->mPersistentBigIcon;

	UE_LOG(LogTh3RootInstance, Verbose, TEXT(" -  Successfully compressed Item Icon for %s"), *OrigItem->GetPathName());

	UE_LOG(LogTh3RootInstance, Log, TEXT(" -  Compressing Conveyor Mesh for %s..."), *OrigItem->GetPathName());

	NewCDO->mConveyorMesh = CompressStaticMesh(OrigCDO->mConveyorMesh, UFGItemDescriptor::GetItemMesh(OrigItem));

	UE_LOG(LogTh3RootInstance, Log, TEXT(" -  Successfully compressed Conveyor Mesh for %s"), *OrigItem->GetPathName());

	MakeCompressionRecipes(OrigItem, NewItem);

	ItemToCompressedMap.Add(OrigItem, NewItem);
	return NewItem;
}

static bool SoftPtrAssetNameContains(const TSoftClassPtr<UObject>& Producer, const TCHAR* ClassName)
{
	return Producer.GetAssetName().Contains(FString(ClassName));
}

static int32 GetStackSize(const FItemAmount& Amount)
{
	return Amount.ItemClass.GetDefaultObject()->GetStackSize(Amount.ItemClass);
}

bool UTh3RootInstance::CanRecipeBeCompressed(const TSubclassOf<UFGRecipe>& Recipe)
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
	/* Do not compress Upgradeable Machines' upgrade packs */
	if (Recipe->GetPackage()->GetName().StartsWith(TEXT("/UpgradeableMachines/"))) {
		return false;
	}
	/* Do not compress Build Gun recipes */
	const auto IsBuildGunRecipe = [](const TSoftClassPtr<UObject>& Producer) { return SoftPtrAssetNameContains(Producer, TEXT("BuildGun")); };
	if (Algo::AnyOf(RecipeCDO->mProducedIn, IsBuildGunRecipe)) {
		return false;
	}
	/* Do not compress Customizer recipes */
	if (RecipeCDO->mMaterialCustomizationRecipe.Get()) {
		return false;
	}
	/* Do not compress our own (de)compression recipes */
	if (RecipesToRegister.Contains(Recipe)) {
		UE_LOG(LogTh3RootInstance, Error, TEXT("[MOD BUG] Attempted to re-compress Recipe %s"), *Recipe->GetPathName());
		return false;
	}
	/*
	 * Do not compress recipes involving items whose stack size is
	 * too small to be compressed continuously (2x in the check).
	 */
	const auto IsStackSizeEnough = [this](const FItemAmount& Amount) { return ItemToCompressedMap.Contains(Amount.ItemClass) or GetStackSize(Amount) >= 2 * CompressionRatio; };
	if (not Algo::AllOf(RecipeCDO->mIngredients, IsStackSizeEnough)) {
		return false;
	}
	if (not Algo::AllOf(RecipeCDO->mProduct, IsStackSizeEnough)) {
		return false;
	}
	return true;
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

TSubclassOf<UFGRecipe> UTh3RootInstance::CompressRecipe(const TSubclassOf<UFGRecipe>& OrigRecipe)
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
	const auto CompressItemAmounts = [this](const FItemAmount& Amount) { return FItemAmount(CompressedFormOf(Amount.ItemClass), Amount.Amount); };
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

void UTh3RootInstance::CompressAllRecipes()
{
	UE_LOG(LogTh3RootInstance, Warning, TEXT("GET RECIPES START"));

	TSet<FTopLevelAssetPath> AllRecipes;
	Th3Utilities::DiscoverSubclassesOf(AllRecipes, UFGRecipe::StaticClass());
	UE_LOG(LogTh3RootInstance, Warning, TEXT("GET RECIPES GOT THEM"));
	Th3Utilities::TransformForEachIf(AllRecipes, Th3Utilities::LoadTopLevelPathSync<UFGRecipe>, TH3_PROJECTION_THIS(CanRecipeBeCompressed), TH3_PROJECTION_THIS(CompressRecipe));
	UE_LOG(LogTh3RootInstance, Warning, TEXT("GET RECIPES PROCESS END"));
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
	Algo::TransformIf(Unlock->mRecipes, NewRecipes, TH3_PROJECTION_THIS(CanRecipeBeCompressed), TH3_PROJECTION_THIS(CompressRecipe));
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

void UTh3RootInstance::CompressAllSchematics()
{
	UE_LOG(LogTh3RootInstance, Display, TEXT("Looking for Schematics..."));
	TSet<FTopLevelAssetPath> AllSchematics;
	Th3Utilities::DiscoverSubclassesOf(AllSchematics, UFGSchematic::StaticClass());
	UE_LOG(LogTh3RootInstance, Display, TEXT("Processing %d Schematics..."), AllSchematics.Num());
	Th3Utilities::TransformForEach(AllSchematics, Th3Utilities::LoadTopLevelPathSync<UFGSchematic>, TH3_PROJECTION_THIS(CompressSchematicUnlocks));
	UE_LOG(LogTh3RootInstance, Display, TEXT("Done processing schematics"));
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
		Algo::ForEach(RecipesToRegister, [&Registry](const auto& Recipe) { Registry->RegisterRecipe(TEXT("Th3RecipeMod"), Recipe); });
		UE_LOG(LogTh3RootInstance, Display, TEXT("Done registering recipes"));
	}
}