//
//  RNMetalTextureInfo.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNMetalTextureInfo.h"
#include "RNMetalRenderer.h"

namespace RN
{
	namespace MetalTextureInfo
	{
		const FormatInfo &GetFormatInfo(Texture::Format format)
		{
			static const FormatInfo formatInfos[] = {
				{ MTLPixelFormatRGBA8Unorm_sRGB, 1, 1, false, false },
				{ MTLPixelFormatBGRA8Unorm_sRGB, 1, 1, false, false },
				{ MTLPixelFormatRGBA8Unorm_sRGB, 1, 1, false, false },
				{ MTLPixelFormatBGRA8Unorm_sRGB, 1, 1, false, false },
				{ MTLPixelFormatRGBA8Unorm, 1, 1, false, false },
				{ MTLPixelFormatBGRA8Unorm, 1, 1, false, false },
				{ MTLPixelFormatRGB10A2Unorm, 1, 1, false, false },
				{ MTLPixelFormatBGR10A2Unorm, 1, 1, false, false },
				{ MTLPixelFormatRG11B10Float, 1, 1, false, false },
				{ MTLPixelFormatR8Unorm, 1, 1, false, false },
				{ MTLPixelFormatRG8Unorm, 1, 1, false, false },
				{ MTLPixelFormatRGBA8Unorm, 1, 1, false, false },
				{ MTLPixelFormatR16Float, 1, 1, false, false },
				{ MTLPixelFormatRG16Float, 1, 1, false, false },
				{ MTLPixelFormatRGBA16Float, 1, 1, false, false },
				{ MTLPixelFormatRGBA16Float, 1, 1, false, false },
				{ MTLPixelFormatR32Float, 1, 1, false, false },
				{ MTLPixelFormatRG32Float, 1, 1, false, false },
				{ MTLPixelFormatRGBA32Float, 1, 1, false, false },
				{ MTLPixelFormatRGBA32Float, 1, 1, false, false },
				{ MTLPixelFormatDepth16Unorm, 1, 1, true, false },
#if RN_PLATFORM_MAC_OS
				{ MTLPixelFormatDepth24Unorm_Stencil8, 1, 1, true, false },
#else
				{ MTLPixelFormatInvalid, 1, 1, true, false },
#endif
				{ MTLPixelFormatDepth32Float, 1, 1, true, false },
				{ MTLPixelFormatStencil8, 1, 1, false, true },
#if RN_PLATFORM_MAC_OS
				{ MTLPixelFormatDepth24Unorm_Stencil8, 1, 1, true, true },
#else
				{ MTLPixelFormatInvalid, 1, 1, true, true },
#endif
				{ MTLPixelFormatDepth32Float_Stencil8, 1, 1, true, true },
#if RN_PLATFORM_MAC_OS
				{ MTLPixelFormatBC1_RGBA_sRGB, 4, 4, false, false },
				{ MTLPixelFormatBC2_RGBA_sRGB, 4, 4, false, false },
				{ MTLPixelFormatBC3_RGBA_sRGB, 4, 4, false, false },
				{ MTLPixelFormatBC7_RGBAUnorm_sRGB, 4, 4, false, false },
				{ MTLPixelFormatBC1_RGBA, 4, 4, false, false },
				{ MTLPixelFormatBC2_RGBA, 4, 4, false, false },
				{ MTLPixelFormatBC3_RGBA, 4, 4, false, false },
				{ MTLPixelFormatBC4_RUnorm, 4, 4, false, false },
				{ MTLPixelFormatBC5_RGUnorm, 4, 4, false, false },
				{ MTLPixelFormatBC7_RGBAUnorm, 4, 4, false, false },
#else
				{ MTLPixelFormatInvalid, 4, 4, false, false },
				{ MTLPixelFormatInvalid, 4, 4, false, false },
				{ MTLPixelFormatInvalid, 4, 4, false, false },
				{ MTLPixelFormatInvalid, 4, 4, false, false },
				{ MTLPixelFormatInvalid, 4, 4, false, false },
				{ MTLPixelFormatInvalid, 4, 4, false, false },
				{ MTLPixelFormatInvalid, 4, 4, false, false },
				{ MTLPixelFormatInvalid, 4, 4, false, false },
				{ MTLPixelFormatInvalid, 4, 4, false, false },
				{ MTLPixelFormatInvalid, 4, 4, false, false },
#endif
#if RN_PLATFORM_MAC_OS
				{ MTLPixelFormatInvalid, 4, 4, false, false },
				{ MTLPixelFormatInvalid, 5, 4, false, false },
				{ MTLPixelFormatInvalid, 5, 5, false, false },
				{ MTLPixelFormatInvalid, 6, 5, false, false },
				{ MTLPixelFormatInvalid, 6, 6, false, false },
				{ MTLPixelFormatInvalid, 8, 5, false, false },
				{ MTLPixelFormatInvalid, 8, 6, false, false },
				{ MTLPixelFormatInvalid, 8, 8, false, false },
				{ MTLPixelFormatInvalid, 10, 5, false, false },
				{ MTLPixelFormatInvalid, 10, 6, false, false },
				{ MTLPixelFormatInvalid, 10, 8, false, false },
				{ MTLPixelFormatInvalid, 10, 10, false, false },
				{ MTLPixelFormatInvalid, 12, 10, false, false },
				{ MTLPixelFormatInvalid, 12, 12, false, false },
				{ MTLPixelFormatInvalid, 4, 4, false, false },
				{ MTLPixelFormatInvalid, 5, 4, false, false },
				{ MTLPixelFormatInvalid, 5, 5, false, false },
				{ MTLPixelFormatInvalid, 6, 5, false, false },
				{ MTLPixelFormatInvalid, 6, 6, false, false },
				{ MTLPixelFormatInvalid, 8, 5, false, false },
				{ MTLPixelFormatInvalid, 8, 6, false, false },
				{ MTLPixelFormatInvalid, 8, 8, false, false },
				{ MTLPixelFormatInvalid, 10, 5, false, false },
				{ MTLPixelFormatInvalid, 10, 6, false, false },
				{ MTLPixelFormatInvalid, 10, 8, false, false },
				{ MTLPixelFormatInvalid, 10, 10, false, false },
				{ MTLPixelFormatInvalid, 12, 10, false, false },
				{ MTLPixelFormatInvalid, 12, 12, false, false },
#else
				{ MTLPixelFormatASTC_4x4_sRGB, 4, 4, false, false },
				{ MTLPixelFormatASTC_5x4_sRGB, 5, 4, false, false },
				{ MTLPixelFormatASTC_5x5_sRGB, 5, 5, false, false },
				{ MTLPixelFormatASTC_6x5_sRGB, 6, 5, false, false },
				{ MTLPixelFormatASTC_6x6_sRGB, 6, 6, false, false },
				{ MTLPixelFormatASTC_8x5_sRGB, 8, 5, false, false },
				{ MTLPixelFormatASTC_8x6_sRGB, 8, 6, false, false },
				{ MTLPixelFormatASTC_8x8_sRGB, 8, 8, false, false },
				{ MTLPixelFormatASTC_10x5_sRGB, 10, 5, false, false },
				{ MTLPixelFormatASTC_10x6_sRGB, 10, 6, false, false },
				{ MTLPixelFormatASTC_10x8_sRGB, 10, 8, false, false },
				{ MTLPixelFormatASTC_10x10_sRGB, 10, 10, false, false },
				{ MTLPixelFormatASTC_12x10_sRGB, 12, 10, false, false },
				{ MTLPixelFormatASTC_12x12_sRGB, 12, 12, false, false },
				{ MTLPixelFormatASTC_4x4_LDR, 4, 4, false, false },
				{ MTLPixelFormatASTC_5x4_LDR, 5, 4, false, false },
				{ MTLPixelFormatASTC_5x5_LDR, 5, 5, false, false },
				{ MTLPixelFormatASTC_6x5_LDR, 6, 5, false, false },
				{ MTLPixelFormatASTC_6x6_LDR, 6, 6, false, false },
				{ MTLPixelFormatASTC_8x5_LDR, 8, 5, false, false },
				{ MTLPixelFormatASTC_8x6_LDR, 8, 6, false, false },
				{ MTLPixelFormatASTC_8x8_LDR, 8, 8, false, false },
				{ MTLPixelFormatASTC_10x5_LDR, 10, 5, false, false },
				{ MTLPixelFormatASTC_10x6_LDR, 10, 6, false, false },
				{ MTLPixelFormatASTC_10x8_LDR, 10, 8, false, false },
				{ MTLPixelFormatASTC_10x10_LDR, 10, 10, false, false },
				{ MTLPixelFormatASTC_12x10_LDR, 12, 10, false, false },
				{ MTLPixelFormatASTC_12x12_LDR, 12, 12, false, false },
#endif
				{ MTLPixelFormatInvalid, 1, 1, false, false }
			};

			static_assert(sizeof(formatInfos) / sizeof(formatInfos[0]) == static_cast<size_t>(Texture::Format::Invalid) + 1, "Metal texture format table is out of sync");

			uint32 index = static_cast<uint32>(format);
			uint32 invalidIndex = static_cast<uint32>(Texture::Format::Invalid);
			if(index > invalidIndex)
				index = invalidIndex;

			return formatInfos[index];
		}

