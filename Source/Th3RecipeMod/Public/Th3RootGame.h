/* SPDX-License-Identifier: MPL-2.0 */

#pragma once

#include <CoreMinimal.h>
#include <Module/GameInstanceModule.h>
#include <Module/GameWorldModule.h>
#include <FGResourceSinkSettings.h>
#include <Engine/DataTable.h>

#include "Th3RootGame.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTh3RootGame, Log, All);

UCLASS(Abstract)
class TH3RECIPEMOD_API UTh3RootGame : public UGameWorldModule {
	GENERATED_BODY()
public:
	UTh3RootGame();
	~UTh3RootGame();
	UGameInstanceModule* GetTh3RootInstance();
	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

	UPROPERTY(EditDefaultsOnly, Category = "Mod Configuration")
	const TSubclassOf<UFGSchematic> SchematicClass;
};
