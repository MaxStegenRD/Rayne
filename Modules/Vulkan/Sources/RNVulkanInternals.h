//
//  RNVulkanInternals.h
//  Rayne
//
//  Copyright 2016 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_VULKANINTERNALS_H__
#define __RAYNE_VULKANINTERNALS_H__

#include "RNVulkan.h"
#include "RNVulkanStateCoordinator.h"
#include "RNVulkanRenderer.h"
#include "RNVulkanSwapChain.h"
#include "../../../Source/Scene/RNLightManager.h"

#include <vk_mem_alloc.h>

namespace RN
{
	//Descriptor sets are allocated from per-frame Vulkan descriptor pools and reclaimed via pool reset on frame completion.
	class VulkanDescriptorPool
	{
		public:
			VulkanDescriptorPool() : _initialized(false), _renderer(nullptr), _activeFramePoolIndex(0)
			{
				
			}

			void Init(VulkanRenderer *renderer)
			{
				_renderer = renderer;
				//Create descriptor pool
				VkDescriptorPoolSize uniformBufferPoolSize = {};
				uniformBufferPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				uniformBufferPoolSize.descriptorCount = 20000;
				VkDescriptorPoolSize storageBufferPoolSize = {};
				storageBufferPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				storageBufferPoolSize.descriptorCount = 20000;
				VkDescriptorPoolSize textureBufferPoolSize = {};
				textureBufferPoolSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				textureBufferPoolSize.descriptorCount = 10000;
				VkDescriptorPoolSize inputAttachmentPoolSize = {};
				inputAttachmentPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				inputAttachmentPoolSize.descriptorCount = 5000;
				VkDescriptorPoolSize samplerBufferPoolSize = {};
				samplerBufferPoolSize.type = VK_DESCRIPTOR_TYPE_SAMPLER;
				samplerBufferPoolSize.descriptorCount = 10000;
				_poolSizes = { uniformBufferPoolSize, storageBufferPoolSize, samplerBufferPoolSize, textureBufferPoolSize, inputAttachmentPoolSize };
				_maxSets = 50000;
				_framePools.clear();
				EnsureFramePool(_renderer, 0);
				_initialized = true;
			}

			~VulkanDescriptorPool()
			{
				if(!_initialized) return;

				RN_ASSERT(_renderer, "VulkanDescriptorPool::Init must be called before use!");
				for(VkDescriptorPool descriptorPool : _framePools)
				{
					if(descriptorPool == VK_NULL_HANDLE) continue;
					vk::DestroyDescriptorPool(_renderer->GetVulkanDevice()->GetDevice(), descriptorPool, _renderer->GetAllocatorCallback());
				}
			}

			void SetActiveFramePool(VulkanRenderer *renderer, size_t framePoolIndex)
			{
				EnsureFramePool(renderer, framePoolIndex);
				_activeFramePoolIndex = framePoolIndex;
			}

			void ResetFramePool(VulkanRenderer *renderer, size_t framePoolIndex)
			{
				RN_ASSERT(renderer, "VulkanDescriptorPool::ResetFramePool requires a valid renderer!");
				if(framePoolIndex >= _framePools.size()) return;
				if(_framePools[framePoolIndex] == VK_NULL_HANDLE) return;

				RNVulkanValidate(vk::ResetDescriptorPool(renderer->GetVulkanDevice()->GetDevice(), _framePools[framePoolIndex], 0));
			}

			VkDescriptorSet Allocate(VulkanRenderer *renderer, VkDescriptorSetLayout layout)
			{
				RN_ASSERT(renderer, "VulkanDescriptorPool::Allocate requires a valid renderer!");
				RN_ASSERT(_activeFramePoolIndex < _framePools.size() && _framePools[_activeFramePoolIndex] != VK_NULL_HANDLE, "VulkanDescriptorPool::Allocate called without an active frame pool!");

				VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
				descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				descriptorSetAllocateInfo.pNext = NULL;
				descriptorSetAllocateInfo.descriptorPool = _framePools[_activeFramePoolIndex];
				descriptorSetAllocateInfo.pSetLayouts = &layout;
				descriptorSetAllocateInfo.descriptorSetCount = 1;

				VkDescriptorSet descriptorSet;
				RNVulkanValidate(vk::AllocateDescriptorSets(renderer->GetVulkanDevice()->GetDevice(), &descriptorSetAllocateInfo, &descriptorSet));
				return descriptorSet;
			}

