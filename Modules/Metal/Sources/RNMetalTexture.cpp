//
//  RNMetalTexture.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#import <Metal/Metal.h>
#include "RNMetalTexture.h"
#include "RNMetalRenderer.h"

namespace RN
{
	RNDefineMeta(MetalTexture, Texture)

	MetalTexture::MetalTexture(MetalRenderer *renderer, void *texture, const Descriptor &descriptor) :
		Texture(descriptor),
		_renderer(renderer),
		_texture(texture)
	{
	}

	MetalTexture::~MetalTexture()
	{
		id<MTLTexture> texture = (id<MTLTexture>)_texture;
		[texture release];
	}

	void MetalTexture::SetData(uint32 mipmapLevel, const void *bytes, size_t bytesPerRow, size_t numberOfRows)
	{
		SetData(GetFullRegionForMipMapLevel(mipmapLevel), mipmapLevel, bytes, bytesPerRow, numberOfRows);
	}
	void MetalTexture::SetData(const Region &region, uint32 mipmapLevel, const void *bytes, size_t bytesPerRow, size_t)
	{
		id<MTLTexture> texture = (id<MTLTexture>)_texture;
		[texture replaceRegion:MTLRegionMake3D(region.x, region.y, region.z, region.width, region.height, region.depth) mipmapLevel:mipmapLevel withBytes:bytes bytesPerRow:bytesPerRow];
	}
	void MetalTexture::SetData(const Region &region, uint32 mipmapLevel, uint32 slice, const void *bytes, size_t bytesPerRow, size_t numberOfRows)
	{
		id<MTLTexture> texture = (id<MTLTexture>)_texture;
		[texture replaceRegion:MTLRegionMake3D(region.x, region.y, region.z, region.width, region.height, region.depth) mipmapLevel:mipmapLevel slice:slice withBytes:bytes bytesPerRow:bytesPerRow bytesPerImage:bytesPerRow*numberOfRows];
	}

