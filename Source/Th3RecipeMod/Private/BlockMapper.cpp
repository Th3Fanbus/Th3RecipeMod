/* SPDX-License-Identifier: MPL-2.0 */

#include "BlockMapper.h"
#include "Th3Utilities.h"

template<typename NativeBlockType>
struct BlockMapper::MapperModel : BlockMapper::MapperConcept
{
	MapperModel(UTexture2D* Texture, const int32 MipIdx) :
		Texture(Texture), MipIdx(MipIdx), Format(Texture->GetPixelFormat()),
		BlockSideX(GPixelFormats[Format].BlockSizeX),
		BlockSideY(GPixelFormats[Format].BlockSizeY),
		SizeX(Texture->GetPlatformData()->Mips[MipIdx].SizeX),
		OldBulkDataFlags(Texture->GetPlatformData()->Mips[MipIdx].BulkData.GetBulkDataFlags())
	{
		Texture->SetForceMipLevelsToBeResident(3600, 0);
		Texture->WaitForStreaming(true, false);

		FTexture2DMipMap* Mip = &Texture->GetPlatformData()->Mips[MipIdx];
		Mip->BulkData.ClearBulkDataFlags(BULKDATA_AlwaysAllowDiscard | BULKDATA_SingleUse);
	}

	void DecodeBlock(FPreciseBlock& OutBlock, const NativeBlockType& InBlock, const size_t Offset) const
	{
		fgcheckf(false, TEXT("%s is unimplemented for %s"), *FString(__func__), GetPixelFormatString(Format));
	}

	NativeBlockType EncodeBlock(const FPreciseBlock& InBlock, const size_t Offset) const
	{
		fgcheckf(false, TEXT("%s is unimplemented for %s"), *FString(__func__), GetPixelFormatString(Format));
		return NativeBlockType();
	}

	virtual FPreciseBlock ReadBlock(size_t x, size_t y) const override
	{
		const FTexture2DMipMap* Mip = &Texture->GetPlatformData()->Mips[MipIdx];
		const NativeBlockType* Src = reinterpret_cast<const NativeBlockType*>(Mip->BulkData.LockReadOnly());
		fgcheck(Src);

		const size_t PreciseBlockNum = MAX_BLOCK_SIDE * MAX_BLOCK_SIDE;
		const size_t SubBlockNum = BlockSideX * BlockSideY;

		FPreciseBlock Dst;
		for (size_t i = 0; i < PreciseBlockNum; i += SubBlockNum) {
			const size_t h = i / MAX_BLOCK_SIDE;
			const size_t w = i % MAX_BLOCK_SIDE;
			const size_t Offset = ((y + h) * SizeX + (x + w) * BlockSideX) / SubBlockNum;
			DecodeBlock(Dst, Src[Offset], i);
		}
		Mip->BulkData.Unlock();
		return Dst;
	}

	virtual void WriteBlock(size_t x, size_t y, const FPreciseBlock& InBlock) override
	{
		FTexture2DMipMap* Mip = &Texture->GetPlatformData()->Mips[MipIdx];
		NativeBlockType* Dst = reinterpret_cast<NativeBlockType*>(Mip->BulkData.Lock(LOCK_READ_WRITE));
		fgcheck(Dst);

		const size_t PreciseBlockNum = MAX_BLOCK_SIDE * MAX_BLOCK_SIDE;
		const size_t SubBlockNum = BlockSideX * BlockSideY;

		for (size_t i = 0; i < PreciseBlockNum; i += SubBlockNum) {
			const size_t h = i / MAX_BLOCK_SIDE;
			const size_t w = i % MAX_BLOCK_SIDE;
			const size_t Offset = ((y + h) * SizeX + (x + w) * BlockSideX) / SubBlockNum;
			Dst[Offset] = EncodeBlock(InBlock, i);
		}
		Mip->BulkData.Unlock();
	}
private:
	virtual void OnDestruction() override
	{
		FTexture2DMipMap* Mip = &Texture->GetPlatformData()->Mips[MipIdx];
		Mip->BulkData.ResetBulkDataFlags(OldBulkDataFlags);
		Texture->SetForceMipLevelsToBeResident(0, 0);
	}

	UTexture2D* Texture;
	const int32 MipIdx;
	const EPixelFormat Format;
	const size_t BlockSideX;
	const size_t BlockSideY;
	const size_t SizeX;
	const uint32 OldBulkDataFlags;
};

static FORCEINLINE FPreciseColor GetDXT1Color(const FPreciseColor& Color0, const FPreciseColor& Color1, const uint8_t Code, const bool bUseThirds = true)
{
	if (bUseThirds) {
		switch (Code) {
		case 0:
			return Color0;
		case 1:
			return Color1;
		case 2:
			return FPreciseColor::Average({ Color0, Color0, Color1 });
		case 3:
			return FPreciseColor::Average({ Color0, Color1, Color1 });
		}
	} else {
		switch (Code) {
		case 0:
			return Color0;
		case 1:
			return Color1;
		case 2:
			return FPreciseColor::Average({ Color0, Color1 });
		case 3:
			return FPreciseColor();
		}
	}
	fgcheckf(false, TEXT("Unhandled case: code %x"), Code);
	return FPreciseColor();
}

