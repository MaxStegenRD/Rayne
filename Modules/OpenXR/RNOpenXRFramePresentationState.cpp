//
//  RNOpenXRFramePresentationState.cpp
//  Rayne-OpenXR
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and corona.
//

#include "RNOpenXRFramePresentationState.h"
#include "RNOpenXRCompositorLayer.h"
#include "RNOpenXRSwapChain.h"
#include "RNOpenXRWindow.h"

namespace RN
{
	RNDefineMeta(OpenXRFramePresentationState, RenderFramePresentationState)

	OpenXRFramePresentationState::OpenXRFramePresentationState(OpenXRWindow *window) :
		_window(window),
		_displayTime(0),
		_isLocalDimmingEnabled(window ? window->_isLocalDimmingEnabled : false)
	{}

	XrCompositionLayerBaseHeader *OpenXRFramePresentationState::LayerState::GetBaseHeader()
	{
		if(type == VRCompositorLayer::TypeProjectionView)
		{
			internals.layerProjection.views = internals.layerProjectionViews;
			if(internals.layerProjection.next) internals.layerProjection.next = &internals.layerSettings;
			return reinterpret_cast<XrCompositionLayerBaseHeader *>(&internals.layerProjection);
		}

		if(type == VRCompositorLayer::TypeQuad)
		{
			if(internals.layerQuad.next) internals.layerQuad.next = &internals.layerSettings;
			return reinterpret_cast<XrCompositionLayerBaseHeader *>(&internals.layerQuad);
		}

		if(type == VRCompositorLayer::TypePassthrough)
			return reinterpret_cast<XrCompositionLayerBaseHeader *>(&internals.layerPassthroughCompFb);

		return nullptr;
	}

	bool OpenXRFramePresentationState::AddLayerSnapshot(OpenXRCompositorLayer *targetLayer, bool includeNonSwapChainLayers)
	{
		if(!targetLayer) return false;
		OpenXRWindow *window = _window.Load();
		if(!window) return false;

		size_t order = 0;
		bool addedTargetLayer = false;
		auto addLayer = [&](OpenXRCompositorLayer *layer) {
			if(!layer) return;

			if(layer == targetLayer || (includeNonSwapChainLayers && !layer->_swapChain))
			{
				AddLayerSnapshot(order, layer);
				if(layer == targetLayer) addedTargetLayer = true;
			}
			order++;
		};

		window->_layersUnderlay->Enumerate<OpenXRCompositorLayer>([&](OpenXRCompositorLayer *layer, size_t index, bool &stop) {
			addLayer(layer);
		});
		addLayer(window->_mainLayer);
		window->_layersOverlay->Enumerate<OpenXRCompositorLayer>([&](OpenXRCompositorLayer *layer, size_t index, bool &stop) {
			addLayer(layer);
		});

		return addedTargetLayer;
	}

	void OpenXRFramePresentationState::AddLayerSnapshot(size_t order, OpenXRCompositorLayer *layer)
	{
		if(!layer) return;

		for(const LayerState &existingLayer : _layers)
		{
			if(existingLayer.layer == layer) return;
		}

		LayerState layerState = {};
		layerState.order = order;
		layerState.layer = layer;
		layerState.type = layer->GetType();
		layerState.swapChain = layer->_swapChain;
		layerState.isActive = layer->_isActive;
		layerState.shouldDisplay = layer->_shouldDisplay;
		layerState.internals = *layer->_internals;

		auto iterator = _layers.begin();
		while(iterator != _layers.end() && iterator->order < order)
		{
			iterator++;
		}

		_layers.insert(iterator, layerState);
	}

	void OpenXRFramePresentationState::GetCompositionLayers(std::vector<XrCompositionLayerBaseHeader *> &layers)
	{
		layers.clear();
		layers.reserve(_layers.size());

		for(LayerState &layer : _layers)
		{
			if(!layer.isActive) continue;
			if(!layer.shouldDisplay) continue;

			XrCompositionLayerBaseHeader *baseHeader = layer.GetBaseHeader();
			if(baseHeader) layers.push_back(baseHeader);
		}
	}

	bool OpenXRFramePresentationState::BeginFrameOnRenderThread()
	{
		OpenXRWindow *window = _window.Load();
		if(!window) return false;

		XrTime displayTime = 0;
		if(!window->BeginRenderFrame(displayTime))
			return false;

		_displayTime = displayTime;
		return true;
	}

	void OpenXRFramePresentationState::EndFrameOnRenderThread()
	{
		OpenXRWindow *window = _window.Load();
		if(!window) return;

		window->EndFrameWithPresentationState(*this);
	}
} // namespace RN
