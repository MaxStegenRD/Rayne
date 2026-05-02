//
//  RNVulkanInternals.cpp
//  Rayne
//
//  Copyright 2016 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNVulkanInternals.h"
#include "RNVulkanRenderer.h"
#include "RNVulkanFramebuffer.h"
#include "../../../Source/Rendering/RNFrameSubmissionPruner.h"

namespace RN
{
	RNDefineMeta(VulkanCommandBuffer, Object)

	bool FrameSubmissionPassUsesRenderPass(const VulkanRenderPass &vulkanRenderPass, RenderPass *renderPass)
	{
		if(vulkanRenderPass.renderPass == renderPass) return true;

		for(const VulkanRenderPass &subpass : vulkanRenderPass.subpasses)
		{
			if(subpass.renderPass == renderPass) return true;
		}

		return false;
	}

	template<class Pruner>
	void FrameSubmissionAddRenderPassDependencies(Pruner &pruner, size_t consumerIndex, VulkanRenderPass &vulkanRenderPass)
	{
		pruner.AddExplicitRenderPassDependencies(consumerIndex, vulkanRenderPass.renderPass);

		for(VulkanRenderPass &subpass : vulkanRenderPass.subpasses)
		{
			pruner.AddExplicitRenderPassDependencies(consumerIndex, subpass.renderPass);
		}
	}

	void VulkanFrameSubmission::AddSwapChain(VulkanSwapChain *swapChain)
	{
		if(!swapChain) return;

		for(VulkanSwapChain *existingSwapChain : swapChains)
		{
			if(existingSwapChain == swapChain) return;
		}

		RenderFramePresentationState *presentationState = swapChain->TakeRenderFramePresentationState(renderFrame.GetFrameID());
		renderFrame.AddPresentationState(presentationState);
		if(swapChain->ShouldRenderBackBuffer())
			swapChains.push_back(swapChain);
	}

	void VulkanFrameSubmission::PruneSkippedRenderPasses()
	{
		FrameSubmissionPruner<VulkanFrameSubmission> pruner(*this);
		pruner.Prune();
	}

	VulkanCommandBuffer::VulkanCommandBuffer(VkDevice device, VkCommandPool pool) : _device(device), _pool(pool)
	{

	}

	VulkanCommandBuffer::~VulkanCommandBuffer()
	{
		vk::FreeCommandBuffers(_device, _pool, 1, &_commandBuffer);
	}

	void VulkanCommandBuffer::Begin()
	{
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		RNVulkanValidate(vk::BeginCommandBuffer(_commandBuffer, &cmdBufInfo));
	}

	void VulkanCommandBuffer::Reset()
	{
		RNVulkanValidate(vk::ResetCommandBuffer(_commandBuffer, 0));
	}

	void VulkanCommandBuffer::End()
	{
		RNVulkanValidate(vk::EndCommandBuffer(_commandBuffer));
	}
}
