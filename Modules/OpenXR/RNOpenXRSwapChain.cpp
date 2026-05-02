//
//  RNOpenXRSwapChain.cpp
//  Rayne-OpenXR
//
//  Copyright 2021 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and corona.
//

#include "RNOpenXRSwapChain.h"
#include "RNOpenXRInternals.h"

namespace RN
{
	OpenXRSwapChain::OpenXRSwapChain(OpenXRWindow *window, OpenXRCompositorLayer *layer, SwapChainType type) :
		_internals(new OpenXRSwapchainInternals()), _xrWindow(window), _layer(layer), _acquiredImageCountForRenderFrame(0), _swapChainType(type)
	{
		_internals->currentFoveationProfile = XR_NULL_HANDLE;
	}

	OpenXRSwapChain::~OpenXRSwapChain()
	{
		delete _internals;
	}
} // namespace RN