		private:
			void EnsureFramePool(VulkanRenderer *renderer, size_t framePoolIndex)
			{
				RN_ASSERT(renderer, "VulkanDescriptorPool::EnsureFramePool requires a valid renderer!");
				if(_framePools.size() <= framePoolIndex)
				{
					_framePools.resize(framePoolIndex + 1, VK_NULL_HANDLE);
				}

				if(_framePools[framePoolIndex] != VK_NULL_HANDLE) return;

				VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
				descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
				descriptorPoolInfo.pNext = NULL;
				descriptorPoolInfo.poolSizeCount = _poolSizes.size();
				descriptorPoolInfo.pPoolSizes = _poolSizes.data();
				descriptorPoolInfo.maxSets = _maxSets;
				descriptorPoolInfo.flags = 0;
				RNVulkanValidate(vk::CreateDescriptorPool(renderer->GetVulkanDevice()->GetDevice(), &descriptorPoolInfo, renderer->GetAllocatorCallback(), &_framePools[framePoolIndex]));
			}

			bool _initialized;
			VulkanRenderer *_renderer;
			std::vector<VkDescriptorPoolSize> _poolSizes;
			uint32 _maxSets;
			std::vector<VkDescriptorPool> _framePools;
			size_t _activeFramePoolIndex;
	};

	struct VulkanDrawable : public Drawable
	{
		VulkanDrawable() = default;

		struct RenderResources
		{
			const VulkanPipelineState *pipelineState = nullptr; //No need for cleanup here as it's shared, but maybe add reference counting to clear later.
			VulkanUniformState *uniformState = nullptr;
			VulkanTransientDescriptorSet *descriptorSet = nullptr;
			Drawable::PipelineKey pipelineKey;
			Drawable::MergedMaterialSnapshot mergedMaterialSnapshot;
		};

		~VulkanDrawable()
		{
			VulkanRenderer *renderer = Renderer::GetActiveRenderer()->Downcast<VulkanRenderer>();
			for(RenderResources &resources : _renderResources)
			{
				VulkanTransientDescriptorSet *descriptorSet = resources.descriptorSet;
				VulkanUniformState *uniformState = resources.uniformState;

				renderer->AddFrameFinishedCallback([descriptorSet, uniformState](){
					if(descriptorSet)
					{
						delete descriptorSet;
					}
					if(uniformState)
					{
						delete uniformState;
					}
				});
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

		void UpdateRenderingState(RenderResources &resources, const VulkanPipelineState *pipelineState, VulkanUniformState *uniformState, const Drawable::PipelineKey &pipelineKey)
		{
			resources.pipelineState = pipelineState;
			resources.pipelineKey = pipelineKey;
			VulkanUniformState *oldUniformState = resources.uniformState;
			if(oldUniformState)
			{
				VulkanRenderer *renderer = Renderer::GetActiveRenderer()->Downcast<VulkanRenderer>();
				renderer->AddFrameFinishedCallback([oldUniformState](){
					delete oldUniformState;
				});
			}
			resources.uniformState = uniformState;
		}

	private:
		//TODO: This can get somewhat big with lots of post processing stages...
		std::vector<RenderResources> _renderResources;
	};

	struct VulkanRenderPass
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
		size_t preparedRenderPassIndex = RenderFrame::InvalidPassIndex;
		size_t frameStatisticsIndex = static_cast<size_t>(-1);
		VulkanFramebuffer *previousStoredFramebuffer;

		std::vector<VulkanRenderPass> subpasses;
		uint64 subpassSignature;

		VulkanFramebuffer *framebuffer;
		VulkanFramebuffer *resolveFramebuffer;
		Shader::UsageHint shaderHint;

		LightManager *lightManager = nullptr;
		uint8 multiviewLayer;

		std::vector<VulkanTexture *> renderTargetsUsedInShader;

	};

	struct VulkanPreparedDrawItem
	{
		const RenderFrame::DrawItem *drawItem = nullptr;
		const VulkanDrawable::RenderResources *renderResources = nullptr;
	};

	struct VulkanPreparedRenderPass
	{
		std::vector<VulkanPreparedDrawItem> drawItems;
		std::vector<uint32> instanceSteps; //Number of draw items that use the same pipeline state and can be rendered with the same draw call.
		size_t resourceIndex = 0;
	};

	struct VulkanFrameResource
	{
		size_t frame;
		std::function<void()> finishedCallback;
	};

