/* SPDX-License-Identifier: MPL-2.0 */

#pragma once

#include <CoreMinimal.h>
#include <Engine/TextureDefines.h>
#include <Engine/Texture2D.h>
#include <Rendering/Texture2DResource.h>

DECLARE_LOG_CATEGORY_EXTERN(LogTh3Tex2DUtils, Log, All);

namespace Th3Tex2DUtils
{
	UTexture2D* OverlayTextures(UTexture2D* Bot, UTexture2D* Top);
};
