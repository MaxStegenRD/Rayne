//
//  RNMetalInternals.h
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_METALINTERNALS_H__
#define __RAYNE_METALINTERNALS_H__

#import <Metal/Metal.h>

#include "RNMetal.h"
#include "RNMetalStateCoordinator.h"
#include "RNMetalUniformBuffer.h"
#include "RNMetalFramebuffer.h"
#include "RNMetalRenderer.h"
#include "../../../Source/Rendering/RNFrameSubmissionQueue.h"

#if RN_PLATFORM_MAC_OS
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#endif

#if RN_PLATFORM_IOS
#import <UIKit/UIKit.h>
#endif

#if RN_PLATFORM_MAC_OS
@interface RNMetalView : NSView
- (id<CAMetalDrawable>)nextDrawable;
- (instancetype)initWithFrame:(NSRect)frameRect device:(id<MTLDevice>)device screen:(RN::Screen*)screen andFormat:(MTLPixelFormat)format;
- (CGSize)getSize;
@end

@interface RNMetalWindow : NSWindow
@end
#endif

#if RN_PLATFORM_IOS
class RNMetalLayerContainer
{
public:
	RNMetalLayerContainer(CAMetalLayer *metalLayer);
	id<CAMetalDrawable> GetNextDrawable();
	RN::Vector2 GetSize();

private:
	CAMetalLayer *_metalLayer;
};
#endif

namespace RN
{
	class MetalWindow;
	class Framebuffer;
	class Camera;
	class MetalGPUBuffer;
	class MetalTexture;

	struct MetalDrawable : public Drawable
	{
		MetalDrawable() = default;

		struct RenderResources
		{
			const MetalRenderingState *pipelineState = nullptr;
			Drawable::PipelineKey pipelineKey;
			Drawable::MergedMaterialSnapshot mergedMaterialSnapshot;

			std::vector<Shader::ArgumentBuffer*> argumentBufferToUniformBufferMapping;
			std::vector<MetalUniformBufferReference*> vertexShaderUniformBuffers;
			std::vector<MetalUniformBufferReference*> fragmentShaderUniformBuffers;
		};

		~MetalDrawable()
		{
			for(RenderResources &resources : _renderResources)
			{
				for(MetalUniformBufferReference *buffer : resources.vertexShaderUniformBuffers)
					delete buffer;

				for(MetalUniformBufferReference *buffer : resources.fragmentShaderUniformBuffers)
					delete buffer;

				for(Shader::ArgumentBuffer *buffer : resources.argumentBufferToUniformBufferMapping)
					buffer->Release();
			}
		}

		RenderResources &EnsureRenderResources(size_t resourceIndex)
		{
			if(_renderResources.size() <= resourceIndex)
				_renderResources.resize(resourceIndex + 1);

			return _renderResources[resourceIndex];
		}

		const RenderResources &GetRenderResources(size_t resourceIndex) const
		{
			RN_DEBUG_ASSERT(resourceIndex < _renderResources.size(), "Invalid render resources index");
			return _renderResources[resourceIndex];
		}

		RenderResources &GetRenderResources(size_t resourceIndex)
		{
			RN_DEBUG_ASSERT(resourceIndex < _renderResources.size(), "Invalid render resources index");
			return _renderResources[resourceIndex];
		}

