/* SPDX-License-Identifier: MPL-2.0 */

#pragma once

#include "PreciseColor.h"

#include <CoreMinimal.h>
#include <Math/Color.h>
#include <Engine/TextureDefines.h>
#include <Engine/Texture2D.h>
#include <Rendering/Texture2DResource.h>

static_assert(sizeof(FDXTColor16) == sizeof(uint16_t), "DXT color size mismatch");
static_assert(sizeof(FColor) == sizeof(uint32_t), "FColor size mismatch");
static_assert(sizeof(FDXT1) == 8, "DXT1 block size mismatch");
static_assert(sizeof(FDXT5) == 16, "DXT5 block size mismatch");

/* Current Max: DXT5 with 4x4 blocks */
static const size_t MAX_BLOCK_SIDE = 4;
static const size_t MAX_BLOCK_PIXELS = MAX_BLOCK_SIDE * MAX_BLOCK_SIDE;

struct FPreciseBlock
{
	FPreciseColor Data[MAX_BLOCK_PIXELS];
};

class BlockMapper final
{
private:
	struct MapperConcept
	{
		virtual ~MapperConcept() = default;
		virtual FPreciseBlock ReadBlock(size_t x, size_t y) const = 0;
		virtual void WriteBlock(size_t x, size_t y, const FPreciseBlock& InBlock) = 0;
	protected:
		friend class BlockMapper;
		virtual void OnDestruction() = 0;
	};
	template<typename NativeBlockType> struct MapperModel;

	static TSharedPtr<MapperConcept> MakeMapper(UTexture2D* Texture, const int32 MipIdx);
	TSharedPtr<MapperConcept> Mapper;
public:
	BlockMapper(UTexture2D* Texture, const int32 MipIdx) : Mapper(MakeMapper(Texture, MipIdx))
	{
	}
	FPreciseBlock ReadBlock(size_t x, size_t y) const
	{
		return Mapper->ReadBlock(x, y);
	}
	void WriteBlock(size_t x, size_t y, const FPreciseBlock& InBlock)
	{
		Mapper->WriteBlock(x, y, InBlock);
	}
	~BlockMapper()
	{
		Mapper->OnDestruction();
	}
};