	void MetalTexture::GetData(void *bytes, uint32 mipmapLevel, size_t bytesPerRow, std::function<void(void)> callback) const
	{
		const Region region = GetFullRegionForMipMapLevel(mipmapLevel);
		const uint32 blockHeight = GetBlockHeight();
		const uint32 blockRows = std::max<uint32>(1, (region.height + blockHeight - 1) / blockHeight);
		const size_t bytesPerImage = bytesPerRow * blockRows;
		const size_t readbackSize = bytesPerImage * region.depth;
		RN_ASSERT(bytesPerRow > 0 && readbackSize > 0, "Metal texture readback requires a non-empty destination buffer layout");

		MetalRenderer *renderer = _renderer;
		id<MTLTexture> texture = [(id<MTLTexture>)_texture retain];
		std::function<void(void)> completion = callback;
		renderer->ScheduleRenderThreadWork([renderer, texture, region, mipmapLevel, bytes, bytesPerRow, bytesPerImage, readbackSize, completion]() {
			id<MTLCommandQueue> commandQueue = renderer->GetCommandQueue();
			id<MTLDevice> device = [commandQueue device];
			id<MTLBuffer> readbackBuffer = [device newBufferWithLength:readbackSize options:MetalRenderer::MetalResourceOptionsFromOptions(GPUResource::AccessOptions::ReadWrite)];
			RN_ASSERT(readbackBuffer, "Failed to create Metal texture readback buffer");

			id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
			RN_ASSERT(commandBuffer, "Failed to create Metal texture readback command buffer");
			id<MTLBlitCommandEncoder> commandEncoder = [commandBuffer blitCommandEncoder];
			RN_ASSERT(commandEncoder, "Failed to create Metal texture readback command encoder");
			[commandEncoder copyFromTexture:texture
								 sourceSlice:0
								 sourceLevel:mipmapLevel
								sourceOrigin:MTLOriginMake(region.x, region.y, region.z)
								  sourceSize:MTLSizeMake(region.width, region.height, region.depth)
									toBuffer:readbackBuffer
						   destinationOffset:0
					  destinationBytesPerRow:bytesPerRow
					destinationBytesPerImage:bytesPerImage];
#if RN_PLATFORM_MAC_OS
			[commandEncoder synchronizeResource:readbackBuffer];
#endif
			[commandEncoder endEncoding];

			[commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
				memcpy(bytes, [readbackBuffer contents], readbackSize);
				[readbackBuffer release];
				[texture release];
				completion();
			}];
			[commandBuffer commit];
		});
	}

	Texture::Region MetalTexture::GetFullRegionForMipMapLevel(uint32 mipmapLevel) const
	{
		const uint32 mipDepth = _descriptor.type == Texture::Type::Type3D ? std::max<uint32>(1, _descriptor.depth >> mipmapLevel) : 1;
		return Region(0, 0, 0, _descriptor.GetWidthForMipMapLevel(mipmapLevel), _descriptor.GetHeightForMipMapLevel(mipmapLevel), mipDepth);
	}

	uint32 MetalTexture::GetBlockHeight() const
	{
		switch(_descriptor.format)
		{
			case Format::RGBA_BC1_SRGB:
			case Format::RGBA_BC2_SRGB:
			case Format::RGBA_BC3_SRGB:
			case Format::RGBA_BC7_SRGB:
			case Format::RGBA_BC1:
			case Format::RGBA_BC2:
			case Format::RGBA_BC3:
			case Format::RGBA_BC4:
			case Format::RGBA_BC5:
			case Format::RGBA_BC7:
			case Format::RGBA_ASTC_4X4_SRGB:
			case Format::RGBA_ASTC_5X4_SRGB:
			case Format::RGBA_ASTC_4X4:
			case Format::RGBA_ASTC_5X4:
				return 4;

			case Format::RGBA_ASTC_5X5_SRGB:
			case Format::RGBA_ASTC_6X5_SRGB:
			case Format::RGBA_ASTC_8X5_SRGB:
			case Format::RGBA_ASTC_10X5_SRGB:
			case Format::RGBA_ASTC_5X5:
			case Format::RGBA_ASTC_6X5:
			case Format::RGBA_ASTC_8X5:
			case Format::RGBA_ASTC_10X5:
				return 5;

			case Format::RGBA_ASTC_6X6_SRGB:
			case Format::RGBA_ASTC_8X6_SRGB:
			case Format::RGBA_ASTC_10X6_SRGB:
			case Format::RGBA_ASTC_6X6:
			case Format::RGBA_ASTC_8X6:
			case Format::RGBA_ASTC_10X6:
				return 6;

			case Format::RGBA_ASTC_8X8_SRGB:
			case Format::RGBA_ASTC_10X8_SRGB:
			case Format::RGBA_ASTC_8X8:
			case Format::RGBA_ASTC_10X8:
				return 8;

			case Format::RGBA_ASTC_10X10_SRGB:
			case Format::RGBA_ASTC_12X10_SRGB:
			case Format::RGBA_ASTC_10X10:
			case Format::RGBA_ASTC_12X10:
				return 10;

			case Format::RGBA_ASTC_12X12_SRGB:
			case Format::RGBA_ASTC_12X12:
				return 12;

			default:
				return 1;
		}
	}

	void MetalTexture::GenerateMipMaps()
	{
		_renderer->CreateMipMapForTexture(this);
	}

	MTLPixelFormat MetalTexture::PixelFormatForTextureFormat(Format format)
	{
		switch(format)
		{
			case Format::RGBA_8_SRGB:
				return MTLPixelFormatRGBA8Unorm_sRGB;
			case Format::BGRA_8_SRGB:
				return MTLPixelFormatBGRA8Unorm_sRGB;
			case Format::RGB_8_SRGB:
				return MTLPixelFormatRGBA8Unorm_sRGB;
			case Format::BGR_8_SRGB:
				return MTLPixelFormatBGRA8Unorm_sRGB;
			case Format::RGB_8:
			case Format::RGBA_8:
				return MTLPixelFormatRGBA8Unorm;
			case Format::BGRA_8:
				return MTLPixelFormatBGRA8Unorm;
			case Format::RGB_10_A_2:
				return MTLPixelFormatRGB10A2Unorm;
			case Format::B_10_GR_11_UF:
				return MTLPixelFormatRG11B10Float;
			case Format::R_8:
				return MTLPixelFormatR8Unorm;
			case Format::RG_8:
				return MTLPixelFormatRG8Unorm;
			case Format::R_16F:
				return MTLPixelFormatR16Float;
			case Format::RG_16F:
				return MTLPixelFormatRG16Float;
			case Format::RGB_16F:
			case Format::RGBA_16F:
				return MTLPixelFormatRGBA16Float;
			case Format::R_32F:
				return MTLPixelFormatR32Float;
			case Format::RG_32F:
				return MTLPixelFormatRG32Float;
			case Format::RGB_32F:
			case Format::RGBA_32F:
				return MTLPixelFormatRGBA32Float;
			case Format::Depth_32F:
				return MTLPixelFormatDepth32Float;
			case Format::Stencil_8:
				return MTLPixelFormatStencil8;
			case Format::Depth_16I:
				return MTLPixelFormatDepth16Unorm;
			case Format::Depth_32F_Stencil_8:
				return MTLPixelFormatDepth32Float_Stencil8;
				
#if RN_PLATFORM_MAC_OS
			case Format::Depth_24I:
				return MTLPixelFormatDepth24Unorm_Stencil8;
			case Format::Depth_24_Stencil_8:
				return MTLPixelFormatDepth24Unorm_Stencil8;
				
			case Format::RGBA_BC1_SRGB:
				return MTLPixelFormatBC1_RGBA_sRGB;
			case Format::RGBA_BC2_SRGB:
				return MTLPixelFormatBC2_RGBA_sRGB;
			case Format::RGBA_BC3_SRGB:
				return MTLPixelFormatBC3_RGBA_sRGB;
			case Format::RGBA_BC7_SRGB:
				return MTLPixelFormatBC7_RGBAUnorm_sRGB;
				
			case Format::RGBA_BC1:
				return MTLPixelFormatBC1_RGBA;
			case Format::RGBA_BC2:
				return MTLPixelFormatBC2_RGBA;
			case Format::RGBA_BC3:
				return MTLPixelFormatBC3_RGBA;
			case Format::RGBA_BC4:
				return MTLPixelFormatBC4_RUnorm;
			case Format::RGBA_BC5:
				return MTLPixelFormatBC5_RGUnorm;
			case Format::RGBA_BC7:
				return MTLPixelFormatBC7_RGBAUnorm;
				
			case Format::RGBA_ASTC_4X4_SRGB:
			case Format::RGBA_ASTC_5X4_SRGB:
			case Format::RGBA_ASTC_5X5_SRGB:
			case Format::RGBA_ASTC_6X5_SRGB:
			case Format::RGBA_ASTC_6X6_SRGB:
			case Format::RGBA_ASTC_8X5_SRGB:
			case Format::RGBA_ASTC_8X6_SRGB:
			case Format::RGBA_ASTC_8X8_SRGB:
			case Format::RGBA_ASTC_10X5_SRGB:
			case Format::RGBA_ASTC_10X6_SRGB:
			case Format::RGBA_ASTC_10X8_SRGB:
			case Format::RGBA_ASTC_10X10_SRGB:
			case Format::RGBA_ASTC_12X10_SRGB:
			case Format::RGBA_ASTC_12X12_SRGB:
			case Format::RGBA_ASTC_4X4:
			case Format::RGBA_ASTC_5X4:
			case Format::RGBA_ASTC_5X5:
			case Format::RGBA_ASTC_6X5:
			case Format::RGBA_ASTC_6X6:
			case Format::RGBA_ASTC_8X5:
			case Format::RGBA_ASTC_8X6:
			case Format::RGBA_ASTC_8X8:
			case Format::RGBA_ASTC_10X5:
			case Format::RGBA_ASTC_10X6:
			case Format::RGBA_ASTC_10X8:
			case Format::RGBA_ASTC_10X10:
			case Format::RGBA_ASTC_12X10:
			case Format::RGBA_ASTC_12X12:
				return MTLPixelFormatInvalid;
#else
			case Format::RGBA_BC1_SRGB:
			case Format::RGBA_BC2_SRGB:
			case Format::RGBA_BC3_SRGB:
			case Format::RGBA_BC7_SRGB:
			case Format::RGBA_BC1:
			case Format::RGBA_BC2:
			case Format::RGBA_BC3:
			case Format::RGBA_BC7:
				return MTLPixelFormatInvalid;
				
			case Format::RGBA_ASTC_4X4_SRGB:
				return MTLPixelFormatASTC_4x4_sRGB;
			case Format::RGBA_ASTC_5X4_SRGB:
				return MTLPixelFormatASTC_5x4_sRGB;
			case Format::RGBA_ASTC_5X5_SRGB:
				return MTLPixelFormatASTC_5x5_sRGB;
			case Format::RGBA_ASTC_6X5_SRGB:
				return MTLPixelFormatASTC_6x5_sRGB;
			case Format::RGBA_ASTC_6X6_SRGB:
				return MTLPixelFormatASTC_6x6_sRGB;
			case Format::RGBA_ASTC_8X5_SRGB:
				return MTLPixelFormatASTC_8x5_sRGB;
			case Format::RGBA_ASTC_8X6_SRGB:
				return MTLPixelFormatASTC_8x6_sRGB;
			case Format::RGBA_ASTC_8X8_SRGB:
				return MTLPixelFormatASTC_8x8_sRGB;
			case Format::RGBA_ASTC_10X5_SRGB:
				return MTLPixelFormatASTC_10x5_sRGB;
			case Format::RGBA_ASTC_10X6_SRGB:
				return MTLPixelFormatASTC_10x6_sRGB;
			case Format::RGBA_ASTC_10X8_SRGB:
				return MTLPixelFormatASTC_10x8_sRGB;
			case Format::RGBA_ASTC_10X10_SRGB:
				return MTLPixelFormatASTC_10x10_sRGB;
			case Format::RGBA_ASTC_12X10_SRGB:
				return MTLPixelFormatASTC_12x10_sRGB;
			case Format::RGBA_ASTC_12X12_SRGB:
				return MTLPixelFormatASTC_12x12_sRGB;
				
			case Format::RGBA_ASTC_4X4:
				return MTLPixelFormatASTC_4x4_LDR;
			case Format::RGBA_ASTC_5X4:
				return MTLPixelFormatASTC_5x4_LDR;
			case Format::RGBA_ASTC_5X5:
				return MTLPixelFormatASTC_5x5_LDR;
			case Format::RGBA_ASTC_6X5:
				return MTLPixelFormatASTC_6x5_LDR;
			case Format::RGBA_ASTC_6X6:
				return MTLPixelFormatASTC_6x6_LDR;
			case Format::RGBA_ASTC_8X5:
				return MTLPixelFormatASTC_8x5_LDR;
			case Format::RGBA_ASTC_8X6:
				return MTLPixelFormatASTC_8x6_LDR;
			case Format::RGBA_ASTC_8X8:
				return MTLPixelFormatASTC_8x8_LDR;
			case Format::RGBA_ASTC_10X5:
				return MTLPixelFormatASTC_10x5_LDR;
			case Format::RGBA_ASTC_10X6:
				return MTLPixelFormatASTC_10x6_LDR;
			case Format::RGBA_ASTC_10X8:
				return MTLPixelFormatASTC_10x8_LDR;
			case Format::RGBA_ASTC_10X10:
				return MTLPixelFormatASTC_10x10_LDR;
			case Format::RGBA_ASTC_12X10:
				return MTLPixelFormatASTC_12x10_LDR;
			case Format::RGBA_ASTC_12X12:
				return MTLPixelFormatASTC_12x12_LDR;
#endif
			
			case Format::Invalid:
				return MTLPixelFormatInvalid;
		}
		
		return MTLPixelFormatRGBA8Unorm_sRGB;
	}
	
	MTLTextureDescriptor *MetalTexture::DescriptorForTextureDescriptor(const Descriptor &descriptor, bool isIOSurfaceBacked)
	{
		MTLTextureDescriptor *metalDescriptor = [[MTLTextureDescriptor alloc] init];
		
		metalDescriptor.width = descriptor.width;
		metalDescriptor.height = descriptor.height;
		metalDescriptor.resourceOptions = MetalRenderer::MetalResourceOptionsFromOptions(descriptor.accessOptions);
		metalDescriptor.mipmapLevelCount = descriptor.mipMaps;
		metalDescriptor.pixelFormat = MetalTexture::PixelFormatForTextureFormat(descriptor.format);
		metalDescriptor.sampleCount = descriptor.sampleCount;
		
		MTLTextureUsage usage = 0;
		
		if(descriptor.usageHint & Texture::UsageHint::ShaderRead)
			usage |= MTLTextureUsageShaderRead;
		if(descriptor.usageHint & Texture::UsageHint::ShaderWrite)
			usage |= MTLTextureUsageShaderWrite;
		if(descriptor.usageHint & Texture::UsageHint::RenderTarget)
		{
			usage |= MTLTextureUsageRenderTarget;
			metalDescriptor.storageMode = descriptor.accessOptions == GPUBuffer::AccessOptions::ReadWrite? MTLStorageModeShared : MTLStorageModePrivate;
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
				metalDescriptor.textureType = descriptor.sampleCount > 1? MTLTextureType2DMultisample : MTLTextureType2D;
				metalDescriptor.depth = descriptor.depth;
				break;
			case Texture::Type::Type2DArray:
				if(@available(iOS 14, macOS 10.14, *)) {
					metalDescriptor.textureType = descriptor.sampleCount > 1? MTLTextureType2DMultisampleArray : MTLTextureType2DArray;
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
