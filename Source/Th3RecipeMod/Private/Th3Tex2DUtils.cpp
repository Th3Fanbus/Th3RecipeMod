/* SPDX-License-Identifier: MPL-2.0 */

#include "Th3Tex2DUtils.h"
#include "Th3Utilities.h"
#include "BlockMapper.h"
#include "PreciseColor.h"

#include <Algo/Accumulate.h>
#include <Math/Color.h>

DEFINE_LOG_CATEGORY(LogTh3Tex2DUtils);

static void LogTextureMipSizes(const UTexture2D* Tex)
{
	const int32 NumMips = Tex->GetNumMips();
	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT("Texture %s has %d Mips"), *Tex->GetName(), NumMips);
	for (int32 i = 0; i < NumMips; i++) {
		const FTexture2DMipMap* Mip = &Tex->GetPlatformData()->Mips[i];
		UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT(" -  Mip%d Size XYZ is %d x %d x %d"), i, Mip->SizeX, Mip->SizeY, Mip->SizeZ);
	}
}

struct TextureParams
{
	int32 SizeX;
	int32 SizeY;
	int32 MipIdxBot;
	int32 MipIdxTop;
	bool bSuccess;
};

static const EPixelFormat OUTPUT_FORMAT = EPixelFormat::PF_B8G8R8A8;

static bool IsFormatSupported(const EPixelFormat Format)
{
	switch (Format) {
	case EPixelFormat::PF_DXT1:
	case EPixelFormat::PF_DXT5:
	case EPixelFormat::PF_B8G8R8A8:
	case EPixelFormat::PF_FloatRGBA:
		fgcheck(GPixelFormats[Format].BlockSizeX <= MAX_BLOCK_SIDE);
		return true;
	default:
		return false;
	}
}

static TextureParams ChooseCompatibleMips(const UTexture2D* Bot, const UTexture2D* Top)
{
	const EPixelFormat BotFmt = Bot->GetPixelFormat();
	const EPixelFormat TopFmt = Top->GetPixelFormat();

	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT("  - Input formats are %s, %s"), GetPixelFormatString(BotFmt), GetPixelFormatString(TopFmt));

	const bool bInputFormatsSupported = IsFormatSupported(BotFmt) and IsFormatSupported(TopFmt);
	if (not bInputFormatsSupported) {
		UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT(" -  PIXEL FORMATS ARE NOT COMPATIBLE"));
		return TextureParams();
	}

	if (Bot->GetSizeX() > Top->GetSizeX()) {
		UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT(" -  SWAPPING TEXTURE ORDER"));
		TextureParams Result = ChooseCompatibleMips(Top, Bot);
		Swap(Result.MipIdxBot, Result.MipIdxTop);
		return Result;
	}

	const int32 NumMipsBot = Bot->GetNumMips();
	const int32 NumMipsTop = Top->GetNumMips();

	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT(" -  Bot is %d x %d, has %d mips (%d allowed)"), Bot->GetSizeX(), Bot->GetSizeY(), Bot->GetNumMips(), Bot->GetNumMipsAllowed(false));
	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT(" -  Top is %d x %d, has %d mips (%d allowed)"), Top->GetSizeX(), Top->GetSizeY(), Top->GetNumMips(), Top->GetNumMipsAllowed(false));

	/* TODO: Handle gracefully */
	fgcheck(Bot->GetSizeX() == Bot->GetSizeY());
	fgcheck(Top->GetSizeX() == Top->GetSizeY());
	fgcheck(Bot->GetNumMips() > 0);
	fgcheck(Top->GetNumMips() > 0);

	fgcheck(NumMipsBot > 0);
	fgcheck(NumMipsTop > 0);

	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT(" -  Bot Mip0 is %d x %d"), Bot->GetPlatformData()->Mips[0].SizeX, Bot->GetPlatformData()->Mips[0].SizeY);
	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT(" -  Top Mip0 is %d x %d"), Top->GetPlatformData()->Mips[0].SizeX, Top->GetPlatformData()->Mips[0].SizeY);

	fgcheck(Bot->GetPlatformData()->Mips[0].SizeX == Bot->GetSizeX());
	fgcheck(Bot->GetPlatformData()->Mips[0].SizeY == Bot->GetSizeY());
	fgcheck(Top->GetPlatformData()->Mips[0].SizeX == Top->GetSizeX());
	fgcheck(Top->GetPlatformData()->Mips[0].SizeY == Top->GetSizeY());

	for (int32 IdxBot = 0; IdxBot < NumMipsBot; IdxBot++) {
		for (int32 IdxTop = 0; IdxTop < NumMipsTop; IdxTop++) {
			const FTexture2DMipMap* MipBot = &Bot->GetPlatformData()->Mips[IdxBot];
			const FTexture2DMipMap* MipTop = &Top->GetPlatformData()->Mips[IdxTop];

			UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT(" -  Bot Mip %d is %d x %d"), IdxBot, MipBot->SizeX, MipBot->SizeY);
			UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT(" -  Top Mip %d is %d x %d"), IdxTop, MipTop->SizeX, MipTop->SizeY);

			if ((MipBot->SizeX != MipTop->SizeX) or (MipBot->SizeY != MipTop->SizeY)) {
				UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT("NO MATCH"));
				continue;
			}

			UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT("FOUND COMPATIBLE MIPS: Bot[%d] and Top[%d] are %d x %d"), IdxBot, IdxTop, MipBot->SizeX, MipBot->SizeY);

			const TextureParams Params = {
				.SizeX = MipBot->SizeX,
				.SizeY = MipBot->SizeY,
				.MipIdxBot = IdxBot,
				.MipIdxTop = IdxTop,
				.bSuccess = true,
			};
			return Params;
		}
	}
	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT(" -  DID NOT FIND ANY COMPATIBLE MIPS"));
	return TextureParams();
}

