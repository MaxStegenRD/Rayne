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
	class OpenXRSwapChain;
	class OpenXRWindow;

	class OpenXRFramePresentationState : public RenderFramePresentationState
	{
	public:
		OpenXRFramePresentationState(OpenXRWindow *window);

		bool AddLayerSnapshot(OpenXRCompositorLayer *targetLayer, bool includeNonSwapChainLayers);
		void GetCompositionLayers(std::vector<XrCompositionLayerBaseHeader *> &layers);
		XrTime GetDisplayTime() const { return _displayTime; }
		bool GetLocalDimmingEnabled() const { return _isLocalDimmingEnabled; }
		void EndFrameOnRenderThread() final;

	private:
		void AddLayerSnapshot(size_t order, OpenXRCompositorLayer *layer);

		struct LayerState
		{
			XrCompositionLayerBaseHeader *GetBaseHeader();

			size_t order;
			OpenXRCompositorLayer *layer;
			VRCompositorLayer::Type type;
			OpenXRSwapChain *swapChain;
			bool isActive;
			bool shouldDisplay;
			OpenXRCompositorLayerInternals internals;
		};

		WeakRef<OpenXRWindow> _window;
		XrTime _displayTime;
		bool _isLocalDimmingEnabled;
		std::vector<LayerState> _layers;

		__RNDeclareMetaInternal(OpenXRFramePresentationState)
	};
} // namespace RN

#endif /* __RAYNE_OPENXRFRAMEPRESENTATIONSTATE_H_ */
