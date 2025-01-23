//
//  RNOpenVRMetalSwapChain.h
//  Rayne-OpenVR
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_OPENVRMETALSWAPCHAIN_H_
#define __RAYNE_OPENVRMETALSWAPCHAIN_H_

#include "RNMetalDevice.h"
#include "RNMetalFramebuffer.h"
#include "RNMetalRenderer.h"
#include "RNMetalSwapChain.h"
#include "RNOpenVRSwapChain.h"

#include "RNOpenVR.h"
#include <openvr.h>

namespace RN
{
	class OpenVRMetalSwapChain : public MetalSwapChain, OpenVRSwapChain
	{
	public:
		friend class OpenVRWindow;

		OVRAPI ~OpenVRMetalSwapChain();

		OVRAPI void AcquireBackBuffer() final;
		OVRAPI void Prepare() final;
		OVRAPI void Finalize() final;
		OVRAPI void PresentBackBuffer(id<MTLCommandBuffer> commandBuffer) final;
		OVRAPI void PostPresent(id<MTLCommandBuffer> commandBuffer) final;

		OVRAPI id GetMTLTexture() const final;

		OVRAPI void ResizeOpenVRSwapChain(const Vector2 &size) final;
		OVRAPI Vector2 GetOpenVRSwapChainSize() const final { return GetSize(); };
		OVRAPI Framebuffer *GetOpenVRSwapChainFramebuffer() const final;

		const Window::SwapChainDescriptor &GetOpenVRSwapChainDescriptor() const final { return _descriptor; }

	protected:
		OVRAPI OpenVRMetalSwapChain(const Window::SwapChainDescriptor &descriptor, vr::IVRSystem *system);

	private:
		MetalTexture *_targetTexture;
		IOSurfaceRef _targetSurface;

		RNDeclareMetaAPI(OpenVRMetalSwapChain, OVRAPI)
	};
} // namespace RN


#endif /* __RAYNE_OPENVRMETALSWAPCHAIN_H_ */