		MTLPixelFormat GetPixelFormat(Texture::Format format)
		{
			return GetFormatInfo(format).pixelFormat;
		}

		uint32 GetBlockWidth(Texture::Format format)
		{
			return GetFormatInfo(format).blockWidth;
		}

		uint32 GetBlockHeight(Texture::Format format)
		{
			return GetFormatInfo(format).blockHeight;
		}

		MTLTextureDescriptor *CreateTextureDescriptor(const Texture::Descriptor &descriptor, bool isIOSurfaceBacked)
		{
			MTLTextureDescriptor *metalDescriptor = [[MTLTextureDescriptor alloc] init];

			metalDescriptor.width = descriptor.width;
			metalDescriptor.height = descriptor.height;
			metalDescriptor.resourceOptions = MetalRenderer::MetalResourceOptionsFromOptions(descriptor.accessOptions);
			metalDescriptor.mipmapLevelCount = descriptor.mipMaps;
			metalDescriptor.pixelFormat = GetPixelFormat(descriptor.format);
			metalDescriptor.sampleCount = descriptor.sampleCount;

			MTLTextureUsage usage = 0;

			if(descriptor.usageHint & Texture::UsageHint::ShaderRead)
				usage |= MTLTextureUsageShaderRead;
			if(descriptor.usageHint & Texture::UsageHint::ShaderWrite)
				usage |= MTLTextureUsageShaderWrite;
			if(descriptor.usageHint & Texture::UsageHint::RenderTarget)
			{
				usage |= MTLTextureUsageRenderTarget;
				metalDescriptor.storageMode = descriptor.accessOptions == GPUBuffer::AccessOptions::ReadWrite ? MTLStorageModeShared : MTLStorageModePrivate;
			}

#if RN_PLATFORM_MAC_OS
			if(isIOSurfaceBacked)
			{
				metalDescriptor.storageMode = MTLStorageModeManaged;
			}
#endif

			metalDescriptor.usage = usage;

			switch(descriptor.type)
			{
				case Texture::Type::Type1D:
					metalDescriptor.textureType = MTLTextureType1D;
					metalDescriptor.depth = descriptor.depth;
					break;
				case Texture::Type::Type1DArray:
					metalDescriptor.textureType = MTLTextureType1DArray;
					metalDescriptor.depth = 1;
					metalDescriptor.arrayLength = descriptor.depth;
					break;
				case Texture::Type::Type2D:
					metalDescriptor.textureType = descriptor.sampleCount > 1 ? MTLTextureType2DMultisample : MTLTextureType2D;
					metalDescriptor.depth = descriptor.depth;
					break;
				case Texture::Type::Type2DArray:
					if(@available(iOS 14, macOS 10.14, *)) {
						metalDescriptor.textureType = descriptor.sampleCount > 1 ? MTLTextureType2DMultisampleArray : MTLTextureType2DArray;
					}
					else
					{
						metalDescriptor.textureType = MTLTextureType2DArray;
					}
					metalDescriptor.depth = 1;
					metalDescriptor.arrayLength = descriptor.depth;
					break;
				case Texture::Type::TypeCube:
					metalDescriptor.textureType = MTLTextureTypeCube;
					metalDescriptor.depth = 1;
					metalDescriptor.arrayLength = 1;
					break;
				case Texture::Type::TypeCubeArray:
					metalDescriptor.textureType = MTLTextureTypeCubeArray;
					metalDescriptor.depth = 1;
					metalDescriptor.arrayLength = descriptor.depth;
					break;
				case Texture::Type::Type3D:
					metalDescriptor.textureType = MTLTextureType3D;
					metalDescriptor.depth = descriptor.depth;
					break;
			}

			return metalDescriptor;
		}
	}
}