		void UpdateRenderingState(RenderResources &resources, Renderer *renderer, const MetalRenderingState *state, const Drawable::PipelineKey &pipelineKey)
		{
			resources.pipelineState = state;
			resources.pipelineKey = pipelineKey;

			for(Shader::ArgumentBuffer *buffer : resources.argumentBufferToUniformBufferMapping)
				buffer->Release();
			resources.argumentBufferToUniformBufferMapping.clear();

			for(MetalUniformBufferReference *buffer : resources.vertexShaderUniformBuffers)
				buffer->Release();
			resources.vertexShaderUniformBuffers.clear();

			for(MetalUniformBufferReference *buffer : resources.fragmentShaderUniformBuffers)
				buffer->Release();
			resources.fragmentShaderUniformBuffers.clear();

			MetalRenderer *metalRenderer = renderer->Downcast<MetalRenderer>();

			const Shader::Signature *vertexShaderSignature = state->vertexShader->GetSignature();
			vertexShaderSignature->GetBuffers()->Enumerate<Shader::ArgumentBuffer>([&](Shader::ArgumentBuffer *buffer, size_t index, bool &stop){
				size_t totalSize = buffer->GetTotalUniformSize();
				if(totalSize > 0)
				{
					resources.argumentBufferToUniformBufferMapping.push_back(buffer->Retain());
					resources.vertexShaderUniformBuffers.push_back(metalRenderer->GetUniformBufferReference(totalSize, buffer->GetIndex())->Retain());
				}
			});

			const Shader::Signature *fragmentShaderSignature = state->fragmentShader->GetSignature();
			fragmentShaderSignature->GetBuffers()->Enumerate<Shader::ArgumentBuffer>([&](Shader::ArgumentBuffer *buffer, size_t index, bool &stop){
				size_t totalSize = buffer->GetTotalUniformSize();
				if(totalSize > 0)
				{
					resources.argumentBufferToUniformBufferMapping.push_back(buffer->Retain());
					resources.fragmentShaderUniformBuffers.push_back(metalRenderer->GetUniformBufferReference(totalSize, buffer->GetIndex())->Retain());
				}
			});
		}

	private:
		std::vector<RenderResources> _renderResources;
	};

	struct MetalRenderPass
	{
		enum Type
		{
			Default,
			ResolveMSAA,
			Blit,
			Convert
		};

		bool UsesDrawItems() const { return type == Type::Default || type == Type::Convert; }

		Type type;
		RenderPass *renderPass;
		RenderPass *previousRenderPass;
		size_t renderFramePassIndex = RenderFrame::InvalidPassIndex;
		size_t preparedRenderPassIndex = RenderFrame::InvalidPassIndex;
		size_t frameStatisticsIndex = static_cast<size_t>(-1);
		MetalFramebuffer *previousStoredFramebuffer;

		MetalFramebuffer *framebuffer;
		Shader::UsageHint shaderHint;
		MetalFramebuffer *resolveFramebuffer;

		uint8 multiviewLayer;
	};

	struct MetalPreparedDrawItem
	{
		const RenderFrame::DrawItem *drawItem = nullptr;
		const MetalDrawable::RenderResources *renderResources = nullptr;
	};

	struct MetalPreparedRenderPass
	{
		std::vector<MetalPreparedDrawItem> drawItems;
		std::vector<uint32> instanceSteps; //Number of draw items that use the same pipeline state and can be rendered with the same draw call.
	};

	struct MetalFrameSubmission
	{
		void AddSwapChain(MetalSwapChain *swapChain)
		{
			if(!swapChain) return;

			for(MetalSwapChain *existingSwapChain : swapChains)
			{
				if(existingSwapChain == swapChain) return;
			}

			swapChains.push_back(swapChain);
		}

		RenderFrame renderFrame;
		std::vector<MetalRenderPass> renderPasses;
		std::vector<MetalPreparedRenderPass> preparedRenderPasses;
		std::vector<MetalSwapChain *> swapChains;
		size_t activeRenderPassIndex = 0;
	};

	struct MetalRendererInternals
	{
		FrameSubmissionQueue<MetalFrameSubmission> frameSubmissionQueue;
		Thread *renderThread = nullptr;
#if RN_BUILD_DEBUG
		std::thread::id submissionThread;
		bool hasSubmissionThread = false;
#endif
		MetalStateCoordinator stateCoordinator;

		id<MTLDevice> device;
		id<MTLCommandQueue> commandQueue;

		id<MTLCommandBuffer> commandBuffer;
		id<MTLRenderCommandEncoder> commandEncoder;

		const MetalRenderingState *currentRenderState;
	};

	struct MetalWindowInternals
	{
#if RN_PLATFORM_MAC_OS
		NSWindow *window;
#endif
#if RN_PLATFORM_IOS
		RNMetalLayerContainer *metalLayerContainer;
#endif
	};
}

#endif /* __RAYNE_METALINTERNALS_H__ */
