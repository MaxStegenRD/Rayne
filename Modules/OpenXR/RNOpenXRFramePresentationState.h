//
//  RNOpenXRFramePresentationState.h
//  Rayne-OpenXR
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and corona.
//

#ifndef __RAYNE_OPENXRFRAMEPRESENTATIONSTATE_H_
#define __RAYNE_OPENXRFRAMEPRESENTATIONSTATE_H_

#include "RNOpenXR.h"
#include "RNOpenXRInternals.h"
#include "RNVRCompositorLayer.h"

namespace RN
{
	class OpenXRCompositorLayer;
	class OpenXRWindow;

	class OpenXRFramePresentationState : public RenderFramePresentationState
	{
	public:
		OpenXRFramePresentationState(OpenXRWindow *window, int64 displayTime, bool shouldRender, bool hasFrameState);

		bool AddLayerSnapshot(OpenXRCompositorLayer *targetLayer, bool includeNonSwapChainLayers);
		void GetCompositionLayers(std::vector<XrCompositionLayerBaseHeader *> &layers);
#if XR_USE_GRAPHICS_API_VULKAN
		const std::vector<VkTilePropertiesQCOM> *GetTilePropertiesHint() const;
#endif
		int64 GetDisplayTime() const { return _displayTime; }
		bool GetLocalDimmingEnabled() const { return _isLocalDimmingEnabled; }
		bool BeginFrameOnRenderThread() final;
		void EndFrameOnRenderThread() final;
		void CancelFrameOnRenderThread() final;

	private:
		void AddLayerSnapshot(size_t order, OpenXRCompositorLayer *layer);

		struct LayerState
		{
			XrCompositionLayerBaseHeader *GetBaseHeader();

			size_t order;
			StrongRef<OpenXRCompositorLayer> layer;
			VRCompositorLayer::Type type;
			bool isActive;
			bool shouldDisplay;
			OpenXRCompositorLayerInternals internals;
		};

		WeakRef<OpenXRWindow> _window;
		int64 _displayTime;
		bool _isLocalDimmingEnabled;
		bool _shouldRender;
		bool _hasFrameState;
		StrongRef<Framebuffer> _tilePropertiesFramebuffer;
		std::vector<LayerState> _layers;

		__RNDeclareMetaInternal(OpenXRFramePresentationState)
	};
} // namespace RN

#endif /* __RAYNE_OPENXRFRAMEPRESENTATIONSTATE_H_ */
