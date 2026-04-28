//
//  VulkanStateCoordinator.h
//  Rayne
//
//  Copyright 2016 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_VULKANSTATECOORDINATOR_H_
#define __RAYNE_VULKANSTATECOORDINATOR_H_

#include "RNVulkan.h"

namespace RN
{
	RNExceptionType(VulkanStructArgumentUnsupported)

	class VulkanFramebuffer;
	class VulkanDynamicBufferReference;
	class VulkanShader;
	class VulkanDevice;
	struct VulkanRenderPass;

	struct VulkanUniformState
	{
		std::vector<Shader::ArgumentBuffer*> constantBufferToArgumentMapping;
		VulkanDynamicBufferReference *instanceAttributesBuffer;
		Shader::ArgumentBuffer *instanceAttributesArgumentBuffer;
		std::vector<VulkanDynamicBufferReference *> vertexConstantBuffers;
		std::vector<VulkanDynamicBufferReference *> fragmentConstantBuffers;

		VkDescriptorSet descriptorSet;

		VulkanUniformState();
		~VulkanUniformState();
	};

	struct VulkanDepthStencilState
	{
		VulkanDepthStencilState() = default;

		~VulkanDepthStencilState()
		{

		}

		DepthMode mode;
		bool depthWriteEnabled;
	};

	struct VulkanRootSignature
	{
		~VulkanRootSignature();

		std::vector<uint16> bindingIndex;
		std::vector<uint8> bindingType;
		Array *samplers;

		uint8 textureCount;
		uint8 subpassInputCount;
		uint8 constantBufferCount;

		VkDescriptorSetLayout descriptorSetLayout;
		VkPipelineLayout pipelineLayout;
	};

	struct VulkanPipelineStateDescriptor
	{
		uint8 sampleCount;
		//uint8 sampleQuality;
		VkRenderPass renderPass;
		uint32 subpassIndex;
		uint8 colorAttachmentCount;
		VkFormat depthStencilFormat;
		
		uint8 colorWriteMask;
		DepthMode depthMode;
		bool depthWriteEnabled;

		Shader *vertexShader;
		Shader *fragmentShader;

		CullMode cullMode;
		bool usePolygonOffset;
		float polygonOffsetFactor;
		float polygonOffsetUnits;

		bool useAlphaToCoverage;

		BlendOperation blendOperationRGB;
		BlendOperation blendOperationAlpha;
		BlendFactor blendFactorSourceRGB;
		BlendFactor blendFactorSourceAlpha;
		BlendFactor blendFactorDestinationRGB;
		BlendFactor blendFactorDestinationAlpha;
	};

	struct VulkanPipelineState
	{
		~VulkanPipelineState();

		VulkanPipelineStateDescriptor descriptor;
		const VulkanRootSignature *rootSignature;

        uint8 vertexAttributeBufferCount; //This should maybe go into the descriptor and be checked cause pipelines could differ with this now!?

		VkPipeline state;
	};

	struct VulkanPipelineStateCollection
	{
		VulkanPipelineStateCollection() = default;
		VulkanPipelineStateCollection(const Mesh::VertexDescriptor &tdescriptor, Shader *vertex, Shader *fragment) :
			descriptor(tdescriptor),
			vertexShader(vertex),
			fragmentShader(fragment)
		{}

		~VulkanPipelineStateCollection()
		{
			for(VulkanPipelineState *state : states)
				delete state;
		}

		Mesh::VertexDescriptor descriptor;
		Shader *vertexShader;
		Shader *fragmentShader;

		std::vector<VulkanPipelineState *> states;
	};

	struct VulkanRenderPassState
	{
		RenderPass::Flags flags;
		uint8 multiviewCount;
		bool hasFragmentDensityMap;
		uint64 subpassSignature; // compact hash of per-subpass read/write masks
		std::vector<VkFormat> imageFormats;
		std::vector<VkFormat> resolveFormats;
		VkRenderPass renderPass;
		uint8 imageSampleCount;
		uint8 resolveSampleCount;

		RN_INLINE bool operator==(const VulkanRenderPassState &descriptor) const
		{
			if(imageFormats.size() != descriptor.imageFormats.size()) return false;
			if(resolveFormats.size() != descriptor.resolveFormats.size()) return false;
			if(flags != descriptor.flags) return false;
			if(multiviewCount != descriptor.multiviewCount) return false;
			if(subpassSignature != descriptor.subpassSignature) return false;
			if(imageSampleCount != descriptor.imageSampleCount) return false;
			if(resolveSampleCount != descriptor.resolveSampleCount) return false;

			for(int i = 0; i < imageFormats.size(); i++)
			{
				if(imageFormats[i] != descriptor.imageFormats[i]) return false;
			}

			for(int i = 0; i < resolveFormats.size(); i++)
			{
				if(resolveFormats[i] != descriptor.resolveFormats[i]) return false;
			}

			return true;
		}
	};

	class VulkanStateCoordinator
	{
	public:
		VulkanStateCoordinator();
		~VulkanStateCoordinator();

		const VulkanRootSignature *GetRootSignature(const VulkanPipelineStateDescriptor &pipelineDescriptor);
		const VulkanPipelineState *GetRenderPipelineState(Shader *vertexShader, Shader *fragmentShader, Mesh *mesh, const Material::PipelineProperties &mergedMaterialProperties, const VulkanRenderPass *rootVulkanPass, uint32 subpassIndex);
		VulkanUniformState *GetUniformStateForPipelineState(const VulkanPipelineState *pipelineState);
		VulkanRenderPassState *GetRenderPassState(const VulkanRenderPass *rootVulkanPass);

		void LoadPipelineCache(uint64 buildNumber, VulkanDevice *device, VkAllocationCallbacks *allocatorCallbacks);
		void SavePipelineCache(uint64 buildNumber, VulkanDevice *device);
		void DestroyPipelineCache(VulkanDevice *device, VkAllocationCallbacks *allocatorCallbacks);

	private:
		std::vector<VkVertexInputAttributeDescription> CreateVertexElementDescriptorsFromMesh(Mesh *mesh, VulkanShader *vertexShader, bool &vertexPositionsOnly);
		const VulkanPipelineState *GetRenderPipelineStateInCollection(VulkanPipelineStateCollection *collection, Mesh *mesh, const VulkanPipelineStateDescriptor &pipelineDescriptor);

		std::vector<VulkanDepthStencilState *> _depthStencilStates;
		const VulkanDepthStencilState *_lastDepthStencilState;

		std::vector<VulkanRenderPassState*> _renderPassStates;
		std::vector<VulkanPipelineStateCollection *> _renderingStates;
		std::vector<VulkanRootSignature *> _rootSignatures;

		VkPipelineCache _pipelineCache;
		bool _pipelineCacheNeedsSaving;
	};
}


#endif /* __RAYNE_VULKANSTATECOORDINATOR_H_ */