static FORCEINLINE double GetDXT5Alpha(const uint8_t Alpha0, const uint8_t Alpha1, const uint8_t Code)
{
	if (Code == 0) {
		return Alpha0;
	}
	if (Code == 1) {
		return Alpha1;
	}
	const double N1 = Code - 1;
	if (Alpha0 > Alpha1) {
		const double N0 = 7 - N1;
		return (N0 * Alpha0 + N1 * Alpha1) / 7;
	} else {
		if (Code == 6) {
			return 0;
		} else if (Code == 7) {
			return 0xff;
		} else {
			const double N0 = 5 - N1;
			return (N0 * Alpha0 + N1 * Alpha1) / 5;
		}
	}
}

template<> void BlockMapper::MapperModel<FDXT1>::DecodeBlock(FPreciseBlock& OutBlock, const FDXT1& InBlock, const size_t Offset) const
{
	const size_t DXT1_NUM_PIXELS = 16;

	const FPreciseColor Color0(InBlock.Color[0]);
	const FPreciseColor Color1(InBlock.Color[1]);

	Th3Utilities::StaticFor<0, DXT1_NUM_PIXELS>()([=, &OutBlock](size_t i) {
		const bool bUseThirds = InBlock.Color[0].Value > InBlock.Color[1].Value;
		const uint8_t ColorCode = (InBlock.Indices >> (2 * i)) & 0x03;
		OutBlock.Data[i] = GetDXT1Color(Color0, Color1, ColorCode, bUseThirds);
	});
}

template<> void BlockMapper::MapperModel<FDXT5>::DecodeBlock(FPreciseBlock& OutBlock, const FDXT5& InBlock, const size_t Offset) const
{
	const size_t DXT5_NUM_PIXELS = 16;

	const FPreciseColor Color0(InBlock.DXT1.Color[0]);
	const FPreciseColor Color1(InBlock.DXT1.Color[1]);

	uint64_t AlphaBits = 0;
	const uint8_t* Bits = &InBlock.Alpha[2];
	Th3Utilities::StaticFor<0, 6>()([&AlphaBits, Bits](size_t i) {
		const uint64_t Mask = Bits[i];
		AlphaBits |= Mask << (8 * i);
	});

	Th3Utilities::StaticFor<0, DXT5_NUM_PIXELS>()([=, &OutBlock](size_t i) {
		const uint8_t ColorCode = (InBlock.DXT1.Indices >> (2 * i)) & 0x03;
		const uint8_t AlphaCode = (AlphaBits >> (3 * i)) & 0x07;
		const double FloatAlpha = GetDXT5Alpha(InBlock.Alpha[0], InBlock.Alpha[1], AlphaCode) / 255.0;
		OutBlock.Data[i] = GetDXT1Color(Color0, Color1, ColorCode).WithAlpha(FloatAlpha);
	});
}

template<> void BlockMapper::MapperModel<FFloat16Color>::DecodeBlock(FPreciseBlock& OutBlock, const FFloat16Color& InBlock, const size_t Offset) const
{
	OutBlock.Data[Offset] = FPreciseColor(InBlock);
}

template<> void BlockMapper::MapperModel<FColor>::DecodeBlock(FPreciseBlock& OutBlock, const FColor& InBlock, const size_t Offset) const
{
	OutBlock.Data[Offset] = FPreciseColor(InBlock);
}

template<> FColor BlockMapper::MapperModel<FColor>::EncodeBlock(const FPreciseBlock& InBlock, const size_t Offset) const
{
	return InBlock.Data[Offset].ToFColor(false);
}

TSharedPtr<BlockMapper::MapperConcept> BlockMapper::MakeMapper(UTexture2D* Texture, const int32 MipIdx)
{
	fgcheck(Texture);
	const int32 NumMips = Texture->GetNumMipsAllowed(false);
	fgcheckf(MipIdx < NumMips, TEXT("Requested mip %d, but texture %s only has %d mips"), MipIdx, *Texture->GetPathName(), NumMips);
	const EPixelFormat Format = Texture->GetPixelFormat();
	switch (Format) {
	case EPixelFormat::PF_DXT1:
		return MakeShared<MapperModel<FDXT1>>(MapperModel<FDXT1>(Texture, MipIdx));
	case EPixelFormat::PF_DXT5:
		return MakeShared<MapperModel<FDXT5>>(MapperModel<FDXT5>(Texture, MipIdx));
	case EPixelFormat::PF_B8G8R8A8:
		return MakeShared<MapperModel<FColor>>(MapperModel<FColor>(Texture, MipIdx));
	case EPixelFormat::PF_FloatRGBA:
		return MakeShared<MapperModel<FFloat16Color>>(MapperModel<FFloat16Color>(Texture, MipIdx));
	default:
		fgcheckf(false, TEXT("Unsupported format %s, cannot create a block mapper for texture %s"), GetPixelFormatString(Format), *Texture->GetPathName());
		return nullptr;
	}
}
