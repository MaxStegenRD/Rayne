//
//  RNMetalSwapChain.cpp
//  Rayne
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNMetalSwapChain.h"
#include "RNMetalInternals.h"
#include "RNMetalFramebuffer.h"
#include "RNMetalRenderer.h"
#include "RNMetalTextureInfo.h"

namespace RN
{
	RNDefineMeta(MetalSwapChain, Object)

#if RN_PLATFORM_MAC_OS
	MetalSwapChain::MetalSwapChain(const Vector2 size, MetalRenderer *renderer, id<MTLDevice> device, Screen *screen, const Window::SwapChainDescriptor &descriptor) : _renderer(renderer), _framebuffer(nullptr), _frameIndex(0), _frameDivider(1), _drawable(nullptr)
	{
		_metalView = [[RNMetalView alloc] initWithFrame:NSMakeRect(0, 0, size.x, size.y) device:device screen:screen andFormat:MetalTextureInfo::GetPixelFormat(descriptor.colorFormat)];
		CGSize realSize = [_metalView getSize];
		_size = Vector2(realSize.width, realSize.height);
		_descriptor = descriptor;

		_framebuffer = CreateSwapChainFramebuffer();
	}
#endif

#if RN_PLATFORM_IOS
	MetalSwapChain::MetalSwapChain(const Vector2 size, MetalRenderer *renderer, RNMetalLayerContainer *metalLayerContainer, const Window::SwapChainDescriptor &descriptor) : _renderer(renderer), _framebuffer(nullptr), _frameIndex(0), _frameDivider(1), _drawable(nullptr)
	{
		_metalLayerContainer = metalLayerContainer;
		_size = size;
		_descriptor = descriptor;

		_framebuffer = CreateSwapChainFramebuffer();
	}
#endif

	MetalSwapChain::~MetalSwapChain()
	{

	}

	Vector2 MetalSwapChain::GetSize() const
	{
		return _size;
	}
	
	void MetalSwapChain::SetFrameDivider(uint8 divider)
	{
		_frameDivider = divider;
	}

	void MetalSwapChain::AcquireBackBuffer()
	{
		_frameIndex += 1;
		
		if(_frameDivider && _frameIndex%_frameDivider == 0)
		{
#if RN_PLATFORM_MAC_OS
			_drawable = [_metalView nextDrawable];
#elif RN_PLATFORM_IOS
			_drawable = _metalLayerContainer->GetNextDrawable();
#endif
		}
		else
		{
			_drawable = nullptr;
		}
	}
	void MetalSwapChain::Prepare()
	{

	}
	void MetalSwapChain::Finalize()
	{

	}
	void MetalSwapChain::PresentBackBuffer(id<MTLCommandBuffer> commandBuffer)
	{
		if(_drawable)
			[commandBuffer presentDrawable:_drawable];
	}
	
	void MetalSwapChain::PostPresent(id<MTLCommandBuffer> commandBuffer)
	{
		
	}

	Framebuffer *MetalSwapChain::CreateSwapChainFramebuffer()
	{
		return new MetalFramebuffer(_size, this, _descriptor.colorFormat, _descriptor.depthStencilFormat);
	}

	id MetalSwapChain::GetMetalColorTexture() const
	{
		if(_drawable)
			return [_drawable texture];
		else
			return nil;
	}

	id MetalSwapChain::GetMetalDepthTexture() const
	{
		return nil;
	}
}
