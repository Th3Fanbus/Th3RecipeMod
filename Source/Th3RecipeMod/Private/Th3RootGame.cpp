/* SPDX-License-Identifier: MPL-2.0 */

#include "Th3RootGame.h"
#include "Th3RootInstance.h"

#include <Module/GameInstanceModuleManager.h>
#include <Registry/ModContentRegistry.h>
#include <FGRecipe.h>
#include <FGRecipeManager.h>
#include <Logging/LogMacros.h>
#include <Logging/StructuredLog.h>
#include <Algo/ForEach.h>

DEFINE_LOG_CATEGORY(LogTh3RootGame);

UTh3RootGame::UTh3RootGame()
{
	UE_LOG(LogTh3RootGame, Display, TEXT("Hello Game World %s"), *this->GetPathName());
}

UTh3RootGame::~UTh3RootGame()
{
	UE_LOG(LogTh3RootGame, Display, TEXT("Goodbye Cruel Game World"));
}

UGameInstanceModule* UTh3RootGame::GetTh3RootInstance()
{
	UWorld* World = GetWorld();
	if (not World) {
		UE_LOG(LogTh3RootGame, Error, TEXT(" -  nullptr World"));
		return nullptr;
	}
	UGameInstance* GameInstance = World->GetGameInstance();
	if (not GameInstance) {
		UE_LOG(LogTh3RootGame, Error, TEXT(" -  nullptr GameInstance"));
		return nullptr;
	}
	UGameInstanceModuleManager* ModuleManager = GameInstance->GetSubsystem<UGameInstanceModuleManager>();
	if (not ModuleManager) {
		UE_LOG(LogTh3RootGame, Error, TEXT(" -  nullptr ModuleManager"));
		return nullptr;
	}
	UGameInstanceModule* InstanceMod = ModuleManager->FindModule(TEXT("Th3RecipeMod"));
	if (not InstanceMod) {
		UE_LOG(LogTh3RootGame, Error, TEXT(" -  nullptr InstanceMod"));
		return nullptr;
	}
	return InstanceMod;
}

void UTh3RootGame::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	Super::DispatchLifecycleEvent(Phase);

	UE_LOG(LogTh3RootGame, Display, TEXT("Dispatching Phase %s on %s"), *LifecyclePhaseToString(Phase), *this->GetPathName());

	UTh3RootInstance* RootInstance = Cast<UTh3RootInstance>(GetTh3RootInstance());
	if (not RootInstance) {
		UE_LOG(LogTh3RootGame, Error, TEXT("Game World module could not find Game Instance module"));
		return;
	}

	if (Phase == ELifecyclePhase::POST_INITIALIZATION) {
		AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(GetWorld());
		if (not RecipeManager) {
			UE_LOG(LogTh3RootGame, Error, TEXT("Could not find recipe manager, not registering %d recipes"), RootInstance->RecipesToRegister.Num());
			return;
		}
		UE_LOG(LogTh3RootGame, Display, TEXT("Making %d (de)compression recipes available..."), RootInstance->RecipesToRegister.Num());
		Algo::ForEach(RootInstance->RecipesToRegister, [&RecipeManager](const auto& Recipe) { RecipeManager->AddAvailableRecipe(Recipe); });
		UE_LOG(LogTh3RootGame, Display, TEXT("Made (de)compression recipes available"));

		AFGResourceSinkSubsystem* SinkSubsystem = AFGResourceSinkSubsystem::Get(GetWorld());
		if (not SinkSubsystem) {
			UE_LOG(LogTh3RootGame, Error, TEXT("Could not find resource sink subsystem, compressed items cannot be sunk"));
			return;
		}
		TMap<FName, const uint8*> DummyDataMap;
		UDataTable* DefaultPointsDataTable = NewObject<UDataTable>();
		UDataTable* ExplorationPointsDataTable = NewObject<UDataTable>();
		DefaultPointsDataTable->CreateTableFromRawData(DummyDataMap, FResourceSinkPointsData::StaticStruct());
		ExplorationPointsDataTable->CreateTableFromRawData(DummyDataMap, FResourceSinkPointsData::StaticStruct());
		UE_LOG(LogTh3RootGame, Display, TEXT("Calculating Sink Points for %d items..."), RootInstance->ItemToCompressedMap.Num());
		const int32 CompressionRatio = RootInstance->CompressionRatio;
		Algo::ForEach(RootInstance->ItemToCompressedMap, [DefaultPointsDataTable, ExplorationPointsDataTable, SinkSubsystem, CompressionRatio](const auto& ItemPair) {
			const TSubclassOf<UFGItemDescriptor> OrigItem = ItemPair.Key;
			const TSubclassOf<UFGItemDescriptor> NewItem = ItemPair.Value;
			int32 NumPoints;
			EResourceSinkTrack SinkTrack;
			if (not SinkSubsystem->FindResourceSinkPointsForItem(OrigItem, NumPoints, SinkTrack)) {
				return;
			}
			FResourceSinkPointsData SinkPoints;
			SinkPoints.ItemClass = NewItem;
			SinkPoints.Points = NumPoints * CompressionRatio;
			if (SinkTrack == EResourceSinkTrack::RST_Default) {
				DefaultPointsDataTable->AddRow(NewItem->GetFName(), SinkPoints);
			} else if (SinkTrack == EResourceSinkTrack::RST_Exploration) {
				ExplorationPointsDataTable->AddRow(NewItem->GetFName(), SinkPoints);
			}
		});
		UE_LOG(LogTh3RootGame, Display, TEXT("Adding %d items to the 'Default' Sink Track..."), DefaultPointsDataTable->GetRowMap().Num());
		SinkSubsystem->SetupPointData(EResourceSinkTrack::RST_Default, DefaultPointsDataTable);
		UE_LOG(LogTh3RootGame, Display, TEXT("Adding %d items to the 'Exploration' Sink Track..."), ExplorationPointsDataTable->GetRowMap().Num());
		SinkSubsystem->SetupPointData(EResourceSinkTrack::RST_Exploration, ExplorationPointsDataTable);
		UE_LOG(LogTh3RootGame, Display, TEXT("Done setting up Resource Sink Points"));
	}
}