static void DoApplyBinaryOp(UTexture2D* Out, UTexture2D* Bot, UTexture2D* Top, const TextureParams& Params, int32 OutMipIdx, TFunction<FPreciseBlock(FPreciseBlock, FPreciseBlock)> Func)
{
	BlockMapper BotBlock = BlockMapper(Bot, Params.MipIdxBot + OutMipIdx);
	BlockMapper TopBlock = BlockMapper(Top, Params.MipIdxTop + OutMipIdx);

	if (OutMipIdx > 0) {
		FTexture2DMipMap* Mip = new FTexture2DMipMap();
		Out->GetPlatformData()->Mips.Add(Mip);
		Mip->SizeX = Params.SizeX;
		Mip->SizeY = Params.SizeY;
		Mip->SizeZ = 1;

		const FPixelFormatInfo& FmtInfo = GPixelFormats[OUTPUT_FORMAT];

		const size_t NumBlocksX = Params.SizeX / FmtInfo.BlockSizeX;
		const size_t NumBlocksY = Params.SizeY / FmtInfo.BlockSizeY;
		const size_t NumBytes = NumBlocksX * NumBlocksY * FmtInfo.BlockBytes;

		Mip->BulkData.Lock(LOCK_READ_WRITE);
		Mip->BulkData.Realloc(NumBytes);
		Mip->BulkData.Unlock();
	}

	BlockMapper OutBlock = BlockMapper(Out, OutMipIdx);

	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT("  - CREATING MIP %d"), OutMipIdx);

	for (size_t y = 0; y < Params.SizeY; y += MAX_BLOCK_SIDE) {
		for (size_t x = 0; x < Params.SizeX; x += MAX_BLOCK_SIDE) {
			OutBlock.WriteBlock(x, y, Invoke(Func, BotBlock.ReadBlock(x, y), TopBlock.ReadBlock(x, y)));
		}
	}

	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT("  - DONE MIP %d"), OutMipIdx);
}

static bool IsPow2Square(const UTexture2D* Tex)
{
	if (Tex->GetSizeX() != Tex->GetSizeY()) {
		return false;
	}
	const int32 NumBits = FMath::CountBits(Tex->GetSizeX());
	if (NumBits != 1) {
		UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT("CountBits(%u) = %d"), Tex->GetSizeX(), NumBits);
		return false;
	}
	const FPixelFormatInfo& FmtInfo = GPixelFormats[Tex->GetPixelFormat()];
	return FmtInfo.Supported and FmtInfo.BlockSizeX == FmtInfo.BlockSizeY;
}

static FString CompressionSettingsToString(TextureCompressionSettings CompSettings)
{
	UEnum* CompressionSettings = StaticEnum<TextureCompressionSettings>();
	return CompressionSettings->GetNameStringByValue((int64)CompSettings);
}

