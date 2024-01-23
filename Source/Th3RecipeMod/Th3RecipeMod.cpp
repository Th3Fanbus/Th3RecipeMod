/* SPDX-License-Identifier: MPL-2.0 */

#include "Th3RecipeMod.h"

DEFINE_LOG_CATEGORY(LogTh3RecipeMod);

void FTh3RecipeModModule::StartupModule()
{
	UE_LOG(LogTh3RecipeMod, Log, TEXT("Hello World"));
}

void FTh3RecipeModModule::ShutdownModule()
{
	UE_LOG(LogTh3RecipeMod, Log, TEXT("Goodbye Cruel World"));
}

IMPLEMENT_MODULE(FTh3RecipeModModule, Th3RecipeMod);