	class VulkanCommandBuffer : public Object
	{
	public:
		friend RN::VulkanRenderer;

		~VulkanCommandBuffer();

		void Begin();
		void Reset();
		void End();

		VkCommandBuffer GetCommandBuffer() const {return _commandBuffer;}

	private:
		VulkanCommandBuffer(VkDevice device, VkCommandPool pool);

		VkCommandBuffer _commandBuffer;
		VkDevice _device;
		VkCommandPool _pool;
		uint32 _frameValue;

		RNDeclareMetaAPI(VulkanCommandBuffer, VKAPI)
	};

	struct VulkanDrawBindStateCache
	{
		VkPipeline pipeline;
		VkPipelineLayout pipelineLayout;
		VkDescriptorSet descriptorSet;
		uint8 vertexBufferCount;
		VkBuffer vertexBuffers[3];
		VkDeviceSize vertexOffsets[3];
		bool hasIndexBufferBinding;
		VkBuffer indexBuffer;
		VkDeviceSize indexOffset;
		VkIndexType indexType;

		VulkanDrawBindStateCache() :
			pipeline(VK_NULL_HANDLE),
			pipelineLayout(VK_NULL_HANDLE),
			descriptorSet(VK_NULL_HANDLE),
			vertexBufferCount(0),
			hasIndexBufferBinding(false),
			indexBuffer(VK_NULL_HANDLE),
			indexOffset(0),
			indexType(VK_INDEX_TYPE_UINT16)
		{
			for(uint8 i = 0; i < 3; i++)
			{
				vertexBuffers[i] = VK_NULL_HANDLE;
				vertexOffsets[i] = 0;
			}
		}
	};

	struct VulkanRendererInternals
	{
		RenderFrame renderFrame;
		std::vector<VulkanRenderPass> renderPasses;
		std::vector<VulkanPreparedRenderPass> preparedRenderPasses;
		VulkanStateCoordinator stateCoordinator;

		std::vector<VulkanSwapChain*> swapChains;
		std::vector<VulkanFrameResource> frameResources;

		size_t currentRenderPassIndex;
		size_t totalDrawableCount;

		size_t totalDescriptorTables;
		VulkanDrawBindStateCache drawBindStateCache;

		uint32 currentSubpassIndex; //TODO: Remove this

		VmaAllocator memoryAllocator;
		VulkanDescriptorPool descriptorPool;
		
		// Tracy Vulkan GPU context
		RN_PROFILE_VULKAN_CONTEXT_TYPE tracyVulkanCtx;
		VulkanCommandBuffer *tracyCommandBuffer;
		VkCommandBuffer tracyVulkanCommandBuffer;
	};

	class VulkanTransientDescriptorSet
	{
		public:
			VulkanTransientDescriptorSet() : _layout(VK_NULL_HANDLE), _activeDescriptorSet(VK_NULL_HANDLE)
			{

			}

			~VulkanTransientDescriptorSet()
			{
			}

			void SetLayout(VkDescriptorSetLayout layout)
			{
				_layout = layout;
			}

			void Allocate(VulkanRenderer *renderer)
			{
				_activeDescriptorSet = renderer->_internals->descriptorPool.Allocate(renderer, _layout);
			}

			VkDescriptorSet GetActiveDescriptorSet()
			{
				return _activeDescriptorSet;
			}

		private:
			VkDescriptorSetLayout _layout;
			VkDescriptorSet _activeDescriptorSet;
	};

	//Based on https://zeux.io/2019/07/17/serializing-pipeline-cache/
	struct VulkanPipelineCachePrefixHeader
	{
		uint32 magic; //Always 8372610
		uint32 dataSize; //Equal to *pDataSize returned by vkGetPipelineCacheData
		uint64 dataHash; //A hash of pipeline cache data, including the header

		uint64 buildNumber;

		uint32 vendorID; //Equal to VkPhysicalDeviceProperties::vendorID
		uint32 deviceID; //Equal to VkPhysicalDeviceProperties::deviceID
		uint32 driverVersion; //Equal to VkPhysicalDeviceProperties::driverVersion
		uint32 driverABI; //Equal to sizeof(void*)

		uint8 uuid[VK_UUID_SIZE]; //Equal to VkPhysicalDeviceProperties::pipelineCacheUUID
	};
}

#endif /* __RAYNE_VULKANINTERNALS_H__ */
