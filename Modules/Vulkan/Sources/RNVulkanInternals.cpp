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

namespace RN
{
	RNDefineMeta(VulkanCommandBuffer, Object)

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

	void VulkanFrameSubmission::RemoveUnsubmittedSwapChainRenderPasses()
	{
		auto usesSubmittedSwapChain = [this](VulkanSwapChain *swapChain) {
			if(!swapChain) return true;

			for(VulkanSwapChain *submittedSwapChain : swapChains)
			{
				if(submittedSwapChain == swapChain)
					return true;
			}

			return false;
		};

		auto usesSubmittedFramebufferSwapChain = [&usesSubmittedSwapChain](const VulkanFramebuffer *framebuffer) {
			return !framebuffer || usesSubmittedSwapChain(framebuffer->GetSwapChain());
		};

		for(auto iterator = renderPasses.begin(); iterator != renderPasses.end();)
		{
			if(usesSubmittedFramebufferSwapChain(iterator->framebuffer) && usesSubmittedFramebufferSwapChain(iterator->resolveFramebuffer))
				iterator++;
			else
				iterator = renderPasses.erase(iterator);
		}
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