static bool AreTexturesCompatible(UTexture2D* Bot, UTexture2D* Top)
{
	if (not IsPow2Square(Bot)) {
		UE_LOG(LogTh3Tex2DUtils, Error, TEXT("CANNOT PROCESS: BOT IS NOT POW2 SQUARE"));
		return false;
	}
	if (not IsPow2Square(Top)) {
		UE_LOG(LogTh3Tex2DUtils, Error, TEXT("CANNOT PROCESS: TOP IS NOT POW2 SQUARE"));
		return false;
	}
	return true;
}

static UTexture2D* ApplyBinaryOp(UTexture2D* Bot, UTexture2D* Top, TFunction<FPreciseBlock(FPreciseBlock, FPreciseBlock)> Func)
{
	if (not Bot) {
		UE_LOG(LogTh3Tex2DUtils, Error, TEXT("Got a nullptr Bot"));
		return Bot;
	}
	if (not Top) {
		UE_LOG(LogTh3Tex2DUtils, Error, TEXT("Got a nullptr Top"));
		return Bot;
	}
	UE_LOG(LogTh3Tex2DUtils, Log, TEXT("Processing %s..."), *Bot->GetName());

	UE_LOG(LogTh3Tex2DUtils, Log, TEXT(" -  Bot is %d x %d, has %d mips (%d allowed), format %s, SRGB %d, Comp %s"), Bot->GetSizeX(), Bot->GetSizeY(), Bot->GetNumMips(), Bot->GetNumMipsAllowed(false), GetPixelFormatString(Bot->GetPixelFormat()), Bot->SRGB, *CompressionSettingsToString(Bot->CompressionSettings));
	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT(" -  Top is %d x %d, has %d mips (%d allowed), format %s, SRGB %d, Comp %s"), Top->GetSizeX(), Top->GetSizeY(), Top->GetNumMips(), Top->GetNumMipsAllowed(false), GetPixelFormatString(Top->GetPixelFormat()), Top->SRGB, *CompressionSettingsToString(Top->CompressionSettings));

	if (not AreTexturesCompatible(Bot, Top)) {
		UE_LOG(LogTh3Tex2DUtils, Error, TEXT("CANNOT PROCESS: INCOMPATIBLE"));
		return Bot;
	}

	TextureParams Params = ChooseCompatibleMips(Bot, Top);

	if (not Params.bSuccess) {
		UE_LOG(LogTh3Tex2DUtils, Error, TEXT("Could not generate new Texture, using default"));
		return Bot;
	}

	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT("Bot Pending Init or Streaming is %d"), Bot->HasPendingInitOrStreaming());
	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT("Top Pending Init or Streaming is %d"), Top->HasPendingInitOrStreaming());

	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT("Bot Fully Streamed In is %d"), Bot->IsFullyStreamedIn());
	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT("Top Fully Streamed In is %d"), Top->IsFullyStreamedIn());

	const FString NewName = FString::Printf(TEXT("Compressed_%s"), *Bot->GetName());
	UTexture2D* Out = UTexture2D::CreateTransient(Params.SizeX, Params.SizeY, OUTPUT_FORMAT, FName(NewName));

	LogTextureMipSizes(Out);

	const int32 NumMipsBot = Bot->GetNumMips();
	const int32 NumMipsTop = Top->GetNumMips();

	int32 MipIdx = 0;
	while (true) {
		if (Params.SizeX < 4) {
			break;
		}
		if (Params.MipIdxBot + MipIdx >= NumMipsBot) {
			break;
		}
		if (Params.MipIdxTop + MipIdx >= NumMipsTop) {
			break;
		}
		DoApplyBinaryOp(Out, Bot, Top, Params, MipIdx, Func);
		MipIdx++;
		Params.SizeX >>= 1;
		Params.SizeY >>= 1;
	}

	UE_LOG(LogTh3Tex2DUtils, Verbose, TEXT("Generated New Texture"));

	LogTextureMipSizes(Out);

	Out->UpdateResource();

	return Out;
}

static FPreciseBlock OverlayBlocks(const FPreciseBlock& Bot, const FPreciseBlock& Top)
{
	FPreciseBlock Out;
	Th3Utilities::StaticFor<0, MAX_BLOCK_PIXELS>()([&](size_t i) {
		Out.Data[i] = FPreciseColor::Over(Bot.Data[i], Top.Data[i]);
	});
	return Out;
}

UTexture2D* Th3Tex2DUtils::OverlayTextures(UTexture2D* Bot, UTexture2D* Top)
{
	return ApplyBinaryOp(Bot, Top, &OverlayBlocks);
}
