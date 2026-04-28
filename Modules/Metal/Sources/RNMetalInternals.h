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
		MetalDrawable(Renderer *renderer) :
			Drawable(renderer)
		{}

		struct RenderResources
		{
			const MetalRenderingState *pipelineState = nullptr;
			Drawable::PipelineKey pipelineKey;
			Drawable::MergedMaterialSnapshot mergedMaterialSnapshot;

			std::vector<Shader::ArgumentBuffer*> argumentBufferToUniformBufferMapping;
			std::vector<MetalUniformBufferReference*> vertexShaderUniformBuffers;
			std::vector<MetalUniformBufferReference*> fragmentShaderUniformBuffers;
		};

		struct RenderPacket
		{
			std::vector<RenderResources> resources;
		};

		~MetalDrawable()
		{
			for(uint8 packetSlot = 0; packetSlot < RN_RENDERING_PACKET_SLOT_COUNT; packetSlot += 1)
			{
				for(RenderResources &resources : _renderPackets[packetSlot].resources)
				{
					for(MetalUniformBufferReference *buffer : resources.vertexShaderUniformBuffers)
						delete buffer;

					for(MetalUniformBufferReference *buffer : resources.fragmentShaderUniformBuffers)
						delete buffer;

					for(Shader::ArgumentBuffer *buffer : resources.argumentBufferToUniformBufferMapping)
						buffer->Release();
				}
			}
		}

		RenderResources &EnsureUpdateRenderResources(size_t resourceIndex, uint8 packetSlot)
		{
			RN_DEBUG_ASSERT(packetSlot < RN_RENDERING_PACKET_SLOT_COUNT, "Invalid render resources packet slot");
			RenderPacket &packet = _renderPackets[packetSlot];
			if(packet.resources.size() <= resourceIndex)
				packet.resources.resize(resourceIndex + 1);

			return packet.resources[resourceIndex];
		}

		const RenderResources &GetRenderResources(size_t resourceIndex, uint8 packetSlot) const
		{
			RN_DEBUG_ASSERT(packetSlot < RN_RENDERING_PACKET_SLOT_COUNT, "Invalid render resources packet slot");
			const RenderPacket &packet = _renderPackets[packetSlot];
			RN_DEBUG_ASSERT(resourceIndex < packet.resources.size(), "Invalid render resources index");
			return packet.resources[resourceIndex];
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
		RenderPacket _renderPackets[RN_RENDERING_PACKET_SLOT_COUNT];
	};

	struct MetalPointLight
	{
		Vector3 position;
		float range;
		Vector4 color;
	};

	struct MetalSpotLight
	{
		Vector3 position;
		float range;
		Vector3 direction;
		float angle;
		Vector4 color;
	};

	struct MetalDirectionalLight
	{
		Vector3 direction;
		float padding;
		Vector4 color;
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

		Type type;
		RenderPass *renderPass;
		RenderPass *previousRenderPass;
		size_t renderFramePassIndex = RenderFrame::InvalidPassIndex;
		size_t frameStatisticsIndex = static_cast<size_t>(-1);
		MetalFramebuffer *previousStoredFramebuffer;

		MetalFramebuffer *framebuffer;
		Shader::UsageHint shaderHint;
		RenderPassResources *renderPassResources = nullptr;
		MetalFramebuffer *resolveFramebuffer;

		Camera *camera;
		Camera *lightingCamera;
		Vector3 viewPosition;
		Matrix viewMatrix;
		Matrix inverseViewMatrix;
		Matrix projectionMatrix;
		Matrix inverseProjectionMatrix;
		Matrix projectionViewMatrix;
		Matrix inverseProjectionViewMatrix;
		
		Color cameraAmbientColor;
		Vector4 cameraCustomData;
		Color cameraFogColor0;
		Color cameraFogColor1;
		Vector2 cameraClipDistance;
		Vector2 cameraFogDistance;
		int32 cameraTag;
		
		uint8 multiviewLayer;
		Rect frameRect;

		std::vector<uint32> instanceSteps; //Number of draw items that use the same pipeline state and can be rendered with the same draw call.

		std::vector<MetalPointLight> pointLights;
		std::vector<MetalSpotLight> spotLights;
		std::vector<MetalDirectionalLight> directionalLights;

		std::vector<Matrix> directionalShadowMatrices;
		MetalTexture *directionalShadowDepthTexture;
		Vector2 directionalShadowInfo;
	};


	struct MetalRendererInternals
	{
		RenderFrame renderFrame;
		std::vector<MetalRenderPass> renderPasses;
		MetalStateCoordinator stateCoordinator;

		id<MTLDevice> device;
		id<MTLCommandQueue> commandQueue;

		id<MTLCommandBuffer> commandBuffer;
		id<MTLRenderCommandEncoder> commandEncoder;

		std::vector<MetalSwapChain *>swapChains;

		size_t currentRenderPassIndex;
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
