/* SPDX-License-Identifier: MPL-2.0 */

#pragma once

#include <CoreMinimal.h>
#include <Modules/ModuleManager.h>
#include <Logging/LogMacros.h>
#include <Logging/StructuredLog.h>

DECLARE_LOG_CATEGORY_EXTERN(LogTh3RecipeMod, Log, All);

class FTh3RecipeModModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
private:
	void EnableGlobalStaticMeshCPUAccess();
};
