//
//  RNVulkanRenderer.cpp
//  Rayne
//
//  Copyright 2016 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNVulkanRenderer.h"
#include "RNVulkanWindow.h"
#include "RNVulkanInternals.h"
#include "RNVulkanShader.h"
#include "RNVulkanShaderLibrary.h"
#include "RNVulkanFramebuffer.h"
#include "RNVulkanDynamicGPUBuffer.h"
#include "RNVulkanStaticGPUBuffer.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace RN
{
	RNDefineMeta(VulkanRenderer, Renderer)

	VulkanRenderer::VulkanRenderer(VulkanRendererDescriptor *descriptor, VulkanDevice *device) :
		Renderer(descriptor, device),
		_mainWindow(nullptr),
		_currentFrame(0),
		_completedFrame(-1),
		_mipMapTextures(new Array()),
		_submittedCommandBuffers(new Array()),
		_executedCommandBuffers(new Array()),
		_currentCommandBuffer(nullptr),
		_currentResourcesCommandBuffer(nullptr),
		_commandBufferPool(new Array()),
		_commandBufferResourcesPool(new Array()),
		_defaultPostProcessingDrawable(nullptr),
		_currentMultiviewLayer(0),
		_currentMultiviewCount(0),
		_currentMultiviewFallbackRenderPass(nullptr)
	{
		vk::GetDeviceQueue(device->GetDevice(), device->GetWorkQueue(), 0, &_workQueue);

		VmaVulkanFunctions vulkanFunctions = {};
		vulkanFunctions.vkGetInstanceProcAddr = vk::GetInstanceProcAddr;
		vulkanFunctions.vkGetDeviceProcAddr = vk::GetDeviceProcAddr;
		vulkanFunctions.vkGetPhysicalDeviceProperties = vk::GetPhysicalDeviceProperties;
		vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vk::GetPhysicalDeviceMemoryProperties;
		vulkanFunctions.vkAllocateMemory = vk::AllocateMemory;
		vulkanFunctions.vkFreeMemory = vk::FreeMemory;
		vulkanFunctions.vkMapMemory = vk::MapMemory;
		vulkanFunctions.vkUnmapMemory = vk::UnmapMemory;
		vulkanFunctions.vkFlushMappedMemoryRanges = vk::FlushMappedMemoryRanges;
		vulkanFunctions.vkInvalidateMappedMemoryRanges = vk::InvalidateMappedMemoryRanges;
		vulkanFunctions.vkBindBufferMemory = vk::BindBufferMemory;
		vulkanFunctions.vkBindImageMemory = vk::BindImageMemory;
		vulkanFunctions.vkGetBufferMemoryRequirements = vk::GetBufferMemoryRequirements;
		vulkanFunctions.vkGetImageMemoryRequirements = vk::GetImageMemoryRequirements;
		vulkanFunctions.vkCreateBuffer = vk::CreateBuffer;
		vulkanFunctions.vkDestroyBuffer = vk::DestroyBuffer;
		vulkanFunctions.vkCreateImage = vk::CreateImage;
		vulkanFunctions.vkDestroyImage = vk::DestroyImage;
		vulkanFunctions.vkCmdCopyBuffer = vk::CmdCopyBuffer;

		VmaAllocatorCreateInfo allocatorCreateInfo = {};
		allocatorCreateInfo.device = device->GetDevice();
		allocatorCreateInfo.physicalDevice = device->GetPhysicalDevice();
		allocatorCreateInfo.instance = device->GetInstance()->GetInstance();
		allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_0;
		allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
		RNVulkanValidate(vmaCreateAllocator(&allocatorCreateInfo, &_internals->memoryAllocator));

		//Create command pool
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = device->GetWorkQueue();
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		RNVulkanValidate(vk::CreateCommandPool(device->GetDevice(), &cmdPoolInfo, nullptr, &_commandPool));

		//Create additional command pool for resource loading
		RNVulkanValidate(vk::CreateCommandPool(device->GetDevice(), &cmdPoolInfo, nullptr, &_commandPoolSynchronised));

		_defaultShaderLibrary = CreateShaderLibraryWithFile(RNCSTR(":RayneVulkan:/Shaders.json"));
		_dynamicBufferPool = new VulkanDynamicBufferPool();

		_internals->descriptorPool.Init(this);

		_internals->stateCoordinator.LoadPipelineCache(Kernel::GetSharedInstance()->GetApplication()->GetBuildNumber(), device, GetAllocatorCallback());

		#if RN_PROFILE_TRACY
			_internals->tracyCommandBuffer = GetCommandBuffer()->Retain();
			_internals->tracyVulkanCtx = RN_PROFILE_VULKAN_DECLARE_CONTEXT(device->GetInstance()->GetInstance(), device->GetDevice(), device->GetPhysicalDevice(), _workQueue, _internals->tracyCommandBuffer->GetCommandBuffer(), vk::GetInstanceProcAddr, vk::GetDeviceProcAddr);
		#else
			_internals->tracyCommandBuffer = nullptr;
			_internals->tracyVulkanCtx = nullptr;
		#endif
	}

	VulkanRenderer::~VulkanRenderer()
	{
		_internals->stateCoordinator.DestroyPipelineCache(GetVulkanDevice(), GetAllocatorCallback());

		_mipMapTextures->Release();

		for(auto &frameRes : _internals->frameResources)
			frameRes.finishedCallback();
		_internals->frameResources.clear();

		delete _dynamicBufferPool;

		vmaDestroyAllocator(_internals->memoryAllocator);
	}

	void VulkanRenderer::ResetDrawBindStateCache()
	{
		_internals->drawBindStateCache.pipeline = VK_NULL_HANDLE;
		_internals->drawBindStateCache.pipelineLayout = VK_NULL_HANDLE;
		_internals->drawBindStateCache.descriptorSet = VK_NULL_HANDLE;
		_internals->drawBindStateCache.vertexBufferCount = 0;
		for(uint8 i = 0; i < 3; i++)
		{
			_internals->drawBindStateCache.vertexBuffers[i] = VK_NULL_HANDLE;
			_internals->drawBindStateCache.vertexOffsets[i] = 0;
		}
		_internals->drawBindStateCache.hasIndexBufferBinding = false;
		_internals->drawBindStateCache.indexBuffer = VK_NULL_HANDLE;
		_internals->drawBindStateCache.indexOffset = 0;
		_internals->drawBindStateCache.indexType = VK_INDEX_TYPE_UINT16;
	}

	VkRenderPass VulkanRenderer::GetVulkanRenderPass(const VulkanRenderPass *renderPass)
	{
		return _internals->stateCoordinator.GetRenderPassState(renderPass)->renderPass;
	}

	void VulkanRenderer::CreateVulkanCommandBuffers(size_t count, std::vector<VkCommandBuffer> &buffers)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = _commandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(count);

		buffers.resize(count);
		RNVulkanValidate(vk::AllocateCommandBuffers(GetVulkanDevice()->GetDevice(), &commandBufferAllocateInfo, buffers.data()));
	}

	VkCommandBuffer VulkanRenderer::CreateVulkanCommandBuffer()
	{
		std::vector<VkCommandBuffer> buffers;
		CreateVulkanCommandBuffers(1, buffers);

		return buffers[0];
	}

	VulkanCommandBuffer *VulkanRenderer::GetCommandBuffer()
	{
		VulkanCommandBuffer *commandBuffer = nullptr;

		if(_commandBufferPool->GetCount() == 0)
		{
			commandBuffer = new VulkanCommandBuffer(GetVulkanDevice()->GetDevice(), _commandPool);
			commandBuffer->_commandBuffer = CreateVulkanCommandBuffer();
		}
		else
		{
			commandBuffer = _commandBufferPool->GetLastObject<VulkanCommandBuffer>();
			commandBuffer->Retain();
			_commandBufferPool->RemoveObjectAtIndex(_commandBufferPool->GetCount() - 1);
		}

		return commandBuffer->Autorelease();
	}

	VulkanCommandBuffer *VulkanRenderer::StartResourcesCommandBuffer()
	{
		RN_PROFILE_SCOPE();
		_currentResourcesCommandBufferLock.Lock();
		if(!_currentResourcesCommandBuffer)
		{
			VulkanCommandBuffer *commandBuffer = nullptr;
			for(int i = 0; i < _commandBufferResourcesPool->GetCount(); i++)
			{
				commandBuffer = static_cast<VulkanCommandBuffer*>(_commandBufferResourcesPool->GetObjectAtIndex(i));
				if(commandBuffer->_frameValue < _completedFrame && _completedFrame != -1) break;
			}

			if(!commandBuffer || commandBuffer->_frameValue >= _completedFrame || _completedFrame == -1)
			{
				commandBuffer = new VulkanCommandBuffer(GetVulkanDevice()->GetDevice(), _commandPoolSynchronised);

				VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
				commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				commandBufferAllocateInfo.commandPool = _commandPoolSynchronised;
				commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				commandBufferAllocateInfo.commandBufferCount = 1;

				RNVulkanValidate(vk::AllocateCommandBuffers(GetVulkanDevice()->GetDevice(), &commandBufferAllocateInfo, &commandBuffer->_commandBuffer));

				_commandBufferResourcesPool->AddObject(commandBuffer);
				commandBuffer->Release();
			}
			else
			{
				commandBuffer->Reset();
			}

			_currentResourcesCommandBuffer = commandBuffer; //already retained at as part of the pool array, not doing any memory management on this!
			_currentResourcesCommandBuffer->Begin();
		}

		return _currentResourcesCommandBuffer;
	}

	void VulkanRenderer::EndResourcesCommandBuffer()
	{
		RN_DEBUG_ASSERT(_currentResourcesCommandBuffer, "No active Resources command buffer!");
		_currentResourcesCommandBufferLock.Unlock();
	}

	void VulkanRenderer::SubmitCommandBuffer(VulkanCommandBuffer *commandBuffer)
	{
		_lock.Lock();
		_submittedCommandBuffers->AddObject(commandBuffer);
		_lock.Unlock();
	}

	Window *VulkanRenderer::CreateAWindow(const Vector2 &size, Screen *screen, const Window::SwapChainDescriptor &descriptor, void *hwnd)
	{
		VulkanWindow *window = new VulkanWindow(size, screen, this, descriptor, hwnd);

		if(!_mainWindow)
			_mainWindow = window->Retain();

		return window;
	}

	void VulkanRenderer::SetMainWindow(Window *window)
	{
		_mainWindow = window;
	}

	Window *VulkanRenderer::GetMainWindow()
	{
		return _mainWindow;
	}

	void VulkanRenderer::UpdateFrameFences()
	{
		RN_PROFILE_SCOPE();
		//Check fence status
		int index = 0;
		int freeFenceIndex = -1;
		for(VkFence fence : _frameFences)
		{
			if(_frameFenceValues[index] != -1)
			{
				VkResult status = vk::GetFenceStatus(GetVulkanDevice()->GetDevice(), fence);
				if(status < 0)
				{
					RNVulkanValidate(status);
					index += 1;
					continue;
				}
				if(status == VK_SUCCESS)
				{
					const size_t completedFrameValue = _frameFenceValues[index];
					if(_completedFrame == static_cast<size_t>(-1) || completedFrameValue > _completedFrame)
					{
						_completedFrame = completedFrameValue;
					}
					ReleaseFrameResources(completedFrameValue);
					_internals->descriptorPool.ResetFramePool(this, index);
					RNVulkanValidate(vk::ResetFences(GetVulkanDevice()->GetDevice(), 1, &fence));

					_frameFenceValues[index] = -1;
					freeFenceIndex = index;
				}
			}
			else
			{
				freeFenceIndex = index;
			}

			index += 1;
		}

		if(freeFenceIndex == -1)
		{
			VkFenceCreateInfo fenceInfo = {};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

			VkFence frameFence;
			RNVulkanValidate(vk::CreateFence(GetVulkanDevice()->GetDevice(), &fenceInfo, GetAllocatorCallback(), &frameFence));
			_frameFences.push_back(frameFence);
			_frameFenceValues.push_back(-1);

			freeFenceIndex = _frameFences.size() - 1;
		}

		_currentFrameFenceIndex = freeFenceIndex;
		_internals->descriptorPool.SetActiveFramePool(this, _currentFrameFenceIndex);
	}

	void VulkanRenderer::ReleaseFrameResources(uint32 frame)
	{
		RN_PROFILE_SCOPE();
		//Delete command lists that finished execution on the graphics card (the command allocator needs to be alive the whole time)
		for(int i = _executedCommandBuffers->GetCount() - 1; i >= 0; i--)
		{
			VulkanCommandBuffer *commandBuffer = _executedCommandBuffers->GetObjectAtIndex<VulkanCommandBuffer>(i);
			if(commandBuffer->_frameValue <= frame) //Will be added to the executed list AFTER ReleaseFrameResources is called, so this check is fine
			{
				_commandBufferPool->AddObject(commandBuffer);
				_executedCommandBuffers->RemoveObjectAtIndex(i);
				commandBuffer->Reset();
			}
		}

		//Free other frame resources such as unused framebuffers and imageviews
		Lock();
		for(int i = _internals->frameResources.size()-1; i >= 0; i--)
		{
			VulkanFrameResource &frameResource = _internals->frameResources[i];
			if(frameResource.frame < frame) //Might be added to the frame resources just before this call, without finishing using them, so just using < here to keep around for one more frame
			{
				if(frameResource.finishedCallback)
				{
					frameResource.finishedCallback();
				}

				_internals->frameResources.erase(_internals->frameResources.begin() + i);
			}
		}
		Unlock();
	}

	void VulkanRenderer::Render(Function &&function)
	{
		RN_PROFILE_SCOPE();

		_internals->stateCoordinator.SavePipelineCache(Kernel::GetSharedInstance()->GetApplication()->GetBuildNumber(), GetVulkanDevice()); //This won't do anything if no new pipelines were loaded

		_currentDrawableIndex = 0;
		_internals->renderPasses.clear();
		_internals->totalDrawableCount = 0;
		_internals->currentRenderPassIndex = 0;
		_internals->currentDrawableResourceIndex = 0;
		_internals->totalDescriptorTables = 0;
		_internals->swapChains.clear();
	//		_currentRootSignature = nullptr;

		UpdateFrameFences(); //Releases resources of frames that finished

		const bool hasCompletedFrame = (_completedFrame != static_cast<size_t>(-1));
		if((!hasCompletedFrame && _currentFrame > 4) || (hasCompletedFrame && (_currentFrame - _completedFrame > 4)))
		{
			//RNDebug("Too many frames in-flight, ignore this one");
			return; //Don't submit a new frame if there are already 5 frames in flight
		}

		CreateMipMaps();

		_frameStatistics.clear();

		_currentResourcesCommandBufferLock.Lock();
		VulkanCommandBuffer *resourcesCommandBuffer = _currentResourcesCommandBuffer;
		if(resourcesCommandBuffer)
		{
			resourcesCommandBuffer->End();
			resourcesCommandBuffer->_frameValue = _currentFrame;
		}
		_currentResourcesCommandBuffer = nullptr; //Always stays inside it's pool array, so just don't do any retain release
		_currentResourcesCommandBufferLock.Unlock();

		_lock.Lock();
		if(_submittedCommandBuffers->GetCount() > 0 || resourcesCommandBuffer)
		{
			std::vector<VkCommandBuffer> buffers;

			buffers.reserve(_submittedCommandBuffers->GetCount() + 1);
			if(resourcesCommandBuffer)
			{
				buffers.push_back(resourcesCommandBuffer->_commandBuffer);
			}
			_submittedCommandBuffers->Enumerate<VulkanCommandBuffer>([&](VulkanCommandBuffer *buffer, int i, bool &stop){
				buffer->_frameValue = _currentFrame;
				buffers.push_back(buffer->_commandBuffer);
				_executedCommandBuffers->AddObject(buffer);
			});

			//Submit command buffers
			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = buffers.size();
			submitInfo.pCommandBuffers = buffers.data();

			RNVulkanValidate(vk::QueueSubmit(_workQueue, 1, &submitInfo, VK_NULL_HANDLE));

			//RNVulkanValidate(vk::DeviceWaitIdle(GetVulkanDevice()->GetDevice()));
			_submittedCommandBuffers->RemoveAllObjects();
		}
		_lock.Unlock();

		//SubmitCamera is called for each camera and creates lists of drawables per camera
		function();

		_dynamicBufferPool->Update(this, _currentFrame, _completedFrame);
		UpdateDescriptorSets();

		for(VulkanSwapChain *swapChain : _internals->swapChains)
		{
			swapChain->AcquireBackBuffer();
		}

		_currentCommandBuffer = GetCommandBuffer();
		_currentCommandBuffer->Retain();
		_currentCommandBuffer->Begin();
		ResetDrawBindStateCache();

		if(_internals->swapChains.size() > 0)
		{
			VkCommandBuffer commandBuffer = _currentCommandBuffer->GetCommandBuffer();
			RN_PROFILE_VULKAN_SCOPE_CMD(_internals->tracyVulkanCtx, commandBuffer);

			for(VulkanSwapChain *swapChain : _internals->swapChains)
			{
				swapChain->Prepare(commandBuffer);
			}

			_internals->currentRenderPassIndex = 0;
			_internals->currentDrawableResourceIndex = 0;
			for(const VulkanRenderPass &renderPass : _internals->renderPasses)
			{
				if(renderPass.type != VulkanRenderPass::Type::Default && renderPass.type != VulkanRenderPass::Type::Convert)
				{
					RenderAPIRenderPass(_currentCommandBuffer, renderPass);
					_internals->currentRenderPassIndex += 1;
					_internals->currentDrawableResourceIndex += 1;
					continue;
				}

				//Set shadow depth texture layout for reading
				if(renderPass.directionalShadowDepthTexture)
				{
					VulkanTexture::SetImageLayout(commandBuffer, renderPass.directionalShadowDepthTexture->GetVulkanImage(), 0, renderPass.directionalShadowDepthTexture->GetDescriptor().mipMaps, 0, renderPass.directionalShadowDepthTexture->GetDescriptor().depth, VK_IMAGE_ASPECT_DEPTH_BIT, renderPass.directionalShadowDepthTexture->GetCurrentLayout(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VulkanTexture::BarrierIntent::ShaderSource);
					renderPass.directionalShadowDepthTexture->SetCurrentLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
				}

				//Set textures layout for reading for render targets that are used in this frame
				for(VulkanTexture *vulkanTexture : renderPass.renderTargetsUsedInShader)
				{
					const Texture::Format format = vulkanTexture->GetDescriptor().format;
					VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					VkImageLayout targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					if(format == Texture::Format::Depth_24_Stencil_8 || format == Texture::Format::Depth_32F_Stencil_8)
					{
						aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
						targetLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
					}
					else if(format == Texture::Format::Depth_16I || format == Texture::Format::Depth_24I || format == Texture::Format::Depth_32F)
					{
						aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
						targetLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
					}
					else if(format == Texture::Format::Stencil_8)
					{
						aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
						targetLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
					}

					if(vulkanTexture->GetCurrentLayout() == targetLayout) continue; //Nothing to do if the layout is already correct

					VulkanTexture::SetImageLayout(commandBuffer, vulkanTexture->GetVulkanImage(), 0, vulkanTexture->GetDescriptor().mipMaps, 0, vulkanTexture->GetDescriptor().depth, aspectMask, vulkanTexture->GetCurrentLayout(), targetLayout, VulkanTexture::BarrierIntent::ShaderSource);
					vulkanTexture->SetCurrentLayout(targetLayout);
				}

				//Set previous framebuffer texture layout for reading
				if(renderPass.previousStoredFramebuffer)
				{
					Texture *texture = renderPass.previousStoredFramebuffer->GetColorTexture(0);
					if(texture)
					{
						VulkanTexture *vulkanTexture = texture->Downcast<VulkanTexture>();
						if(vulkanTexture->GetCurrentLayout() != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
						{
							VulkanTexture::SetImageLayout(commandBuffer, vulkanTexture->GetVulkanImage(), 0, vulkanTexture->GetDescriptor().mipMaps, 0, vulkanTexture->GetDescriptor().depth, VK_IMAGE_ASPECT_COLOR_BIT, vulkanTexture->GetCurrentLayout(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VulkanTexture::BarrierIntent::ShaderSource);
							vulkanTexture->SetCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
						}
					}
				}

				if(renderPass.subpasses.size() > 0)
				{
					//Determine first/last usage of each color attachment across subpasses to choose appropriate initial/final layouts
					uint32 numColorAttachments = renderPass.renderPass->GetFramebuffer()->GetColorTargetCount();

					for(uint32 ci = 0; ci < numColorAttachments; ++ci)
					{
						Texture *t = renderPass.renderPass->GetFramebuffer()->GetColorTexture(ci);
						if(!t) continue;

						VulkanTexture *vulkanTexture = t->Downcast<VulkanTexture>();
						VkImageLayout initialLayout = renderPass.renderPass->GetSubpassFirstUseIsRead(ci)? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
						VkImageLayout targetLayout = renderPass.renderPass->GetSubpassLastUseIsRead(ci)? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

						if(vulkanTexture->GetCurrentLayout() != initialLayout)
						{
							VulkanTexture::SetImageLayout(commandBuffer, vulkanTexture->GetVulkanImage(), 0, vulkanTexture->GetDescriptor().mipMaps, 0, vulkanTexture->GetDescriptor().depth, VK_IMAGE_ASPECT_COLOR_BIT, vulkanTexture->GetCurrentLayout(), initialLayout, renderPass.renderPass->GetSubpassFirstUseIsRead(ci) ? VulkanTexture::BarrierIntent::ShaderSource : VulkanTexture::BarrierIntent::RenderTarget);
							vulkanTexture->SetCurrentLayout(initialLayout);
						}
					}

					if(renderPass.renderPass->GetFramebuffer()->GetDepthStencilTexture())
					{
						Texture *t = renderPass.renderPass->GetFramebuffer()->GetDepthStencilTexture();
						if(t)
						{
							bool depthFirstIsReadOnly = renderPass.renderPass->GetSubpassFirstDepthStencilUseIsRead();
							bool depthLastIsReadOnly = renderPass.renderPass->GetSubpassLastDepthStencilUseIsRead();

							VulkanTexture *vulkanTexture = t->Downcast<VulkanTexture>();
							VkImageLayout initialLayout = depthFirstIsReadOnly? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
							VkImageLayout targetLayout = depthLastIsReadOnly? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

							if(vulkanTexture->GetCurrentLayout() != initialLayout)
							{
								VulkanTexture::SetImageLayout(commandBuffer, vulkanTexture->GetVulkanImage(), 0, vulkanTexture->GetDescriptor().mipMaps, 0, vulkanTexture->GetDescriptor().depth, VK_IMAGE_ASPECT_DEPTH_BIT, vulkanTexture->GetCurrentLayout(), initialLayout, depthFirstIsReadOnly ? VulkanTexture::BarrierIntent::ShaderSource : VulkanTexture::BarrierIntent::RenderTarget);
								vulkanTexture->SetCurrentLayout(initialLayout);
							}
						}
					}

					SetupRendertargets(commandBuffer, renderPass);

					uint32 counter = 0;
					for(const VulkanRenderPass &subpass : renderPass.subpasses)
					{
						//TODO: Sort drawables by camera and root signature? Maybe not...
						//Draw drawables
						uint32 stepSize = 0;
						uint32 stepSizeIndex = 0;
						for(size_t i = 0; i < subpass.drawables.size(); i+= stepSize)
						{
							stepSize = subpass.instanceSteps[stepSizeIndex++];
							RenderDrawable(commandBuffer, subpass.drawables[i], stepSize);
						}

						//RNDebug("draw calls: " << subpass.instanceSteps.size());

						counter++;
						_internals->currentDrawableResourceIndex += 1;

						if(counter < renderPass.subpasses.size())
						{
							vk::CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
						}
					}
				}
				else
				{
					if(renderPass.drawables.size() > 0)
					{
						SetupRendertargets(commandBuffer, renderPass);

						//TODO: Sort drawables by camera and root signature? Maybe not...
						//Draw drawables
						uint32 stepSize = 0;
						uint32 stepSizeIndex = 0;
						for(size_t i = 0; i < renderPass.drawables.size(); i+= stepSize)
						{
							stepSize = renderPass.instanceSteps[stepSizeIndex++];
							RenderDrawable(commandBuffer, renderPass.drawables[i], stepSize);
						}

						//RNDebug("draw calls: " << renderPass.instanceSteps.size());
					}

					_internals->currentDrawableResourceIndex += 1;
				}

				if(renderPass.subpasses.size() > 0 || renderPass.drawables.size() > 0)
				{
					vk::CmdEndRenderPass(commandBuffer);
				}

				// Update tracked layouts for attachments to match final layouts of this render pass
				{
					VulkanFramebuffer *fb = renderPass.framebuffer;
					if(fb)
					{
						uint32 numColorAttachments = fb->GetColorTargetCount();

						for(uint32 ci = 0; ci < numColorAttachments; ++ci)
						{
							Texture *t = fb->GetColorTexture(ci);
							if(!t) continue;
							VulkanTexture *vt = t->Downcast<VulkanTexture>();
							vt->SetCurrentLayout(renderPass.renderPass->GetSubpassLastUseIsRead(ci)? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
						}

						// Depth-stencil
						Texture *dt = fb->GetDepthStencilTexture();
						if(dt)
						{
							bool depthLastIsReadOnly = renderPass.renderPass->GetSubpassLastDepthStencilUseIsRead();

							VulkanTexture *dvt = dt->Downcast<VulkanTexture>();
							dvt->SetCurrentLayout(depthLastIsReadOnly? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
						}
					}
				}

				//Set shadow depth texture layout for writing
				if(renderPass.directionalShadowDepthTexture)
				{
					VulkanTexture::SetImageLayout(commandBuffer, renderPass.directionalShadowDepthTexture->GetVulkanImage(), 0, renderPass.directionalShadowDepthTexture->GetDescriptor().mipMaps, 0, renderPass.directionalShadowDepthTexture->GetDescriptor().depth, VK_IMAGE_ASPECT_DEPTH_BIT, renderPass.directionalShadowDepthTexture->GetCurrentLayout(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VulkanTexture::BarrierIntent::RenderTarget);
					renderPass.directionalShadowDepthTexture->SetCurrentLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
				}

				//Set textures layout for writing for render targets that are used in this frame
				for(VulkanTexture *vulkanTexture : renderPass.renderTargetsUsedInShader)
				{
					const Texture::Format format = vulkanTexture->GetDescriptor().format;
					VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					VkImageLayout targetLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					if(format == Texture::Format::Depth_24_Stencil_8 || format == Texture::Format::Depth_32F_Stencil_8)
					{
						aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
						targetLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					}
					else if(format == Texture::Format::Depth_16I || format == Texture::Format::Depth_24I || format == Texture::Format::Depth_32F)
					{
						aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
						targetLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					}
					else if(format == Texture::Format::Stencil_8)
					{
						aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
						targetLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					}

					if(vulkanTexture->GetCurrentLayout() == targetLayout) continue; //Nothing to do if the layout is already correct

					VulkanTexture::SetImageLayout(commandBuffer, vulkanTexture->GetVulkanImage(), 0, vulkanTexture->GetDescriptor().mipMaps, 0, vulkanTexture->GetDescriptor().depth, aspectMask, vulkanTexture->GetCurrentLayout(), targetLayout, VulkanTexture::BarrierIntent::RenderTarget);
					vulkanTexture->SetCurrentLayout(targetLayout);
				}

				//Set previous framebuffer texture layout for writing
				if(renderPass.previousStoredFramebuffer)
				{
					Texture *texture = renderPass.previousStoredFramebuffer->GetColorTexture(0);
					if(texture)
					{
						VulkanTexture *vulkanTexture = texture->Downcast<VulkanTexture>();
						if(vulkanTexture->GetCurrentLayout() != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
						{
							VulkanTexture::SetImageLayout(commandBuffer, vulkanTexture->GetVulkanImage(), 0, vulkanTexture->GetDescriptor().mipMaps, 0, vulkanTexture->GetDescriptor().depth, VK_IMAGE_ASPECT_COLOR_BIT, vulkanTexture->GetCurrentLayout(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VulkanTexture::BarrierIntent::RenderTarget);
							vulkanTexture->SetCurrentLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
						}
					}
				}

				_internals->currentRenderPassIndex += 1;
			}

			for(VulkanSwapChain *swapChain : _internals->swapChains)
			{
				swapChain->Finalize(commandBuffer);
			}
		}

		_dynamicBufferPool->FlushAllBuffers();

		//Prepare command buffer submission
		std::vector<VkSemaphore> presentSemaphores;
		std::vector<VkPipelineStageFlags> presentSemaphoresWaitStages;
		std::vector<VkSemaphore> renderSemaphores;

		for(VulkanSwapChain *swapChain : _internals->swapChains)
		{
			VkSemaphore presentSemaphore = swapChain->GetCurrentPresentSemaphore();
			VkSemaphore renderSemaphore = swapChain->GetCurrentRenderSemaphore();
			if(presentSemaphore != VK_NULL_HANDLE)
			{
				presentSemaphores.push_back(presentSemaphore);
				presentSemaphoresWaitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
			}
			if(renderSemaphore != VK_NULL_HANDLE) renderSemaphores.push_back(renderSemaphore);
		}

		RN_PROFILE_VULKAN_COLLECT(_internals->tracyVulkanCtx, _currentCommandBuffer->GetCommandBuffer());

		_currentCommandBuffer->End();
		SubmitCommandBuffer(_currentCommandBuffer);
		_currentCommandBuffer = nullptr;

		std::vector<VkCommandBuffer> buffers;
		_lock.Lock();
		if(_submittedCommandBuffers->GetCount() == 0)
		{
			_lock.Unlock();
			return;
		}

		buffers.reserve(_submittedCommandBuffers->GetCount());
		_submittedCommandBuffers->Enumerate<VulkanCommandBuffer>([&](VulkanCommandBuffer *buffer, int i, bool &stop){
			buffer->_frameValue = _currentFrame;
			buffers.push_back(buffer->_commandBuffer);
			_executedCommandBuffers->AddObject(buffer);
		});
		_submittedCommandBuffers->RemoveAllObjects();
		_lock.Unlock();

		//Submit command buffers
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = buffers.size();
		submitInfo.pCommandBuffers = buffers.data();
		submitInfo.waitSemaphoreCount = presentSemaphores.size();
		submitInfo.pWaitSemaphores = presentSemaphores.data();
		submitInfo.signalSemaphoreCount = renderSemaphores.size();
		submitInfo.pSignalSemaphores = renderSemaphores.data();

		submitInfo.pWaitDstStageMask = presentSemaphoresWaitStages.data();

		_frameFenceValues[_currentFrameFenceIndex] = _currentFrame;
		RNVulkanValidate(vk::QueueSubmit(_workQueue, 1, &submitInfo, _frameFences[_currentFrameFenceIndex]));

		for(VulkanSwapChain *swapChain : _internals->swapChains)
		{
			swapChain->PresentBackBuffer(_workQueue);
		}

		RN_PROFILE_FRAME_TRACY();

		_currentFrame ++;
	}

	void VulkanRenderer::SetupRendertargets(VkCommandBuffer commandBuffer, const VulkanRenderPass &renderpass)
	{
		RN_PROFILE_SCOPE();
		RN_PROFILE_VULKAN_SCOPE_CMD_N(_internals->tracyVulkanCtx, commandBuffer, "SetupRendertargets");

		//TODO: Call PrepareAsRendertargetForFrame() only once per framebuffer per frame, find new solution for setting things up for msaa while reusing a framebuffer?
		{
			RN_PROFILE_VULKAN_SCOPE_CMD_N(_internals->tracyVulkanCtx, commandBuffer, "PrepareRendertargetForFrame");
			renderpass.framebuffer->PrepareAsRendertargetForFrame(&renderpass);
		}
		{
			RN_PROFILE_VULKAN_SCOPE_CMD_N(_internals->tracyVulkanCtx, commandBuffer, "SetAsRendertarget");
			renderpass.framebuffer->SetAsRendertarget(commandBuffer, renderpass.resolveFramebuffer, renderpass.renderPass->GetClearColor(), renderpass.renderPass->GetClearDepth(), renderpass.renderPass->GetClearStencil());
		}
		ResetDrawBindStateCache();

		//Setup viewport and scissor rect
		Rect cameraRect = renderpass.cameraViewport;

		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.x = cameraRect.x;
		viewport.y = cameraRect.y;
		viewport.width = cameraRect.width;
		viewport.height = cameraRect.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		vk::CmdSetViewport(commandBuffer, 0, 1, &viewport);

		// Update dynamic scissor state
		VkRect2D scissor = {};
		scissor.extent.width = static_cast<uint32_t>(cameraRect.width);
		scissor.extent.height = static_cast<uint32_t>(cameraRect.height);
		scissor.offset.x = cameraRect.x;
		scissor.offset.y = cameraRect.y;

		vk::CmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	//TODO: Merge parts of this with SubmitRenderPass and call it in here
	void VulkanRenderer::SubmitCamera(Camera *camera, Function &&function)
	{
		RN_PROFILE_SCOPE();
		VulkanRenderPass renderPass;
		renderPass.previousStoredFramebuffer = nullptr;

		const Array *multiviewCameras = camera->GetMultiviewCameras();
		if(multiviewCameras && multiviewCameras->GetCount() > 0)
		{
			if(multiviewCameras->GetCount() > 1 && GetVulkanDevice()->GetSupportsMultiview())
			{
				if(multiviewCameras->GetCount() <= GetVulkanDevice()->GetMaxMultiviewViewCount() || _currentMultiviewCount > 0)
				{
					multiviewCameras->Enumerate<Camera>([&](Camera *multiviewCamera, size_t index, bool &stop){
						if(_currentMultiviewCount > 0 && index < _currentMultiviewLayer) return;
						if(_currentMultiviewCount > 0 && index >= _currentMultiviewLayer + _currentMultiviewCount)
						{
							stop = true;
							return;
						}

						VulkanRenderPassCameraInfo cameraInfo;

						cameraInfo.viewPosition = multiviewCamera->GetWorldPosition();
						cameraInfo.viewMatrix = multiviewCamera->GetViewMatrix();
						cameraInfo.inverseViewMatrix = multiviewCamera->GetInverseViewMatrix();

						Matrix clipSpaceCorrectionMatrix;
						clipSpaceCorrectionMatrix.m[5] = -1.0f;
						cameraInfo.projectionMatrix = clipSpaceCorrectionMatrix * multiviewCamera->GetProjectionMatrix();
						cameraInfo.inverseProjectionMatrix = multiviewCamera->GetInverseProjectionMatrix();
						cameraInfo.projectionViewMatrix = cameraInfo.projectionMatrix * cameraInfo.viewMatrix;

						cameraInfo.lightManager = multiviewCamera->GetLightManager();

						renderPass.multiviewCameraInfo.push_back(cameraInfo);
					});
				}
				else
				{
					int increment = GetVulkanDevice()->GetMaxMultiviewViewCount();
					int i = 0;
					while(increment > 0)
					{
						_currentMultiviewLayer = i;
						_currentMultiviewCount = increment;

						RN::Function submission = RN::MakeFunction([&function](){ function(); });
						if(increment == 1)
						{
							_currentMultiviewFallbackRenderPass = camera->GetRenderPass();
							SubmitCamera(multiviewCameras->GetObjectAtIndex<Camera>(i), std::move(submission));
							_currentMultiviewFallbackRenderPass = nullptr;
						}
						else
						{
							SubmitCamera(camera, std::move(submission));
						}

						_currentMultiviewLayer = 0;
						_currentMultiviewCount = 0;

						i += increment;
						increment = std::min(static_cast<int>(GetVulkanDevice()->GetMaxMultiviewViewCount()), static_cast<int>(multiviewCameras->GetCount() - i));
					}

					return;
				}
			}
			else
			{
				//If multiview is not supported or there is only one multiview camera, render them individually (and ignore their parent camera)
				multiviewCameras->Enumerate<Camera>([&](Camera *multiviewCamera, size_t index, bool &stop){
					_currentMultiviewLayer = index;
					_currentMultiviewCount = 1;
					_currentMultiviewFallbackRenderPass = camera->GetRenderPass();

					RN::Function submission = RN::MakeFunction([&function](){ function(); });
					SubmitCamera(multiviewCamera, std::move(submission));

					_currentMultiviewLayer = 0;
					_currentMultiviewCount = 0;
					_currentMultiviewFallbackRenderPass = nullptr;
				});

				return;
			}
		}

		_frameStatistics.push_back({0, 0, 0, 0});

		RenderPass *cameraRenderPass = _currentMultiviewFallbackRenderPass? _currentMultiviewFallbackRenderPass : camera->GetRenderPass();
		cameraRenderPass->UpdateSubpassChain();

		renderPass.drawables.resize(0);
		renderPass.multiviewLayer = _currentMultiviewLayer;

		renderPass.type = VulkanRenderPass::Type::Default;
		renderPass.renderPass = cameraRenderPass;
		renderPass.previousRenderPass = nullptr;
		renderPass.currentPipelineState = nullptr;
		renderPass.currentInstanceDrawable = nullptr;

		renderPass.resolveFramebuffer = nullptr;

		renderPass.shaderHint = cameraRenderPass->GetShaderHint();
		renderPass.overrideMaterial = cameraRenderPass->GetOverrideMaterial();

		renderPass.cameraInfo.camera = camera;

		renderPass.cameraInfo.viewPosition = camera->GetWorldPosition();
		renderPass.cameraInfo.viewMatrix = camera->GetViewMatrix();
		renderPass.cameraInfo.inverseViewMatrix = camera->GetInverseViewMatrix();

		Matrix clipSpaceCorrectionMatrix;
		clipSpaceCorrectionMatrix.m[5] = -1.0f;
		renderPass.cameraInfo.projectionMatrix = clipSpaceCorrectionMatrix * camera->GetProjectionMatrix();
		renderPass.cameraInfo.inverseProjectionMatrix = camera->GetInverseProjectionMatrix();

		renderPass.cameraInfo.projectionViewMatrix = renderPass.cameraInfo.projectionMatrix * renderPass.cameraInfo.viewMatrix;
		renderPass.cameraInfo.lightManager = camera->GetLightManager();
		renderPass.directionalShadowDepthTexture = nullptr;

		renderPass.cameraAmbientColor = camera->GetAmbientColor();
		renderPass.cameraCustomData = camera->GetCustomData();

		renderPass.cameraClipDistance = Vector2(camera->GetClipNear(), camera->GetClipFar());
		renderPass.cameraFogDistance = Vector2(camera->GetFogNear(), camera->GetFogFar());
		renderPass.cameraFogColor0 = camera->GetFogColor0();
		renderPass.cameraFogColor1 = camera->GetFogColor1();
		renderPass.cameraTag = camera->GetTag();

		Framebuffer *framebuffer = cameraRenderPass->GetFramebuffer();
		if(!framebuffer) return;

		VulkanSwapChain *newSwapChain = nullptr;
		newSwapChain = framebuffer->Downcast<VulkanFramebuffer>()->GetSwapChain();
		renderPass.framebuffer = framebuffer->Downcast<VulkanFramebuffer>();

		if(newSwapChain)
		{
			bool notIncluded = true;
			for(VulkanSwapChain *swapChain : _internals->swapChains)
			{
				if(swapChain == newSwapChain)
				{
					notIncluded = false;
					break;
				}
			}

			if(notIncluded)
			{
				_internals->swapChains.push_back(newSwapChain);
			}
		}

		size_t previousRenderPassIndex = _internals->renderPasses.size();
		_internals->currentRenderPassIndex = previousRenderPassIndex;
		_internals->renderPasses.push_back(renderPass);

		const Array *nextRenderPasses = renderPass.renderPass->GetNextRenderPasses();

		size_t previousDrawableResourceIndex = _internals->currentDrawableResourceIndex;

		if(!renderPass.renderPass->GetIsSubpass() && !renderPass.renderPass->GetIsRoot()) _internals->currentDrawableResourceIndex += 1;
		nextRenderPasses->Enumerate<RenderPass>([&](RenderPass *nextPass, size_t index, bool &stop) {
			SubmitRenderPass(nextPass, _internals->renderPasses[previousRenderPassIndex]);
		});

		size_t newDrawableResourceIndex = _internals->currentDrawableResourceIndex;
		_internals->currentRenderPassIndex = previousRenderPassIndex;
		_internals->currentDrawableResourceIndex = previousDrawableResourceIndex;

		for(size_t i = previousRenderPassIndex; i < _internals->renderPasses.size(); i++)
		{
			//Calculate viewport size
			VulkanRenderPass &renderpass = _internals->renderPasses[i];
			renderpass.cameraViewport = renderpass.renderPass->GetFrame();
			if(renderpass.resolveFramebuffer)
			{
				if(renderpass.cameraViewport.width > renderpass.resolveFramebuffer->_size.x) renderpass.cameraViewport.width = renderpass.resolveFramebuffer->_size.x;
				if(renderpass.cameraViewport.height > renderpass.resolveFramebuffer->_size.y) renderpass.cameraViewport.height = renderpass.resolveFramebuffer->_size.y;
			}

			for(VulkanRenderPass &subpass : renderpass.subpasses)
			{
				subpass.cameraViewport = renderpass.cameraViewport;
			}

			renderpass.subpassSignature = 0;
			if(renderpass.subpasses.size() > 0)
			{
				uint32 colorAttachmentCount = renderpass.framebuffer->GetColorTargetCount();
				bool hasDepth = (renderpass.framebuffer->_depthStencilTarget != nullptr);

				uint64 signature = 0;
				for(size_t si = 0; si < renderpass.subpasses.size(); si++)
				{
					RenderPass *rp = renderpass.subpasses[si].renderPass;
					for(uint32 ci = 0; ci < colorAttachmentCount; ci++)
					{
						if(rp->GetSubpassWritesColorAttachment(ci)) signature ^= (0x9e3779b97f4a7c15ull + ((static_cast<uint64>(si) << 32) ^ ci));
						if(rp->GetSubpassReadColorAttachment(ci)) signature ^= (0x85ebca6b + ((static_cast<uint64>(si) << 33) ^ ci));
					}
					if(hasDepth)
					{
						if(rp->GetSubpassWritesDepthStencil()) signature ^= (0x27d4eb2f + (static_cast<uint64>(si) << 1));
						else if(rp->GetSubpassReadDepthStencilAttachment()) signature ^= (0x165667b1 + (static_cast<uint64>(si) << 1));
					}
				}
				renderpass.subpassSignature = signature ^ (static_cast<uint64>(renderpass.subpasses.size()) * 0x9e3779b97f4a7c15ull);
			}
		}

		// Run once to submit all scene nodes; SubmitDrawable will route to all matching passes
		_lock.Lock();
		function();
		_lock.Unlock();

		_internals->currentDrawableResourceIndex = newDrawableResourceIndex;
	}

	void VulkanRenderer::SubmitRenderPass(RenderPass *renderPass, VulkanRenderPass &previousRenderPass)
	{
		RN_PROFILE_SCOPE();

		renderPass->UpdateSubpassChain();
		_internals->currentSubpassIndex = 0;

		PostProcessingAPIStage *apiStage = renderPass->Downcast<PostProcessingAPIStage>();
		PostProcessingStage *ppStage = renderPass->Downcast<PostProcessingStage>();

		VulkanRenderPass vulkanRenderPass;

		if(vulkanRenderPass.type != VulkanRenderPass::Type::ResolveMSAA && !ppStage && vulkanRenderPass.type != VulkanRenderPass::Type::Convert)
		{
			vulkanRenderPass.cameraInfo = previousRenderPass.cameraInfo;
			vulkanRenderPass.multiviewCameraInfo = previousRenderPass.multiviewCameraInfo;
			vulkanRenderPass.multiviewLayer = previousRenderPass.multiviewLayer;
		}
		else
		{
			vulkanRenderPass.multiviewLayer = 0;
		}
		
		vulkanRenderPass.directionalShadowDepthTexture = nullptr;

		vulkanRenderPass.renderPass = renderPass;
		vulkanRenderPass.previousRenderPass = previousRenderPass.renderPass;
		vulkanRenderPass.previousStoredFramebuffer = nullptr;
		vulkanRenderPass.currentPipelineState = nullptr;
		vulkanRenderPass.currentInstanceDrawable = nullptr;

		vulkanRenderPass.framebuffer = nullptr;
		vulkanRenderPass.resolveFramebuffer = nullptr;

		vulkanRenderPass.shaderHint = renderPass->GetShaderHint();
		vulkanRenderPass.overrideMaterial = ppStage ? ppStage->GetMaterial() : renderPass->GetOverrideMaterial();

		if(!apiStage)
		{
			vulkanRenderPass.type = VulkanRenderPass::Type::Default;
		}
		else
		{
			switch(apiStage->GetType())
			{
				case PostProcessingAPIStage::Type::ResolveMSAA:
				{
					vulkanRenderPass.type = VulkanRenderPass::Type::ResolveMSAA;
					break;
				}
				case PostProcessingAPIStage::Type::Blit:
				{
					vulkanRenderPass.type = VulkanRenderPass::Type::Blit;
					break;
				}
				case PostProcessingAPIStage::Type::Convert:
				{
					vulkanRenderPass.type = VulkanRenderPass::Type::Convert;

	/*					if(!_ppConvertMaterial)
					{
						_ppConvertMaterial = Material::WithShaders(_defaultShaderLibrary->GetShaderWithName(RNCSTR("pp_vertex")), _defaultShaderLibrary->GetShaderWithName(RNCSTR("pp_blit_fragment")));
					}
					vulkanRenderPass.overrideMaterial = _ppConvertMaterial;*/
					break;
				}
			}
		}

		if(previousRenderPass.renderPass)
		{
			vulkanRenderPass.previousStoredFramebuffer = previousRenderPass.resolveFramebuffer ? previousRenderPass.resolveFramebuffer : previousRenderPass.framebuffer;
		}

		if(!renderPass->GetIsSubpass())
		{
			Framebuffer *framebuffer = renderPass->GetFramebuffer();
			VulkanSwapChain *newSwapChain = nullptr;
			newSwapChain = framebuffer->Downcast<VulkanFramebuffer>()->GetSwapChain();
			vulkanRenderPass.framebuffer = framebuffer->Downcast<VulkanFramebuffer>();

			if(newSwapChain)
			{
				bool notIncluded = true;
				for(VulkanSwapChain *swapChain : _internals->swapChains)
				{
					if(swapChain == newSwapChain)
					{
						notIncluded = false;
						break;
					}
				}

				if(notIncluded)
				{
					_internals->swapChains.push_back(newSwapChain);
				}
			}
		}

		if(vulkanRenderPass.type != VulkanRenderPass::Type::ResolveMSAA)
		{
			if(renderPass->GetIsSubpass())
			{
				previousRenderPass.subpasses.push_back(vulkanRenderPass);
			}
			else
			{
				_internals->currentRenderPassIndex = _internals->renderPasses.size();
				_internals->renderPasses.push_back(vulkanRenderPass);

				// Inject a default fullscreen quad for post processing passes so we don't redraw the whole scene.
				if(ppStage || vulkanRenderPass.type == VulkanRenderPass::Type::Convert)
				{
					if(!_defaultPostProcessingDrawable)
					{
						Mesh *planeMesh = Mesh::WithTexturedPlane(Quaternion::WithEulerAngle(Vector3(0.0f, 90.0f + 180.0f, 0.0f)), Vector3(0.0f), Vector2(1.0f, 1.0f));
						Material *planeMaterial = Material::WithShaders(GetDefaultShader(Shader::Type::Vertex, nullptr), GetDefaultShader(Shader::Type::Fragment, nullptr));

						_lock.Lock();
						_defaultPostProcessingDrawable = static_cast<VulkanDrawable*>(CreateDrawable());
						_defaultPostProcessingDrawable->Update(planeMesh, planeMaterial, nullptr, nullptr);
						_lock.Unlock();
					}

					SubmitDrawable(_defaultPostProcessingDrawable);
				}
			}

			if(!renderPass->GetIsRoot()) _internals->currentDrawableResourceIndex += 1;
		}
		else
		{
			_internals->renderPasses[_internals->currentRenderPassIndex].resolveFramebuffer = vulkanRenderPass.framebuffer;
		}

		const Array *nextRenderPasses = renderPass->GetNextRenderPasses();
		nextRenderPasses->Enumerate<RenderPass>([&](RenderPass *nextPass, size_t index, bool &stop) {
			SubmitRenderPass(nextPass, _internals->renderPasses[_internals->currentRenderPassIndex]);
		});
	}

	bool VulkanRenderer::SupportsTextureFormat(const String *format) const
	{
		return false;
	}
	bool VulkanRenderer::SupportsDrawMode(DrawMode mode) const
	{
		return false;
	}

	size_t VulkanRenderer::GetAlignmentForType(PrimitiveType type) const
	{
		switch(type)
		{
			case PrimitiveType::Uint8:
			case PrimitiveType::Int8:
				return 1;

			case PrimitiveType::Uint16:
			case PrimitiveType::Int16:
			case PrimitiveType::Half:
				return 2;

			case PrimitiveType::Uint32:
			case PrimitiveType::Int32:
			case PrimitiveType::Float:
			case PrimitiveType::HalfVector2:
				return 4;

			case PrimitiveType::Vector2:
			case PrimitiveType::HalfVector3:
			case PrimitiveType::HalfVector4:
			case PrimitiveType::Matrix2x2:
				return 8;

			case PrimitiveType::Vector3:
			case PrimitiveType::Vector4:
			case PrimitiveType::Matrix3x3:
			case PrimitiveType::Matrix4x4:
			case PrimitiveType::Quaternion:
			case PrimitiveType::Color:
				return 16;
		}

		return 1;
	}
	size_t VulkanRenderer::GetSizeForType(PrimitiveType type) const
	{
		switch(type)
		{
			case PrimitiveType::Uint8:
			case PrimitiveType::Int8:
				return 1;

			case PrimitiveType::Uint16:
			case PrimitiveType::Int16:
			case PrimitiveType::Half:
				return 2;

			case PrimitiveType::Uint32:
			case PrimitiveType::Int32:
			case PrimitiveType::Float:
			case PrimitiveType::HalfVector2:
				return 4;

			case PrimitiveType::Vector2:
			case PrimitiveType::HalfVector3:
			case PrimitiveType::HalfVector4:
				return 8;

			case PrimitiveType::Vector3:
			case PrimitiveType::Vector4:
			case PrimitiveType::Matrix2x2:
			case PrimitiveType::Quaternion:
			case PrimitiveType::Color:
				return 16;

			case PrimitiveType::Matrix3x3:
				return 48;
			case PrimitiveType::Matrix4x4:
				return 64;
		}
		return 1;
	}

	void VulkanRenderer::CreateMipMapForTexture(VulkanTexture *texture)
	{
		_lock.Lock();
		_mipMapTextures->AddObject(texture);
		_lock.Unlock();
	}

	void VulkanRenderer::CreateMipMaps()
	{
		RN_PROFILE_SCOPE();
		if(_mipMapTextures->GetCount() == 0)
			return;

		VulkanCommandBuffer *commandBuffer = StartResourcesCommandBuffer();

		_mipMapTextures->Enumerate<VulkanTexture>([&](VulkanTexture *texture, size_t index, bool &stop) {

			//TODO: Fix mipmap generation for texture arrays
			VulkanTexture::SetImageLayout(commandBuffer->GetCommandBuffer(), texture->GetVulkanImage(), 0, 1, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VulkanTexture::BarrierIntent::CopySource);
			VulkanTexture::SetImageLayout(commandBuffer->GetCommandBuffer(), texture->GetVulkanImage(), 1, texture->GetDescriptor().mipMaps-1, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VulkanTexture::BarrierIntent::CopyDestination);
			for(uint16 i = 0; i < texture->GetDescriptor().mipMaps-1; i++)
			{
				VkImageBlit imageBlit = {};

				imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageBlit.srcSubresource.mipLevel = i;
				imageBlit.srcSubresource.baseArrayLayer = 0;
				imageBlit.srcSubresource.layerCount = 1;

				imageBlit.srcOffsets[0].x = 0;
				imageBlit.srcOffsets[0].y = 0;
				imageBlit.srcOffsets[0].z = 0;
				imageBlit.srcOffsets[1].x = texture->GetDescriptor().GetWidthForMipMapLevel(i);
				imageBlit.srcOffsets[1].y = texture->GetDescriptor().GetHeightForMipMapLevel(i);
				imageBlit.srcOffsets[1].z = 1;

				imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageBlit.dstSubresource.mipLevel = i+1;
				imageBlit.dstSubresource.baseArrayLayer = 0;
				imageBlit.dstSubresource.layerCount = 1;

				imageBlit.dstOffsets[0].x = 0;
				imageBlit.dstOffsets[0].y = 0;
				imageBlit.dstOffsets[0].z = 0;
				imageBlit.dstOffsets[1].x = texture->GetDescriptor().GetWidthForMipMapLevel(i+1);
				imageBlit.dstOffsets[1].y = texture->GetDescriptor().GetHeightForMipMapLevel(i+1);
				imageBlit.dstOffsets[1].z = 1;

				vk::CmdBlitImage(commandBuffer->GetCommandBuffer(), texture->GetVulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->GetVulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

				VulkanTexture::SetImageLayout(commandBuffer->GetCommandBuffer(), texture->GetVulkanImage(), i + 1, 1, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VulkanTexture::BarrierIntent::CopySource);
			}

			VulkanTexture::SetImageLayout(commandBuffer->GetCommandBuffer(), texture->GetVulkanImage(), 0, texture->GetDescriptor().mipMaps, 0, texture->GetDescriptor().depth, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VulkanTexture::BarrierIntent::ShaderSource);
		});

		EndResourcesCommandBuffer();

		_mipMapTextures->RemoveAllObjects();
	}

	GPUBuffer *VulkanRenderer::CreateBufferWithLength(size_t length, GPUResource::UsageOptions usageOptions, GPUResource::AccessOptions accessOptions, bool isStreamable)
	{
		if(isStreamable)
		{
			return new VulkanDynamicGPUBuffer(this, length, usageOptions);
		}

		return (new VulkanStaticGPUBuffer(this, nullptr, length, usageOptions, accessOptions));
	}

	VulkanDynamicBufferReference *VulkanRenderer::GetConstantBufferReference(size_t size, size_t index, GPUResource::UsageOptions usageOptions)
	{
		VulkanDynamicBufferReference *reference = _dynamicBufferPool->GetDynamicBufferReference(size, index, usageOptions);
		return reference;
	}

	void VulkanRenderer::UpdateDynamicBufferReference(VulkanDynamicBufferReference *reference, bool align)
	{
		LockGuard<Lockable> lock(_lock);
		return _dynamicBufferPool->UpdateDynamicBufferReference(reference, align);
	}

	ShaderLibrary *VulkanRenderer::CreateShaderLibraryWithFile(const String *file)
	{
		return new VulkanShaderLibrary(file);
	}

	ShaderLibrary *VulkanRenderer::CreateShaderLibraryWithSource(const String *source)
	{
		RN_ASSERT(-1, "NOT IMPLEMENTED!");
		return nullptr;
	}

	ShaderLibrary *VulkanRenderer::GetDefaultShaderLibrary()
	{
		return _defaultShaderLibrary;
	}

	Texture *VulkanRenderer::CreateTextureWithDescriptor(const Texture::Descriptor &descriptor)
	{
		VulkanTexture *texture = new VulkanTexture(descriptor, this);
		return texture;
	}

	Framebuffer *VulkanRenderer::CreateFramebuffer(const Vector2 &size)
	{
		return new VulkanFramebuffer(size, this);
	}

	void VulkanRenderer::FillUniformBuffer(Shader::ArgumentBuffer *argumentBuffer, VulkanDynamicBufferReference *dynamicBufferReference, VulkanDrawable *drawable)
	{
		uint8 *buffer = reinterpret_cast<uint8 *>(dynamicBufferReference->dynamicBuffer->GetBuffer()) + dynamicBufferReference->offset;

		Material *overrideMaterial = _internals->renderPasses[_internals->currentRenderPassIndex].overrideMaterial;
		Material::Properties mergedMaterialProperties;
		drawable->material.GetMergedProperties(overrideMaterial, mergedMaterialProperties);
		const VulkanRenderPass &renderPass = _internals->renderPasses[_internals->currentRenderPassIndex];

		const RN::Array *uniformDescriptors = argumentBuffer->GetUniformDescriptors();
		size_t count = uniformDescriptors->GetCount();
		for(size_t index = 0; index < count; index ++)
		{
			Shader::UniformDescriptor *descriptor = static_cast<Shader::UniformDescriptor*>(uniformDescriptors->GetObjectAtIndex(index));
			switch(descriptor->GetIdentifier())
			{
				case Shader::UniformDescriptor::Identifier::Time:
				{
					float temp = static_cast<float>(Kernel::GetSharedInstance()->GetTotalTime());
					std::memcpy(buffer + descriptor->GetOffset(), &temp, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::ModelMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), drawable->modelMatrix.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseModelMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), drawable->inverseModelMatrix.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::NormalMatrix:
				{
					Matrix normalMatrix = drawable->inverseModelMatrix.GetTransposed();
					std::memcpy(buffer + descriptor->GetOffset(), &normalMatrix.m[0], 12);
					std::memcpy(buffer + descriptor->GetOffset() + 16, &normalMatrix.m[4], 12);
					std::memcpy(buffer + descriptor->GetOffset() + 32, &normalMatrix.m[8], 12);
					break;
				}

				case Shader::UniformDescriptor::Identifier::ModelViewMatrix:
				{
					Matrix result = renderPass.cameraInfo.viewMatrix * drawable->modelMatrix;
					std::memcpy(buffer + descriptor->GetOffset(), result.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::ModelViewMatrixMultiview:
				{
					if(renderPass.multiviewCameraInfo.size() > 0)
					{
						size_t viewCount = std::min(renderPass.multiviewCameraInfo.size(), descriptor->GetElementCount());
						for(int i = 0; i < viewCount; i++)
						{
							Matrix result = renderPass.multiviewCameraInfo[i].viewMatrix * drawable->modelMatrix;
							std::memcpy(buffer + descriptor->GetOffset() + 64 * i, result.m, 64);
						}
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::ModelViewProjectionMatrix:
				{
					Matrix result = renderPass.cameraInfo.projectionViewMatrix * drawable->modelMatrix;
					std::memcpy(buffer + descriptor->GetOffset(), result.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::ModelViewProjectionMatrixMultiview:
				{
					if(renderPass.multiviewCameraInfo.size() > 0)
					{
						size_t viewCount = std::min(renderPass.multiviewCameraInfo.size(), descriptor->GetElementCount());
						for(int i = 0; i < viewCount; i++)
						{
							Matrix result = renderPass.multiviewCameraInfo[i].projectionViewMatrix * drawable->modelMatrix;
							std::memcpy(buffer + descriptor->GetOffset() + 64 * i, result.m, 64);
						}
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::ViewMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), renderPass.cameraInfo.viewMatrix.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::ViewMatrixMultiview:
				{
					if(renderPass.multiviewCameraInfo.size() > 0)
					{
						size_t viewCount = std::min(renderPass.multiviewCameraInfo.size(), descriptor->GetElementCount());
						for(int i = 0; i < viewCount; i++)
						{
							std::memcpy(buffer + descriptor->GetOffset() + 64 * i, renderPass.multiviewCameraInfo[i].viewMatrix.m, 64);
						}
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::ViewProjectionMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), renderPass.cameraInfo.projectionViewMatrix.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::ViewProjectionMatrixMultiview:
				{
					if(renderPass.multiviewCameraInfo.size() > 0)
					{
						size_t viewCount = std::min(renderPass.multiviewCameraInfo.size(), descriptor->GetElementCount());
						for(int i = 0; i < viewCount; i++)
						{
							std::memcpy(buffer + descriptor->GetOffset() + 64 * i, renderPass.multiviewCameraInfo[i].projectionViewMatrix.m, 64);
						}
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::ProjectionMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), renderPass.cameraInfo.projectionMatrix.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::ProjectionMatrixMultiview:
				{
					if(renderPass.multiviewCameraInfo.size() > 0)
					{
						size_t viewCount = std::min(renderPass.multiviewCameraInfo.size(), descriptor->GetElementCount());
						for(int i = 0; i < viewCount; i++)
						{
							std::memcpy(buffer + descriptor->GetOffset() + 64 * i, renderPass.multiviewCameraInfo[i].projectionMatrix.m, 64);
						}
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseModelViewMatrix:
				{
					Matrix result = renderPass.cameraInfo.inverseViewMatrix * drawable->inverseModelMatrix;
					std::memcpy(buffer + descriptor->GetOffset(), result.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseModelViewMatrixMultiview:
				{
					if(renderPass.multiviewCameraInfo.size() > 0)
					{
						size_t viewCount = std::min(renderPass.multiviewCameraInfo.size(), descriptor->GetElementCount());
						for(int i = 0; i < viewCount; i++)
						{
							Matrix result = renderPass.multiviewCameraInfo[i].inverseViewMatrix * drawable->inverseModelMatrix;
							std::memcpy(buffer + descriptor->GetOffset() + 64 * i, result.m, 64);
						}
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseModelViewProjectionMatrix:
				{
					Matrix result = renderPass.cameraInfo.inverseProjectionViewMatrix * drawable->inverseModelMatrix;
					std::memcpy(buffer + descriptor->GetOffset(), result.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseModelViewProjectionMatrixMultiview:
				{
					if(renderPass.multiviewCameraInfo.size() > 0)
					{
						size_t viewCount = std::min(renderPass.multiviewCameraInfo.size(), descriptor->GetElementCount());
						for(int i = 0; i < viewCount; i++)
						{
							Matrix result = renderPass.multiviewCameraInfo[i].inverseProjectionViewMatrix * drawable->inverseModelMatrix;
							std::memcpy(buffer + descriptor->GetOffset() + 64 * i, result.m, 64);
						}
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseViewMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), renderPass.cameraInfo.inverseViewMatrix.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseViewMatrixMultiview:
				{
					if(renderPass.multiviewCameraInfo.size() > 0)
					{
						size_t viewCount = std::min(renderPass.multiviewCameraInfo.size(), descriptor->GetElementCount());
						for(int i = 0; i < viewCount; i++)
						{
							std::memcpy(buffer + descriptor->GetOffset() + 64 * i, renderPass.multiviewCameraInfo[i].inverseViewMatrix.m, 64);
						}
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseViewProjectionMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), renderPass.cameraInfo.inverseProjectionViewMatrix.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseViewProjectionMatrixMultiview:
				{
					if(renderPass.multiviewCameraInfo.size() > 0)
					{
						size_t viewCount = std::min(renderPass.multiviewCameraInfo.size(), descriptor->GetElementCount());
						for(int i = 0; i < viewCount; i++)
						{
							std::memcpy(buffer + descriptor->GetOffset() + 64 * i, renderPass.multiviewCameraInfo[i].inverseProjectionViewMatrix.m, 64);
						}
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseProjectionMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), renderPass.cameraInfo.inverseProjectionMatrix.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseProjectionMatrixMultiview:
				{
					if(renderPass.multiviewCameraInfo.size() > 0)
					{
						size_t viewCount = std::min(renderPass.multiviewCameraInfo.size(), descriptor->GetElementCount());
						for(int i = 0; i < viewCount; i++)
						{
							std::memcpy(buffer + descriptor->GetOffset() + 64 * i, renderPass.multiviewCameraInfo[i].inverseProjectionMatrix.m, 64);
						}
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraPosition:
				{
					Vector4 cameraPosition(renderPass.cameraInfo.viewPosition, 0.0f);
					std::memcpy(buffer + descriptor->GetOffset(), &cameraPosition.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraPositionMultiview:
				{
					if(renderPass.multiviewCameraInfo.size() > 0)
					{
						size_t viewCount = std::min(renderPass.multiviewCameraInfo.size(), descriptor->GetElementCount());
						for(int i = 0; i < viewCount; i++)
						{
							std::memcpy(buffer + descriptor->GetOffset() + 16 * i, &renderPass.multiviewCameraInfo[i].viewPosition.x, 16);
						}
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::AmbientColor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &mergedMaterialProperties.ambientColor.r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::DiffuseColor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &mergedMaterialProperties.diffuseColor.r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::SpecularColor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &mergedMaterialProperties.specularColor.r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::EmissiveColor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &mergedMaterialProperties.emissiveColor.r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CustomMatrix1:
				{
					std::memcpy(buffer + descriptor->GetOffset(), mergedMaterialProperties.customMatrix1.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CustomMatrix2:
				{
					std::memcpy(buffer + descriptor->GetOffset(), mergedMaterialProperties.customMatrix2.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::UIClippingRect:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &mergedMaterialProperties.uiClippingRect.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::UIOffset:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &mergedMaterialProperties.uiOffset.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::UIOutlineColor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &mergedMaterialProperties.uiOutlineColor.r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::TextureTileFactor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &mergedMaterialProperties.textureTileFactor, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::AlphaToCoverageClamp:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &mergedMaterialProperties.alphaToCoverageClamp.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraClipDistance:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &renderPass.cameraClipDistance.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraFogDistance:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &renderPass.cameraFogDistance.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraTag:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &renderPass.cameraTag, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraViewport:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &renderPass.cameraViewport.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraAmbientColor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &renderPass.cameraAmbientColor.r, descriptor->GetSize());
					break;
				}
				case Shader::UniformDescriptor::Identifier::CameraCustomData:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &renderPass.cameraCustomData.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraFogColor0:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &renderPass.cameraFogColor0.r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraFogColor1:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &renderPass.cameraFogColor1.r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::DirectionalLightsCount:
				{
					uint32 lightCount = std::min(renderPass.directionalLights.size(), descriptor->GetElementCount());
					std::memcpy(buffer + descriptor->GetOffset(), &lightCount, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::DirectionalLights:
				{
					size_t lightCount = std::min(renderPass.directionalLights.size(), descriptor->GetElementCount());
					if(lightCount > 0)
					{
						std::memcpy(buffer + descriptor->GetOffset(), &renderPass.directionalLights[0], (16 + 16) * lightCount);
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::DirectionalShadowMatricesCount:
				{
					//TODO: Limit matrixCount to descriptor->GetElementCount() of Shader::UniformDescriptor::Identifier::DirectionalShadowMatrices
					uint32 matrixCount = renderPass.directionalShadowMatrices.size();
					std::memcpy(buffer + descriptor->GetOffset(), &matrixCount, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::DirectionalShadowMatrices:
				{
					size_t matrixCount = std::min(renderPass.directionalShadowMatrices.size(), descriptor->GetElementCount());
					if(matrixCount > 0)
					{
						std::memcpy(buffer + descriptor->GetOffset(), &renderPass.directionalShadowMatrices[0].m[0], 64 * matrixCount);
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::DirectionalShadowInfo:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &renderPass.directionalShadowInfo.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::PointLights:
				{
					size_t lightCount = std::min(renderPass.pointLights.size(), descriptor->GetElementCount());
					if(lightCount > 0)
					{
						std::memcpy(buffer + descriptor->GetOffset(), &renderPass.pointLights[0], (12 + 4 + 16) * lightCount);
					}
					if(lightCount < descriptor->GetElementCount())
					{
						std::memset(buffer + descriptor->GetOffset() + (12 + 4 + 16) * lightCount, 0, (12 + 4 + 16) * (descriptor->GetElementCount() - lightCount));
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::SpotLights:
				{
					size_t lightCount = std::min(renderPass.spotLights.size(), descriptor->GetElementCount());
					if(lightCount > 0)
					{
						std::memcpy(buffer + descriptor->GetOffset(), &renderPass.spotLights[0], (12 + 4 + 12 + 4 + 16) * lightCount);
					}
					if(lightCount < descriptor->GetElementCount())
					{
						std::memset(buffer + descriptor->GetOffset() + (12 + 4 + 12 + 4 + 16) * lightCount, 0, (12 + 4 + 12 + 4 + 16) * (descriptor->GetElementCount() - lightCount));
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::BoneMatrices:
				{
					const std::vector<Matrix> &boneMatrices = drawable->skeleton.GetMatrices();
					if(boneMatrices.size() > 0)
					{
						size_t matrixCount = std::min(boneMatrices.size(), descriptor->GetElementCount());
						if(matrixCount > 0)
						{
							std::memcpy(buffer + descriptor->GetOffset(), &boneMatrices[0].m[0], 64 * matrixCount);
						}
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::Custom:
				{
					Object *object = mergedMaterialProperties.GetCustomShaderUniform(descriptor->GetNameHash());
					if(object)
					{
						if(object->IsKindOfClass(Value::GetMetaClass()))
						{
							Value *value = object->Downcast<Value>();
							switch(value->GetValueType())
							{
								case TypeTranslator<Vector2>::value:
								{
									if(descriptor->GetSize() == sizeof(Vector2))
									{
										Vector2 vector = value->GetValue<Vector2>();
										std::memcpy(buffer + descriptor->GetOffset(), &vector.x, descriptor->GetSize());
									}
									break;
								}
								case TypeTranslator<Vector3>::value:
								{
									if(descriptor->GetSize() == sizeof(Vector3))
									{
										Vector3 vector = value->GetValue<Vector3>();
										std::memcpy(buffer + descriptor->GetOffset(), &vector.x, descriptor->GetSize());
									}
									break;
								}
								case TypeTranslator<Vector4>::value:
								{
									if(descriptor->GetSize() == sizeof(Vector4))
									{
										Vector4 vector = value->GetValue<Vector4>();
										std::memcpy(buffer + descriptor->GetOffset(), &vector.x, descriptor->GetSize());
									}
									break;
								}
								case TypeTranslator<Matrix>::value:
								{
									if(descriptor->GetSize() == sizeof(Matrix))
									{
										Matrix matrix = value->GetValue<Matrix>();
										std::memcpy(buffer + descriptor->GetOffset(), &matrix.m[0], descriptor->GetSize());
									}
									break;
								}
								case TypeTranslator<Quaternion>::value:
								{
									if(descriptor->GetSize() == sizeof(Quaternion))
									{
										Quaternion quaternion = value->GetValue<Quaternion>();
										std::memcpy(buffer + descriptor->GetOffset(), &quaternion.x, descriptor->GetSize());
									}
									break;
								}
								case TypeTranslator<Color>::value:
								{
									if(descriptor->GetSize() == sizeof(Color))
									{
										Color color = value->GetValue<Color>();
										std::memcpy(buffer + descriptor->GetOffset(), &color.r, descriptor->GetSize());
									}
									break;
								}
								default:
									break;
							}
						}
						else
						{
							Number *number = object->Downcast<Number>();
							switch(number->GetType())
							{
								case Number::Type::Int8:
								{
									if(descriptor->GetSize() == sizeof(int8))
									{
										int8 value = number->GetInt8Value();
										std::memcpy(buffer + descriptor->GetOffset(), &value, descriptor->GetSize());
									}
									break;
								}
								case Number::Type::Int16:
								{
									if(descriptor->GetSize() == sizeof(int8))
									{
										int16 value = number->GetInt16Value();
										std::memcpy(buffer + descriptor->GetOffset(), &value, descriptor->GetSize());
									}
									break;
								}
								case Number::Type::Int32:
								{
									if(descriptor->GetSize() == sizeof(int32))
									{
										int32 value = number->GetInt32Value();
										std::memcpy(buffer + descriptor->GetOffset(), &value, descriptor->GetSize());
									}
									break;
								}
								case Number::Type::Uint8:
								{
									if(descriptor->GetSize() == sizeof(uint8))
									{
										uint8 value = number->GetUint8Value();
										std::memcpy(buffer + descriptor->GetOffset(), &value, descriptor->GetSize());
									}
									break;
								}
								case Number::Type::Uint16:
								{
									if(descriptor->GetSize() == sizeof(uint16))
									{
										uint16 value = number->GetUint16Value();
										std::memcpy(buffer + descriptor->GetOffset(), &value, descriptor->GetSize());
									}
									break;
								}
								case Number::Type::Uint32:
								{
									if(descriptor->GetSize() == sizeof(uint32))
									{
										uint32 value = number->GetUint32Value();
										std::memcpy(buffer + descriptor->GetOffset(), &value, descriptor->GetSize());
									}
									break;
								}
								case Number::Type::Float32:
								{
									if(descriptor->GetSize() == sizeof(float))
									{
										float value = number->GetFloatValue();
										std::memcpy(buffer + descriptor->GetOffset(), &value, descriptor->GetSize());
									}
									break;
								}
								case Number::Type::Boolean:
								{
									if(descriptor->GetSize() == sizeof(bool))
									{
										bool value = number->GetBoolValue();
										std::memcpy(buffer + descriptor->GetOffset(), &value, descriptor->GetSize());
									}
									break;
								}
								default:
									break;
							}
						}
					}
					break;
				}

				default:
					break;
			}
		}
	}

	Drawable *VulkanRenderer::CreateDrawable()
	{
		VulkanDrawable *newDrawable = new VulkanDrawable();
		return newDrawable;
	}

	void VulkanRenderer::DeleteDrawable(Drawable *drawable)
	{
		delete drawable;
	}

	void VulkanRenderer::SubmitLight(const Light *light)
	{
		// Distribute the light to all passes belonging to the current camera range
		size_t startIndex = _internals->currentRenderPassIndex;
		size_t originalIndex = _internals->currentRenderPassIndex;

		for(size_t pi = startIndex; pi < _internals->renderPasses.size(); pi++)
		{
			VulkanRenderPass &renderPass = _internals->renderPasses[pi];
			// Only real draw passes
			if(renderPass.type != VulkanRenderPass::Type::Default && renderPass.type != VulkanRenderPass::Type::Convert) continue;

			if(light->GetType() == Light::Type::DirectionalLight)
			{
				renderPass.directionalLights.push_back(VulkanDirectionalLight{ light->GetForward(), 0.0f, light->GetFinalColor() });

				if(light->HasShadows())
				{
					bool isShadowCamera = false;
					light->GetShadowDepthCameras()->Enumerate<Camera>([&](Camera *camera, size_t index, bool &stop) {
						if (renderPass.framebuffer == camera->GetRenderPass()->GetFramebuffer())
						{
							stop = true;
							isShadowCamera = true;
						}
					});

					if(!isShadowCamera)
					{
						renderPass.directionalShadowDepthTexture = light->GetShadowDepthTexture()->Downcast<VulkanTexture>();
					}

					renderPass.directionalShadowMatrices.clear();
					light->GetShadowDepthCameras()->Enumerate<Camera>([&](Camera *camera, size_t index, bool &stop) {
						Matrix clipSpaceCorrectionMatrix;
						clipSpaceCorrectionMatrix.m[5] = 1.0f;
						Matrix shadowMatrix = clipSpaceCorrectionMatrix * camera->GetProjectionMatrix();
						shadowMatrix = shadowMatrix * camera->GetWorldTransform().GetInverse();
						renderPass.directionalShadowMatrices.push_back(shadowMatrix);
					});

					renderPass.directionalShadowInfo = Vector2(1.0f / light->GetShadowParameters().resolution);
				}
			}
			else if(light->GetType() == Light::Type::PointLight)
			{
				renderPass.pointLights.push_back(VulkanPointLight{ light->GetWorldPosition(), light->GetRange(), light->GetFinalColor() });
			}
			else if(light->GetType() == Light::Type::SpotLight)
			{
				renderPass.spotLights.push_back(VulkanSpotLight{ light->GetWorldPosition(), light->GetRange(), light->GetForward(), light->GetAngleCos(), light->GetFinalColor() });
			}
		}

		_internals->currentRenderPassIndex = originalIndex;
	}

	void VulkanRenderer::WarmupDrawable(Mesh *mesh, Material *material, Camera *camera)
	{
		//TODO: This is all a bit simplified and won't handle everything, but hopefully catches the main use case for now
		Renderer::WarmupDrawable(mesh, material, camera);
		if(!mesh || !material || !camera || !camera->GetRenderPass()) return;

		RenderPass *renderPass = camera->GetRenderPass();
		Framebuffer *framebuffer = renderPass->GetFramebuffer();
		VulkanFramebuffer *vulkanFramebuffer = framebuffer? framebuffer->Downcast<VulkanFramebuffer>() : nullptr;
		VulkanFramebuffer *resolveFramebuffer = nullptr;
		RenderPass::Flags renderPassFlags = renderPass->GetFlags();

		//Try to find resolve frame buffer (if there is one)
		const Array *nextRenderPasses = renderPass->GetNextRenderPasses();
		nextRenderPasses->Enumerate<RenderPass>([&](RenderPass *nextPass, size_t index, bool &stop) {
			PostProcessingAPIStage *apiStage = nextPass->Downcast<PostProcessingAPIStage>();
			if(apiStage && apiStage->GetType() == PostProcessingAPIStage::Type::ResolveMSAA)
			{
				RN::Framebuffer *tempFramebuffer = apiStage->GetFramebuffer();
				resolveFramebuffer = tempFramebuffer? tempFramebuffer->Downcast<VulkanFramebuffer>() : nullptr;
				stop = true;
			}
		});

		size_t multiviewCameraCount = 0;
		const Array *multiviewCameras = camera->GetMultiviewCameras();
		if(multiviewCameras->GetCount() > 1 && GetVulkanDevice()->GetSupportsMultiview())
		{
			multiviewCameraCount = std::min(multiviewCameras->GetCount(), static_cast<size_t>(GetVulkanDevice()->GetMaxMultiviewViewCount()));
		}

		VulkanRenderPass warmupRenderPass;
		warmupRenderPass.type = VulkanRenderPass::Type::Default;
		warmupRenderPass.renderPass = renderPass;
		warmupRenderPass.previousRenderPass = nullptr;
		warmupRenderPass.previousStoredFramebuffer = nullptr;
		warmupRenderPass.framebuffer = vulkanFramebuffer;
		warmupRenderPass.resolveFramebuffer = resolveFramebuffer;
		warmupRenderPass.shaderHint = renderPass->GetShaderHint();
		warmupRenderPass.overrideMaterial = renderPass->GetOverrideMaterial();
		warmupRenderPass.subpassSignature = 0; // no subpasses in warmup
		warmupRenderPass.multiviewLayer = 0;
		warmupRenderPass.subpasses.clear();

		//TODO: Support subpasses
		_internals->stateCoordinator.GetRenderPipelineState(material, mesh, warmupRenderPass.shaderHint, warmupRenderPass.overrideMaterial, &warmupRenderPass, 0);
	}

	void VulkanRenderer::SubmitDrawable(Drawable *tdrawable)
	{
		VulkanDrawable *drawable = static_cast<VulkanDrawable *>(tdrawable);

		size_t drawableResourceIndex = _internals->currentDrawableResourceIndex;

		auto submitDrawable = [&](VulkanRenderPass &renderPass, VulkanRenderPass &renderSubPass, uint32 subpassIndex){
			if(renderSubPass.type != VulkanRenderPass::Type::Default && renderSubPass.type != VulkanRenderPass::Type::Convert)
			{
				_internals->currentDrawableResourceIndex += 1;
				return;
			}
			// Post processing passes should only render the injected fullscreen quad.
			if((renderSubPass.type == VulkanRenderPass::Type::Convert || renderSubPass.renderPass->IsKindOfClass(PostProcessingStage::GetMetaClass())) && drawable != _defaultPostProcessingDrawable)
			{
				_internals->currentDrawableResourceIndex += 1;
				return;
			}
			if((drawable->renderGroup & renderSubPass.renderPass->GetRenderGroupMask()) == 0)
			{
				_internals->currentDrawableResourceIndex += 1;
				return;
			}

			drawable->AddCameraSpecificsIfNeeded(_internals->currentDrawableResourceIndex);
			auto &cameraSpecifics = drawable->_cameraSpecifics[_internals->currentDrawableResourceIndex];

			Material *material = drawable->GetSourceMaterial();
			if(cameraSpecifics.dirty || cameraSpecifics.camera != renderPass.cameraInfo.camera)
			{
				//TODO: Fix the camera situation...
				const VulkanPipelineState *pipelineState = _internals->stateCoordinator.GetRenderPipelineState(material, drawable->GetSourceMesh(), renderSubPass.shaderHint, renderSubPass.overrideMaterial, &renderPass, subpassIndex);
				VulkanUniformState *uniformState = _internals->stateCoordinator.GetUniformStateForPipelineState(pipelineState);

				RN_ASSERT(pipelineState && uniformState, "Failed to create pipeline or uniform state for drawable!");
				drawable->UpdateRenderingState(_internals->currentDrawableResourceIndex, renderPass.cameraInfo.camera, pipelineState, uniformState);

				if(cameraSpecifics.descriptorSet)
				{
					cameraSpecifics.descriptorSet->SetLayout(pipelineState->rootSignature->descriptorSetLayout);
				}
			}

			//Vertex and fragment shaders need to explicitly be marked to support instancing in the shader library json
			RN::Shader *vertexShader = cameraSpecifics.pipelineState->descriptor.vertexShader;
			RN::Shader *fragmentShader = cameraSpecifics.pipelineState->descriptor.fragmentShader;
			bool canUseInstancing = (!vertexShader || vertexShader->GetHasInstancing()) && (!fragmentShader || fragmentShader->GetHasInstancing());

			auto *vertexConstantBuffers = cameraSpecifics.uniformState->vertexConstantBuffers.data();
			auto *fragmentConstantBuffers = cameraSpecifics.uniformState->fragmentConstantBuffers.data();
			size_t vertexConstantBuffersCount = cameraSpecifics.uniformState->vertexConstantBuffers.size();
			size_t fragmentConstantBuffersCount = cameraSpecifics.uniformState->fragmentConstantBuffers.size();

			//TODO: Use binding and type arrays in vulkan root signatures pipeline layout instead
			//Check if uniform buffers are the same, the object can't be part of the same instanced draw call if it doesn't share the same buffers (because they are full for example)
			if(canUseInstancing && renderSubPass.currentInstanceDrawable && vertexConstantBuffersCount == renderSubPass.currentInstanceDrawable->_cameraSpecifics[_internals->currentDrawableResourceIndex].uniformState->vertexConstantBuffers.size() && fragmentConstantBuffersCount == renderSubPass.currentInstanceDrawable->_cameraSpecifics[_internals->currentDrawableResourceIndex].uniformState->fragmentConstantBuffers.size())
			{
				auto &instanceCameraSpecifics = renderSubPass.currentInstanceDrawable->_cameraSpecifics[_internals->currentDrawableResourceIndex];

				canUseInstancing = true;
				for(int i = 0; i < vertexConstantBuffersCount && canUseInstancing; i++)
				{
					if(vertexConstantBuffers[i]->dynamicBuffer != instanceCameraSpecifics.uniformState->vertexConstantBuffers[i]->dynamicBuffer)
					{
						canUseInstancing = false;
					}
				}

				for(int i = 0; i < fragmentConstantBuffersCount && canUseInstancing; i++)
				{
					if(fragmentConstantBuffers[i]->dynamicBuffer != instanceCameraSpecifics.uniformState->fragmentConstantBuffers[i]->dynamicBuffer)
					{
						canUseInstancing = false;
					}
				}
			}

			if(canUseInstancing && renderSubPass.instanceSteps.size() > 0 && renderSubPass.instanceSteps.back() >= std::min(vertexShader->GetMaxInstanceCount(), fragmentShader? fragmentShader->GetMaxInstanceCount() : -1))
			{
				canUseInstancing = false;
			}

			if(canUseInstancing && renderSubPass.currentPipelineState == cameraSpecifics.pipelineState && drawable->GetSourceMesh() == renderSubPass.currentInstanceDrawable->GetSourceMesh() && drawable->material.GetTextures()->IsEqualLite(renderSubPass.currentInstanceDrawable->material.GetTextures()))
			{
				renderSubPass.instanceSteps.back() += 1; //Increase counter if the rendering state is the same
			}
			else
			{
				renderSubPass.currentPipelineState = cameraSpecifics.pipelineState;
				renderSubPass.currentInstanceDrawable = drawable;
				renderSubPass.instanceSteps.push_back(1); //Add new entry if the rendering state changed
				_frameStatistics.back().numberOfDrawCalls += 1;

				//This stuff should only be needed per draw call and not for any additional instances... hopefully
				if(!cameraSpecifics.descriptorSet)
				{
					cameraSpecifics.descriptorSet = new VulkanTransientDescriptorSet();
					cameraSpecifics.descriptorSet->SetLayout(cameraSpecifics.pipelineState->rootSignature->descriptorSetLayout);
				}
				cameraSpecifics.descriptorSet->Allocate(this);
				_internals->totalDescriptorTables += cameraSpecifics.pipelineState->rootSignature->textureCount;
				_internals->totalDescriptorTables += cameraSpecifics.pipelineState->rootSignature->constantBufferCount;
			}

			// Push into the queue
			renderSubPass.drawables.push_back(drawable);
			_internals->currentDrawableResourceIndex += 1;

			_frameStatistics.back().numberOfDrawables += 1;
			_frameStatistics.back().numberOfVertices += drawable->mesh.GetVerticesCount();
			_frameStatistics.back().numberOfIndices += drawable->mesh.GetIndicesCount();
		};

		for(size_t pi = _internals->currentRenderPassIndex; pi < _internals->renderPasses.size(); pi++)
		{
			VulkanRenderPass &renderPass = _internals->renderPasses[pi];
			if(renderPass.subpasses.size() > 0)
			{
				size_t subpassIndex = 0;
				for(VulkanRenderPass &renderSubPass : renderPass.subpasses)
				{
					submitDrawable(renderPass, renderSubPass, subpassIndex++);
				}
			}
			else
			{
				submitDrawable(renderPass, renderPass, 0);
			}
		}
		_internals->currentDrawableResourceIndex = drawableResourceIndex;
	}

	void VulkanRenderer::UpdateDescriptorSets()
	{
		RN_PROFILE_SCOPE();
		_internals->currentRenderPassIndex = 0;
		_internals->currentDrawableResourceIndex = 0;

		uint32 totalConstantBufferCount = 0;
		uint32 totalTextureCount = 0;
		uint32 totalSubpassInputCount = 0;

		for(const VulkanRenderPass &renderPass : _internals->renderPasses)
		{
			RN_PROFILE_SCOPE();
			if(renderPass.type != VulkanRenderPass::Type::Default && renderPass.type != VulkanRenderPass::Type::Convert)
			{
				_internals->currentRenderPassIndex += 1;
				_internals->currentDrawableResourceIndex += 1;
				continue;
			}

			if(renderPass.subpasses.size() > 0)
			{
				for(const VulkanRenderPass &subpass : renderPass.subpasses)
				{
					if(subpass.drawables.size() > 0)
					{
						uint32 stepSize = 0;
						uint32 stepSizeIndex = 0;
						for(size_t i = 0; i < subpass.drawables.size(); i+= stepSize)
						{
							stepSize = subpass.instanceSteps[stepSizeIndex++];
		
							const auto &spec= subpass.drawables[i]->_cameraSpecifics[_internals->currentDrawableResourceIndex];
							const VulkanUniformState *uniformState = spec.uniformState;
							const VulkanPipelineState *pipelineState = spec.pipelineState;
		
							totalConstantBufferCount += uniformState->vertexConstantBuffers.size();
							totalConstantBufferCount += uniformState->fragmentConstantBuffers.size();

							if(renderPass.cameraInfo.lightManager) totalConstantBufferCount += 4;
		
							totalTextureCount += pipelineState->rootSignature->textureCount;
							totalSubpassInputCount += pipelineState->rootSignature->subpassInputCount;
						}
					}
					_internals->currentDrawableResourceIndex += 1;
				}
			}
			else
			{
				if(renderPass.drawables.size() > 0)
				{
					uint32 stepSize = 0;
					uint32 stepSizeIndex = 0;
					for(size_t i = 0; i < renderPass.drawables.size(); i+= stepSize)
					{
						stepSize = renderPass.instanceSteps[stepSizeIndex++];

						const auto &spec= renderPass.drawables[i]->_cameraSpecifics[_internals->currentDrawableResourceIndex];
						const VulkanUniformState *uniformState = spec.uniformState;
						const VulkanPipelineState *pipelineState = spec.pipelineState;

						totalConstantBufferCount += uniformState->vertexConstantBuffers.size();
						totalConstantBufferCount += uniformState->fragmentConstantBuffers.size();

						if(renderPass.cameraInfo.lightManager) totalConstantBufferCount += 4;

						totalTextureCount += pipelineState->rootSignature->textureCount;
					}
				}

				_internals->currentDrawableResourceIndex += 1;
			}

			_internals->currentRenderPassIndex += 1;
		}

		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.reserve(totalConstantBufferCount + totalTextureCount);
		std::vector<VkDescriptorBufferInfo> constantBufferDescriptorInfoArray;
		constantBufferDescriptorInfoArray.reserve(totalConstantBufferCount);
		std::vector<VkDescriptorImageInfo> imageBufferDescriptorInfoArray;
		imageBufferDescriptorInfoArray.reserve(totalTextureCount);
		std::vector<VkDescriptorImageInfo> subpassInputDescriptorInfoArray;
		subpassInputDescriptorInfoArray.reserve(totalSubpassInputCount);

		_internals->currentRenderPassIndex = 0;
		_internals->currentDrawableResourceIndex = 0;

		auto updateDescriptorSets = [&](const VulkanRenderPass &renderPass, VulkanRenderPass &rootRenderPass){
			if(renderPass.drawables.size() > 0)
			{
				std::vector<uint32> subpassInputColorIndices;
				bool subpassReadsDepthStencilAttachment = false;
				VulkanFramebuffer *rootFramebuffer = rootRenderPass.framebuffer;
				std::vector<VkImageView> subpassInputColorViews;
				VkImageView subpassInputDepthView = VK_NULL_HANDLE;

				if(rootFramebuffer && rootRenderPass.subpasses.size() > 0)
				{
					RenderPass *subpassRP = renderPass.renderPass;
					uint32 totalColorAttachments = rootFramebuffer->_swapChain ? 1 : static_cast<uint32>(rootFramebuffer->_colorTargets.size());
					for(uint32 ci = 0; ci < totalColorAttachments; ci++)
					{
						if(subpassRP->GetSubpassReadColorAttachment(ci))
						{
							subpassInputColorIndices.push_back(ci);
						}
					}
					subpassReadsDepthStencilAttachment = subpassRP->GetSubpassReadDepthStencilAttachment();

					if(subpassInputColorIndices.size() > 0)
					{
						subpassInputColorViews.reserve(subpassInputColorIndices.size());
						for(uint32 colorIndex : subpassInputColorIndices)
						{
							Texture *texture = rootFramebuffer->GetColorTexture(colorIndex);
							VulkanTexture *framebufferTexture = texture ? texture->Downcast<VulkanTexture>() : nullptr;
							subpassInputColorViews.push_back(framebufferTexture ? framebufferTexture->_imageView : VK_NULL_HANDLE);
						}
					}

					if(subpassReadsDepthStencilAttachment)
					{
						Texture *depthTexture = rootFramebuffer->GetDepthStencilTexture();
						VulkanTexture *depthFramebufferTexture = depthTexture ? depthTexture->Downcast<VulkanTexture>() : nullptr;
						subpassInputDepthView = depthFramebufferTexture ? depthFramebufferTexture->_imageView : VK_NULL_HANDLE;
					}
				}
				
				VkImageView previousPassColorView = VK_NULL_HANDLE;
				if(rootRenderPass.previousStoredFramebuffer)
				{
					VulkanFramebuffer *previousFramebuffer = rootRenderPass.previousStoredFramebuffer;
					if(previousFramebuffer)
					{
						Texture *previousColorTexture = previousFramebuffer->GetColorTexture();
						if(previousColorTexture)
						{
							VulkanTexture *previousColorVulkanTexture = previousColorTexture->Downcast<VulkanTexture>();
							previousPassColorView = previousColorVulkanTexture ? previousColorVulkanTexture->_imageView : VK_NULL_HANDLE;
						}
					}
				}

				size_t stepSize = 0;
				uint32 stepSizeIndex = 0;
				for(size_t i = 0; i < renderPass.drawables.size(); i+= stepSize)
				{
					stepSize = renderPass.instanceSteps[stepSizeIndex++];

					const size_t drawableResourceIndex = _internals->currentDrawableResourceIndex;
					VulkanDrawable *drawable = renderPass.drawables[i];
					VulkanDrawable::CameraSpecific &cameraSpecific = drawable->_cameraSpecifics[drawableResourceIndex];

					const VulkanPipelineState *pipelineState = cameraSpecific.pipelineState;

					VkDescriptorSet descriptorSet = cameraSpecific.descriptorSet->GetActiveDescriptorSet();

					VulkanUniformState *uniformState = cameraSpecific.uniformState;
					if(uniformState->instanceAttributesBuffer)
					{
						//These are not actually part of the descripter sets, but filling them with data here anyway
						Shader::ArgumentBuffer *argument = uniformState->instanceAttributesArgumentBuffer;
						const size_t maxInstanceCount = argument->GetMaxInstanceCount();
						const size_t instanceCount = (maxInstanceCount == 0)? stepSize : std::min(stepSize, maxInstanceCount);

						//Setup per instance uniforms as vertex data for all instances that are part of this draw call
						for(size_t instance = 0; instance < instanceCount; instance += 1)
						{
							VulkanDrawable *instanceDrawable = renderPass.drawables[i + instance];
							VulkanUniformState *instanceUniformState = instanceDrawable->_cameraSpecifics[drawableResourceIndex].uniformState;
							VulkanDynamicBufferReference *instanceAttributesBuffer = instanceUniformState->instanceAttributesBuffer;
							_dynamicBufferPool->UpdateDynamicBufferReference(instanceAttributesBuffer, instance == 0);
							FillUniformBuffer(argument, instanceAttributesBuffer, instanceDrawable);
						}
					}

					size_t counter = 0;
					for(size_t bufferIndex = 0; bufferIndex < uniformState->vertexConstantBuffers.size(); bufferIndex += 1)
					{
						Shader::ArgumentBuffer *argument = uniformState->constantBufferToArgumentMapping[counter++];
						const size_t maxInstanceCount = argument->GetMaxInstanceCount();
						const size_t instanceCount = (maxInstanceCount == 0)? stepSize : std::min(stepSize, maxInstanceCount);

						//Setup uniforms for all instances that are part of this draw call
						for(size_t instance = 0; instance < instanceCount; instance += 1)
						{
							VulkanDrawable *instanceDrawable = renderPass.drawables[i + instance];
							VulkanUniformState *instanceUniformState = instanceDrawable->_cameraSpecifics[drawableResourceIndex].uniformState;
							_dynamicBufferPool->UpdateDynamicBufferReference(instanceUniformState->vertexConstantBuffers[bufferIndex], instance == 0);
							FillUniformBuffer(argument, instanceUniformState->vertexConstantBuffers[bufferIndex], instanceDrawable);
						}

						VulkanDynamicBufferReference *constantBuffer = uniformState->vertexConstantBuffers[bufferIndex];

						GPUBuffer *gpuBuffer = constantBuffer->dynamicBuffer->GetActiveGPUBuffer();
						VkDescriptorBufferInfo constantBufferDescriptorInfo = {};
						constantBufferDescriptorInfo.buffer = gpuBuffer->Downcast<VulkanGPUBuffer>()->GetVulkanBuffer();
						constantBufferDescriptorInfo.offset = constantBuffer->offset;
						constantBufferDescriptorInfo.range = constantBuffer->size * instanceCount;
						constantBufferDescriptorInfoArray.push_back(constantBufferDescriptorInfo);

						VkWriteDescriptorSet writeConstantDescriptorSet = {};
						writeConstantDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
						writeConstantDescriptorSet.pNext = NULL;
						writeConstantDescriptorSet.dstSet = descriptorSet;
						writeConstantDescriptorSet.descriptorType = (argument->GetType() == Shader::ArgumentBuffer::Type::UniformBuffer)? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						writeConstantDescriptorSet.dstBinding = argument->GetIndex();
						writeConstantDescriptorSet.pBufferInfo = &constantBufferDescriptorInfoArray[constantBufferDescriptorInfoArray.size()-1];
						writeConstantDescriptorSet.descriptorCount = 1;

						writeDescriptorSets.push_back(writeConstantDescriptorSet);
					}

					for(size_t bufferIndex = 0; bufferIndex < uniformState->fragmentConstantBuffers.size(); bufferIndex += 1)
					{
						Shader::ArgumentBuffer *argument = uniformState->constantBufferToArgumentMapping[counter++];
						const size_t maxInstanceCount = argument->GetMaxInstanceCount();
						const size_t instanceCount = (maxInstanceCount == 0)? stepSize : std::min(stepSize, maxInstanceCount);

						//Setup uniforms for all instances that are part of this draw call
						for(size_t instance = 0; instance < instanceCount; instance += 1)
						{
							VulkanDrawable *instanceDrawable = renderPass.drawables[i + instance];
							VulkanUniformState *instanceUniformState = instanceDrawable->_cameraSpecifics[drawableResourceIndex].uniformState;
							_dynamicBufferPool->UpdateDynamicBufferReference(
									instanceUniformState->fragmentConstantBuffers[bufferIndex],
									instance == 0);
							FillUniformBuffer(argument, instanceUniformState->fragmentConstantBuffers[bufferIndex], instanceDrawable);
						}

						VulkanDynamicBufferReference *constantBuffer = uniformState->fragmentConstantBuffers[bufferIndex];

						GPUBuffer *gpuBuffer = constantBuffer->dynamicBuffer->GetActiveGPUBuffer();
						VkDescriptorBufferInfo constantBufferDescriptorInfo = {};
						constantBufferDescriptorInfo.buffer = gpuBuffer->Downcast<VulkanGPUBuffer>()->GetVulkanBuffer();
						constantBufferDescriptorInfo.offset = constantBuffer->offset;
						constantBufferDescriptorInfo.range = constantBuffer->size * instanceCount;
						constantBufferDescriptorInfoArray.push_back(constantBufferDescriptorInfo);

						VkWriteDescriptorSet writeConstantDescriptorSet = {};
						writeConstantDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
						writeConstantDescriptorSet.pNext = NULL;
						writeConstantDescriptorSet.dstSet = descriptorSet;
						writeConstantDescriptorSet.descriptorType = (argument->GetType() == Shader::ArgumentBuffer::Type::UniformBuffer)? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						writeConstantDescriptorSet.dstBinding = argument->GetIndex();
						writeConstantDescriptorSet.pBufferInfo = &constantBufferDescriptorInfoArray[constantBufferDescriptorInfoArray.size()-1];
						writeConstantDescriptorSet.descriptorCount = 1;

						writeDescriptorSets.push_back(writeConstantDescriptorSet);
					}

					// Bind LightManager buffers by semantic for forward rendering
					if(renderPass.cameraInfo.lightManager)
					{
						const Shader::Signature *signature = pipelineState->descriptor.fragmentShader ? pipelineState->descriptor.fragmentShader->GetSignature() : nullptr;
						if(signature)
						{
							signature->GetBuffers()->Enumerate<Shader::ArgumentBuffer>([&](Shader::ArgumentBuffer *argument, size_t index, bool &stop) {
								VkDescriptorBufferInfo bufferInfo = {};
								VkWriteDescriptorSet write = {};
								write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
								write.pNext = NULL;
								write.dstSet = descriptorSet;
								write.dstBinding = argument->GetIndex();
								write.descriptorCount = 1;

								switch(argument->GetSemantic())
								{
									case Shader::ArgumentBuffer::Semantic::LightClusterPointLights:
									{
										GPUBuffer *pointlightBuffer = renderPass.cameraInfo.lightManager->GetPointLightBuffer();
										if(pointlightBuffer)
										{
											bufferInfo.buffer = pointlightBuffer->Downcast<VulkanDynamicGPUBuffer>()->GetVulkanBuffer();
											bufferInfo.offset = 0;
											bufferInfo.range = pointlightBuffer->GetLength();
											constantBufferDescriptorInfoArray.push_back(bufferInfo);
											write.pBufferInfo = &constantBufferDescriptorInfoArray.back();
											write.descriptorType = (argument->GetType() == Shader::ArgumentBuffer::Type::UniformBuffer)? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
											writeDescriptorSets.push_back(write);
										}
										break;
									}
									case Shader::ArgumentBuffer::Semantic::LightClusterSpotLights:
									{
										GPUBuffer *spotlightBuffer = renderPass.cameraInfo.lightManager->GetSpotLightBuffer();
										if(spotlightBuffer)
										{
											bufferInfo.buffer = spotlightBuffer->Downcast<VulkanDynamicGPUBuffer>()->GetVulkanBuffer();
											bufferInfo.offset = 0;
											bufferInfo.range = spotlightBuffer->GetLength();
											constantBufferDescriptorInfoArray.push_back(bufferInfo);
											write.pBufferInfo = &constantBufferDescriptorInfoArray.back();
											write.descriptorType = (argument->GetType() == Shader::ArgumentBuffer::Type::UniformBuffer)? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
											writeDescriptorSets.push_back(write);
										}
										break;
									}
									case Shader::ArgumentBuffer::Semantic::LightClusterRecords:
									{
										GPUBuffer *clusterRecordsBuffer = renderPass.cameraInfo.lightManager->GetClusterRecordsBuffer();
										if(clusterRecordsBuffer)
										{
											bufferInfo.buffer = clusterRecordsBuffer->Downcast<VulkanDynamicGPUBuffer>()->GetVulkanBuffer();
											bufferInfo.offset = 0;
											bufferInfo.range = clusterRecordsBuffer->GetLength();
											constantBufferDescriptorInfoArray.push_back(bufferInfo);
											write.pBufferInfo = &constantBufferDescriptorInfoArray.back();
											write.descriptorType = (argument->GetType() == Shader::ArgumentBuffer::Type::UniformBuffer)? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
											writeDescriptorSets.push_back(write);
										}
										break;
									}
									case Shader::ArgumentBuffer::Semantic::LightClusterIndices:
									{
										GPUBuffer *clusterIndexBuffer = renderPass.cameraInfo.lightManager->GetClusterIndexBuffer();
										if(clusterIndexBuffer)
										{
											bufferInfo.buffer = clusterIndexBuffer->Downcast<VulkanDynamicGPUBuffer>()->GetVulkanBuffer();
											bufferInfo.offset = 0;
											bufferInfo.range = clusterIndexBuffer->GetLength();
											constantBufferDescriptorInfoArray.push_back(bufferInfo);
											write.pBufferInfo = &constantBufferDescriptorInfoArray.back();
											write.descriptorType = (argument->GetType() == Shader::ArgumentBuffer::Type::UniformBuffer)? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
											writeDescriptorSets.push_back(write);
										}
										break;
									}
									default: break;
								}
							});
						}
					}

					//TODO: Support vertex shader textures
					Shader *fragmentShader = pipelineState->descriptor.fragmentShader;
					if(fragmentShader)
					{
						const Shader::Signature *signature = fragmentShader->GetSignature();

						signature->GetSubpassInputs()->Enumerate<Shader::ArgumentTexture>([&](Shader::ArgumentTexture *argument, size_t index, bool &stop) {
							uint8 materialTextureIndex = argument->GetMaterialTextureIndex();
							bool isDepthInput = materialTextureIndex >= 128;
							materialTextureIndex = isDepthInput ? materialTextureIndex - 128 : materialTextureIndex;

							VkImageView imageView = VK_NULL_HANDLE;

							if(isDepthInput)
							{
								// Ensure this subpass reads depth
								if(!subpassReadsDepthStencilAttachment || !subpassInputDepthView)
								{
									stop = true;
									return;
								}

								imageView = subpassInputDepthView;
							}
							else
							{
								// Map color input ordinal to actual color attachment index via cached views
								if(materialTextureIndex >= subpassInputColorViews.size())
								{
									stop = true;
									return;
								}
								imageView = subpassInputColorViews[materialTextureIndex];
								if(imageView == VK_NULL_HANDLE)
								{
									stop = true;
									return;
								}
							}

							VkDescriptorImageInfo inputAttachmentDescriptorInfo = {};
							inputAttachmentDescriptorInfo.imageLayout = isDepthInput ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							inputAttachmentDescriptorInfo.imageView = imageView;
							subpassInputDescriptorInfoArray.push_back(inputAttachmentDescriptorInfo);

							VkWriteDescriptorSet writeImageDescriptorSet = {};
							writeImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
							writeImageDescriptorSet.pNext = NULL;
							writeImageDescriptorSet.dstSet = descriptorSet;
							writeImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
							writeImageDescriptorSet.dstBinding = argument->GetIndex();
							writeImageDescriptorSet.pImageInfo = &subpassInputDescriptorInfoArray[subpassInputDescriptorInfoArray.size() - 1];
							writeImageDescriptorSet.descriptorCount = 1;

							writeDescriptorSets.push_back(writeImageDescriptorSet);
						});

						signature->GetTextures()->Enumerate<Shader::ArgumentTexture>([&](Shader::ArgumentTexture *argument, size_t index, bool &stop) {

							VkImageView imageView = VK_NULL_HANDLE;
							if(argument->GetMaterialTextureIndex() == Shader::ArgumentTexture::IndexDirectionalShadowTexture && rootRenderPass.directionalShadowDepthTexture)
							{
								const VulkanTexture *materialTexture = rootRenderPass.directionalShadowDepthTexture;
								imageView = materialTexture->_imageView;
							}
							else if(argument->GetMaterialTextureIndex() == Shader::ArgumentTexture::IndexFramebufferTexture)
							{
								if(!previousPassColorView)
								{
									return;
								}
								imageView = previousPassColorView;
							}
							else if(argument->GetMaterialTextureIndex() >= drawable->material.GetTextures()->GetCount())
							{
								stop = true;
								return;
							}
							else
							{
								Object *textureObject = drawable->material.GetTextures()->GetObjectAtIndex(argument->GetMaterialTextureIndex());

								VulkanTexture *materialTexture = nullptr;
								if(textureObject->IsKindOfClass(VulkanTexture::GetMetaClass()))
								{
									materialTexture = static_cast<VulkanTexture*>(textureObject);
								}
								else
								{
									VulkanFramebuffer *framebuffer = static_cast<VulkanFramebuffer*>(textureObject);
									// Prevent binding the current framebuffer as a sampled texture
									if(rootFramebuffer && framebuffer == rootFramebuffer)
									{
										return; // skip this texture argument
									}
									size_t textureIndex = 0;
									if(framebuffer->GetSwapChain()) textureIndex = framebuffer->GetSwapChain()->GetFrameIndex();
									materialTexture = framebuffer->GetColorTexture(textureIndex)->Downcast<VulkanTexture>();
								}

								imageView = materialTexture->_imageView;

								if(materialTexture->GetDescriptor().usageHint & Texture::UsageHint::RenderTarget)
								{
									//Add render targets to list of textures that needs to be transitioned for this render pass
									rootRenderPass.renderTargetsUsedInShader.push_back(materialTexture);
								}
							}

							VkDescriptorImageInfo imageBufferDescriptorInfo = {};
							imageBufferDescriptorInfo.imageView = imageView;
							imageBufferDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							imageBufferDescriptorInfoArray.push_back(imageBufferDescriptorInfo);

							VkWriteDescriptorSet writeImageDescriptorSet = {};
							writeImageDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
							writeImageDescriptorSet.pNext = NULL;
							writeImageDescriptorSet.dstSet = descriptorSet;
							writeImageDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
							writeImageDescriptorSet.dstBinding = argument->GetIndex();
							writeImageDescriptorSet.pImageInfo = &imageBufferDescriptorInfoArray[imageBufferDescriptorInfoArray.size() - 1];
							writeImageDescriptorSet.descriptorCount = 1;

							writeDescriptorSets.push_back(writeImageDescriptorSet);
						});
					}
				}
			}
			_internals->currentDrawableResourceIndex += 1;
		};

		for(VulkanRenderPass &renderPass : _internals->renderPasses)
		{
			RN_PROFILE_SCOPE();
			renderPass.renderTargetsUsedInShader.clear();

			if(renderPass.type != VulkanRenderPass::Type::Default && renderPass.type != VulkanRenderPass::Type::Convert)
			{
				_internals->currentRenderPassIndex += 1;
				_internals->currentDrawableResourceIndex += 1;
				continue;
			}

			if(renderPass.subpasses.size() > 0)
			{
				for(VulkanRenderPass &subpass : renderPass.subpasses)
				{
					updateDescriptorSets(subpass, renderPass);
				}
			}
			else
			{
				updateDescriptorSets(renderPass, renderPass);
			}

			_internals->currentRenderPassIndex += 1;
		}

		if(writeDescriptorSets.size() > 0)
		{
			vk::UpdateDescriptorSets(GetVulkanDevice()->GetDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
		}
	}

	void VulkanRenderer::RenderDrawable(VkCommandBuffer commandBuffer, VulkanDrawable *drawable, uint32 instanceCount)
	{
		RN_PROFILE_VULKAN_SCOPE_CMD_N(_internals->tracyVulkanCtx, commandBuffer, "Draw");

		VulkanDrawable::CameraSpecific &cameraSpecific = drawable->_cameraSpecifics[_internals->currentDrawableResourceIndex];
		const VulkanPipelineState *pipelineState = cameraSpecific.pipelineState;
		const VulkanUniformState *uniformState = cameraSpecific.uniformState;
		const VulkanRootSignature *rootSignature = pipelineState->rootSignature;

		VkDescriptorSet descriptorSet = cameraSpecific.descriptorSet->GetActiveDescriptorSet();
		if(_internals->drawBindStateCache.pipelineLayout != rootSignature->pipelineLayout || _internals->drawBindStateCache.descriptorSet != descriptorSet)
		{
			vk::CmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rootSignature->pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			_internals->drawBindStateCache.pipelineLayout = rootSignature->pipelineLayout;
			_internals->drawBindStateCache.descriptorSet = descriptorSet;
		}
		if(_internals->drawBindStateCache.pipeline != pipelineState->state)
		{
			vk::CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineState->state);
			_internals->drawBindStateCache.pipeline = pipelineState->state;
		}

		VulkanGPUBuffer *buffer = static_cast<VulkanGPUBuffer *>(drawable->mesh.GetVertexBuffer());
		VulkanGPUBuffer *indices = static_cast<VulkanGPUBuffer *>(drawable->mesh.GetIndicesBuffer());
		VulkanGPUBuffer *instanceAttributesBuffer = uniformState->instanceAttributesBuffer? static_cast<VulkanGPUBuffer *>(uniformState->instanceAttributesBuffer->dynamicBuffer->GetActiveGPUBuffer()) : nullptr;

		//IF positions are separated, they will be in the first part of the buffer, everything else will be bound as the second binding, per instance data if provided through attributes are bound as a third buffer
		VkDeviceSize offsets[3];
		VkBuffer vertexBuffers[3];
		int attributesBufferIndex = 0;

		offsets[attributesBufferIndex] = 0;
		vertexBuffers[attributesBufferIndex++] = buffer->GetVulkanBuffer();

		if(pipelineState->vertexAttributeBufferCount > 1)
		{
			offsets[attributesBufferIndex] = drawable->mesh.GetVertexPositionsSeparatedSize();
			vertexBuffers[attributesBufferIndex++] = buffer->GetVulkanBuffer();
		}
		if(instanceAttributesBuffer)
		{
			offsets[attributesBufferIndex] = uniformState->instanceAttributesBuffer->offset;
			vertexBuffers[attributesBufferIndex++] = instanceAttributesBuffer->GetVulkanBuffer();
		}

		bool needsVertexBufferBind = _internals->drawBindStateCache.vertexBufferCount != attributesBufferIndex;
		for(int i = 0; i < attributesBufferIndex && !needsVertexBufferBind; i++)
		{
			needsVertexBufferBind = (_internals->drawBindStateCache.vertexBuffers[i] != vertexBuffers[i] || _internals->drawBindStateCache.vertexOffsets[i] != offsets[i]);
		}
		if(needsVertexBufferBind)
		{
			vk::CmdBindVertexBuffers(commandBuffer, 0, attributesBufferIndex, vertexBuffers, offsets);
			_internals->drawBindStateCache.vertexBufferCount = attributesBufferIndex;
			for(int i = 0; i < attributesBufferIndex; i++)
			{
				_internals->drawBindStateCache.vertexBuffers[i] = vertexBuffers[i];
				_internals->drawBindStateCache.vertexOffsets[i] = offsets[i];
			}
		}
		if(drawable->mesh.GetIndicesCount() > 0)
		{
			VkBuffer indexBuffer = indices->GetVulkanBuffer();
			VkIndexType indexType = drawable->mesh.GetIndexType() == PrimitiveType::Uint16? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
			if(!_internals->drawBindStateCache.hasIndexBufferBinding || _internals->drawBindStateCache.indexBuffer != indexBuffer || _internals->drawBindStateCache.indexOffset != 0 || _internals->drawBindStateCache.indexType != indexType)
			{
				// Bind mesh index buffer
				vk::CmdBindIndexBuffer(commandBuffer, indexBuffer, 0, indexType);
				_internals->drawBindStateCache.hasIndexBufferBinding = true;
				_internals->drawBindStateCache.indexBuffer = indexBuffer;
				_internals->drawBindStateCache.indexOffset = 0;
				_internals->drawBindStateCache.indexType = indexType;
			}
			// Render mesh vertex buffer using it's indices
			vk::CmdDrawIndexed(commandBuffer, drawable->mesh.GetIndicesCount(), instanceCount, 0, 0, 0);
		}
		else
		{
			vk::CmdDraw(commandBuffer, drawable->mesh.GetVerticesCount(), instanceCount, 0, 0);
		}

		_currentDrawableIndex += 1;
	}

	void VulkanRenderer::RenderAPIRenderPass(VulkanCommandBuffer *commandList, const VulkanRenderPass &renderPass)
	{
		//TODO: Handle multiple and not existing textures
	/*		Texture *sourceColorTexture = renderPass.previousRenderPass->GetFramebuffer()->GetColorTexture(0);
		VulkanTexture *sourceD3DColorTexture = nullptr;
		D3D12_RESOURCE_STATES oldColorSourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		if(sourceColorTexture)
		{
			sourceD3DColorTexture = sourceColorTexture->Downcast<D3D12Texture>();
			oldColorSourceState = sourceD3DColorTexture->_currentState;
		}

		Texture *sourceDepthTexture = renderPass.previousRenderPass->GetFramebuffer()->GetDepthStencilTexture();
		D3D12Texture *sourceD3DDepthTexture = nullptr;
		D3D12_RESOURCE_STATES oldDepthSourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		if (sourceColorTexture)
		{
			sourceD3DDepthTexture = sourceDepthTexture->Downcast<D3D12Texture>();
			oldDepthSourceState = sourceD3DDepthTexture->_currentState;
		}

		D3D12Framebuffer *destinationFramebuffer = renderPass.renderPass->GetFramebuffer()->Downcast<RN::D3D12Framebuffer>();

		Texture *destinationColorTexture = destinationFramebuffer->GetColorTexture(0);
		D3D12Texture *destinationD3DColorTexture = nullptr;
		ID3D12Resource *destinationColorResource = nullptr;
		D3D12_RESOURCE_STATES oldColorDestinationState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		DXGI_FORMAT targetColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		if(destinationColorTexture)
		{
			destinationD3DColorTexture = destinationColorTexture->Downcast<D3D12Texture>();
			targetColorFormat = destinationD3DColorTexture->_srvDescriptor.Format;
			oldColorDestinationState = destinationD3DColorTexture->_currentState;
			destinationColorResource = destinationD3DColorTexture->_resource;
		}
		else
		{
			targetColorFormat = destinationFramebuffer->_colorTargets[0]->d3dTargetViewDesc.Format;
			destinationColorResource = destinationFramebuffer->GetSwapChainColorBuffer();
		}


		Texture *destinationDepthTexture = destinationFramebuffer->GetDepthStencilTexture();
		D3D12Texture *destinationD3DDepthTexture = nullptr;
		ID3D12Resource *destinationDepthResource = nullptr;
		D3D12_RESOURCE_STATES oldDepthDestinationState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		DXGI_FORMAT targetDepthFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		if(destinationDepthTexture)
		{
			destinationD3DDepthTexture = destinationDepthTexture->Downcast<D3D12Texture>();
			targetDepthFormat = destinationD3DDepthTexture->_srvDescriptor.Format;
			oldDepthDestinationState = destinationD3DDepthTexture->_currentState;
			destinationDepthResource = destinationD3DDepthTexture->_resource;
		}
		else if(destinationFramebuffer->GetSwapChain() && destinationFramebuffer->GetSwapChain()->HasDepthBuffer())
		{
			targetDepthFormat = destinationFramebuffer->_depthStencilTarget->d3dTargetViewDesc.Format;
			destinationDepthResource = destinationFramebuffer->GetSwapChainDepthBuffer();
		}

		switch(targetDepthFormat)
		{
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			{
				targetDepthFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			}
			case DXGI_FORMAT_D32_FLOAT:
			{
				targetDepthFormat = DXGI_FORMAT_R32_FLOAT;
				break;
			}
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			{
				targetDepthFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			}
		}

		if(renderPass.type == D3D12RenderPass::Type::ResolveMSAA)
		{
			sourceD3DColorTexture->TransitionToState(commandList, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

			if(destinationColorTexture)
			{
				destinationD3DColorTexture->TransitionToState(commandList, D3D12_RESOURCE_STATE_RESOLVE_DEST);
			}
			else
			{
				commandList->GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(destinationColorResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_DEST));
			}

			//TODO: Handle multiple subresources?
			commandList->GetCommandList()->ResolveSubresource(destinationColorResource, 0, sourceD3DColorTexture->_resource, 0, targetColorFormat);

			sourceD3DColorTexture->TransitionToState(commandList, oldColorSourceState);
			if(destinationD3DColorTexture)
			{
				destinationD3DColorTexture->TransitionToState(commandList, oldColorDestinationState);
			}
			else
			{
				commandList->GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(destinationColorResource, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET));
			}

			if(sourceD3DDepthTexture && destinationDepthResource)
			{
				sourceD3DDepthTexture->TransitionToState(commandList, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

				if(destinationDepthTexture)
				{
					destinationD3DDepthTexture->TransitionToState(commandList, D3D12_RESOURCE_STATE_RESOLVE_DEST);
				}
				else
				{
					commandList->GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(destinationDepthResource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RESOLVE_DEST));
				}

				//TODO: Handle multiple subresources?
				commandList->GetCommandList()->ResolveSubresource(destinationDepthResource, 0, sourceD3DDepthTexture->_resource, 0, targetDepthFormat);

				sourceD3DDepthTexture->TransitionToState(commandList, oldDepthSourceState);
				if(destinationD3DDepthTexture)
				{
					destinationD3DDepthTexture->TransitionToState(commandList, oldDepthDestinationState);
				}
				else
				{
					commandList->GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(destinationDepthResource, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COMMON));
				}
			}
		}
		else if(renderPass.type == D3D12RenderPass::Type::Blit)
		{
			sourceD3DColorTexture->TransitionToState(commandList, D3D12_RESOURCE_STATE_COPY_SOURCE);

			if(destinationColorTexture)
			{
				destinationD3DColorTexture->TransitionToState(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
			}
			else
			{
				commandList->GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(destinationColorResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST));
			}

			//TODO: Handle multiple subresources and 3D/Arrays?
			CD3DX12_TEXTURE_COPY_LOCATION destinationLocation(destinationColorResource, 0);
			CD3DX12_TEXTURE_COPY_LOCATION sourceLocation(sourceD3DColorTexture->_resource, 0);
			Rect frame = renderPass.renderPass->GetFrame();
			commandList->GetCommandList()->CopyTextureRegion(&destinationLocation, frame.x, frame.y, 0, &sourceLocation, nullptr);

			sourceD3DColorTexture->TransitionToState(commandList, oldColorSourceState);
			if(destinationD3DColorTexture)
			{
				destinationD3DColorTexture->TransitionToState(commandList, oldColorDestinationState);
			}
			else
			{
				commandList->GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(destinationColorResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET));
			}

			if(sourceD3DDepthTexture && destinationDepthResource)
			{
				sourceD3DDepthTexture->TransitionToState(commandList, D3D12_RESOURCE_STATE_COPY_SOURCE);

				if(destinationDepthTexture)
				{
					destinationD3DDepthTexture->TransitionToState(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
				}
				else
				{
					commandList->GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(destinationDepthResource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
				}

				//TODO: Handle multiple subresources and 3D/Arrays?
				CD3DX12_TEXTURE_COPY_LOCATION destinationLocation(destinationDepthResource, 0);
				CD3DX12_TEXTURE_COPY_LOCATION sourceLocation(sourceD3DDepthTexture->_resource, 0);
				Rect frame = renderPass.renderPass->GetFrame();
				commandList->GetCommandList()->CopyTextureRegion(&destinationLocation, frame.x, frame.y, 0, &sourceLocation, nullptr);

				sourceD3DDepthTexture->TransitionToState(commandList, oldDepthSourceState);
				if (destinationD3DDepthTexture)
				{
					destinationD3DDepthTexture->TransitionToState(commandList, oldDepthDestinationState);
				}
				else
				{
					commandList->GetCommandList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(destinationDepthResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));
				}
			}
		}*/
	}

	void VulkanRenderer::AddFrameFinishedCallback(std::function<void()> callback, size_t frameOffset)
	{
		Lock();
		_internals->frameResources.push_back({ _currentFrame + frameOffset, callback });
		Unlock();
	}
}
