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
#include "RNMetalTextureInfo.h"

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
		const uint32 blockHeight = MetalTextureInfo::GetBlockHeight(_descriptor.format);
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

	void MetalTexture::GenerateMipMaps()
	{
		_renderer->CreateMipMapForTexture(this);
	}
}
