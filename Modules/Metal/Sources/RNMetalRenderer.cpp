//
//  RNMetalRenderer.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#import <Metal/Metal.h>
#include "RNMetalRenderer.h"
#include "../../../Source/Scene/RNLightManager.h"
#include "RNMetalInternals.h"
#include "RNMetalShaderLibrary.h"
#include "RNMetalGPUBuffer.h"
#include "RNMetalDynamicGPUBuffer.h"
#include "RNMetalTexture.h"
#include "RNMetalUniformBuffer.h"
#include "RNMetalDevice.h"
#include "RNMetalRendererDescriptor.h"
#include "RNMetalFramebuffer.h"
#include "../../../Source/Rendering/RNShader.h"

namespace RN
{
	RNDefineMeta(MetalRenderer, Renderer)

	MetalRenderer::MetalRenderer(MetalRendererDescriptor *descriptor, MetalDevice *device) :
		Renderer(descriptor, device),
		_mipMapTextures(new Set()),
		_mainWindow(nullptr),
		_activeFrameSubmission(nullptr),
		_defaultPostProcessingDrawable(nullptr),
		_ppConvertMaterial(nullptr),
		_defaultShaderLibrary(nullptr),
		_currentMultiviewLayer(0),
		_currentMultiviewFallbackRenderPass(nullptr)
	{
		RN_PROFILE_SCOPE();
		_internals->device = device->GetDevice();
		_internals->commandQueue = [_internals->device newCommandQueue];
		_internals->stateCoordinator.SetDevice(_internals->device);

		_defaultShaderLibrary = CreateShaderLibraryWithFile(RNCSTR(":RayneMetal:/Shaders.json"));
		_uniformBufferPool = new MetalUniformBufferPool();
		StartRenderThread();
	}

	MetalRenderer::~MetalRenderer()
	{
		RN_PROFILE_SCOPE();
		StopRenderThread();
		FlushAllDeletedDrawables();

		[_internals->commandQueue release];
		[_internals->device release];

		SafeRelease(_mipMapTextures);
		SafeRelease(_defaultShaderLibrary);

		delete _uniformBufferPool;
	}


	Window *MetalRenderer::CreateAWindow(const Vector2 &size, Screen *screen, const Window::SwapChainDescriptor &descriptor, void *hwnd)
	{
		RN_PROFILE_SCOPE();
		MetalWindow *window = new MetalWindow(size, screen, this, descriptor);
		if(!_mainWindow)
			_mainWindow = window->Retain();

		return window;
	}

	Window *MetalRenderer::GetMainWindow()
	{
		RN_PROFILE_SCOPE();
		return _mainWindow;
	}

	void MetalRenderer::SetMainWindow(Window *window)
	{
		RN_PROFILE_SCOPE();
		_mainWindow = window;
	}

	id MetalRenderer::GetCommandQueue() const
	{
		RN_PROFILE_SCOPE();
		return _internals->commandQueue;
	}

	MetalFrameSubmission &MetalRenderer::GetActiveFrameSubmission()
	{
		AssertOnSubmissionThread();
		RN_ASSERT(_activeFrameSubmission, "No active Metal frame submission");
		return *_activeFrameSubmission;
	}

	Shader::UsageHint MetalRenderer::GetMetalShaderHint(Shader::UsageHint shaderHint) const
	{
		if(shaderHint == Shader::UsageHint::Multiview)
		{
			return Shader::UsageHint::Default;
		}
		if(shaderHint == Shader::UsageHint::DepthMultiview)
		{
			return Shader::UsageHint::Depth;
		}
		if(shaderHint == Shader::UsageHint::ShadowDepthMultiview)
		{
			return Shader::UsageHint::ShadowDepth;
		}

		return shaderHint;
	}


	void MetalRenderer::CreateMipMapForTexture(MetalTexture *texture)
	{
		RN_PROFILE_SCOPE();
		LockGuard<Lockable> lock(_lock);
		_mipMapTextures->AddObject(texture);
	}

	void MetalRenderer::CreateMipMaps()
	{
		RN_PROFILE_SCOPE();
		AssertOnRenderThread();
		Set *mipMapTextures = nullptr;
		{
			LockGuard<Lockable> lock(_lock);
			if(_mipMapTextures->GetCount() > 0)
			{
				mipMapTextures = new Set(_mipMapTextures);
				_mipMapTextures->RemoveAllObjects();
			}
		}

		if(!mipMapTextures)
			return;

		id<MTLCommandBuffer> commandBuffer = [_internals->commandQueue commandBuffer];

		mipMapTextures->Enumerate<MetalTexture>([&](MetalTexture *texture, bool &stop) {

			id<MTLBlitCommandEncoder> commandEncoder = [commandBuffer blitCommandEncoder];
			[commandEncoder generateMipmapsForTexture:(id<MTLTexture>)texture->__GetUnderlyingTexture()];
			[commandEncoder endEncoding];
		});

		[commandBuffer commit];

		//TODO: make async
		[commandBuffer waitUntilCompleted];

		mipMapTextures->Release();
	}


	void MetalRenderer::Render(Function &&function)
	{
		RN_PROFILE_SCOPE();
		@autoreleasepool {
			QueueFrameSubmission(std::move(function));
		}
	}

	void MetalRenderer::StartRenderThread()
	{
		_internals->renderThread = new Thread([this]() {
			while(true)
			{
				@autoreleasepool {
					AutoreleasePool pool;
					if(!ConsumeRenderThreadWork())
						return;
				}
			}
		}, false);
		_internals->renderThread->SetName(RNCSTR("RN::MetalRender"));
		_internals->renderThread->Start();
		while(!_internals->renderThread->IsRunning())
			std::this_thread::yield();
	}

	void MetalRenderer::StopRenderThread()
	{
		Thread *renderThread = _internals->renderThread;
		if(!renderThread)
			return;

		_internals->renderThreadQueue.Shutdown();
		renderThread->WaitForExit();
		_internals->renderThreadQueue.Drain();
		renderThread->Release();
		_internals->renderThread = nullptr;
	}

	void MetalRenderer::AssertOnSubmissionThread()
	{
#if RN_BUILD_DEBUG
		std::thread::id currentThread = std::this_thread::get_id();
		if(!_internals->hasSubmissionThread)
		{
			_internals->submissionThread = currentThread;
			_internals->hasSubmissionThread = true;
		}

		RN_DEBUG_ASSERT(_internals->submissionThread == currentThread, "Metal frame submission must stay on one submission thread");
		RN_DEBUG_ASSERT(!_internals->renderThread || !_internals->renderThread->OnThread(), "Metal frame submission must not run on the render thread");
#endif
	}

	bool MetalRenderer::IsOnRenderThread() const
	{
		return _internals->renderThread && _internals->renderThread->OnThread();
	}

	void MetalRenderer::AssertOnRenderThread() const
	{
#if RN_BUILD_DEBUG
		RN_DEBUG_ASSERT(IsOnRenderThread(), "Metal render work must run on the render thread");
#endif
	}

	void MetalRenderer::ScheduleRenderThreadWork(Function &&function)
	{
		if(IsOnRenderThread())
		{
			function();
			return;
		}

		_internals->renderThreadQueue.PushTask(std::move(function));
	}

	void MetalRenderer::QueueFrameSubmission(Function &&function)
	{
		RN_PROFILE_SCOPE();
		AssertOnSubmissionThread();

		MetalFrameSubmission submission;
		if(!_internals->renderThreadQueue.WaitForSpace())
			return;

		BuildFrameSubmission(submission, std::move(function));
		_internals->renderThreadQueue.Push(std::move(submission));
	}

	bool MetalRenderer::ConsumeRenderThreadWork()
	{
		RN_PROFILE_SCOPE();
		AssertOnRenderThread();

		MetalFrameSubmission submission;
		Function task;
		using WorkType = RenderThreadQueue<MetalFrameSubmission>::WorkType;
		WorkType workType = _internals->renderThreadQueue.Pop(submission, task);
		if(workType == WorkType::None)
			return false;

		if(workType == WorkType::Task)
		{
			task();
			return true;
		}

		if(!submission.renderFrame.BeginPresentationStatesOnRenderThread())
		{
			FinishRenderFrameSubmission(submission.renderFrame);
			return true;
		}

		PrepareRenderFrame(submission);
		RenderFrameSubmission(submission);
		PrintFrameStatistics(submission.renderFrame);
		FinishRenderFrameSubmission(submission.renderFrame);
		return true;
	}

	void MetalRenderer::BuildFrameSubmission(MetalFrameSubmission &submission, Function &&function)
	{
		RN_PROFILE_SCOPE();
		AssertOnSubmissionThread();

		//Submit camera is called for each camera and creates draw items per camera
		BeginRenderFrameSubmission(submission.renderFrame);
		MetalFrameSubmission *previousSubmission = _activeFrameSubmission;
		_activeFrameSubmission = &submission;
		function();
		_activeFrameSubmission = previousSubmission;
	}

	void MetalRenderer::RenderFrameSubmission(const MetalFrameSubmission &submission)
	{
		RN_PROFILE_SCOPE();
		AssertOnRenderThread();

		for(MetalSwapChain *swapChain : submission.swapChains)
		{
			//TODO: do this the first time the swap chain is actually used
			swapChain->AcquireBackBuffer();
			swapChain->Prepare();
		}

		_internals->commandBuffer = [_internals->commandQueue commandBuffer];

		for(const MetalRenderPass &renderPass : submission.renderPasses)
		{
			if(renderPass.framebuffer->GetSwapChain() && !renderPass.framebuffer->GetSwapChain()->GetMetalColorTexture())
			{
				continue;
			}

			if(!renderPass.UsesDrawItems())
			{
				RenderAPIRenderPass(submission, renderPass);
				continue;
			}

			const RenderFrame::Pass &framePass = submission.renderFrame.GetPass(renderPass.renderFramePassIndex);
			const RenderPass::DrawSnapshot &drawSnapshot = framePass.GetDrawSnapshot();
			// Skip creating a Metal render encoder for root/container passes
			if(drawSnapshot.IsRoot())
			{
				continue;
			}

			_internals->currentRenderState = nullptr; //This is a property of the encoder and needs to be set to nullptr here to force setting it again.
			MTLRenderPassDescriptor *descriptor = renderPass.framebuffer->GetRenderPassDescriptor(drawSnapshot, renderPass.resolveFramebuffer, renderPass.multiviewLayer, 0);
			_internals->commandEncoder = [_internals->commandBuffer renderCommandEncoderWithDescriptor:descriptor];
			[descriptor release];

			Rect cameraRect = framePass.GetCameraSnapshot().GetFrame();
			MTLViewport viewPort;
			viewPort.originX = cameraRect.x;
			viewPort.originY = cameraRect.y;
			viewPort.width = cameraRect.width;
			viewPort.height = cameraRect.height;
			viewPort.znear = 0.0f;
			viewPort.zfar = 1.0f;
			[_internals->commandEncoder setViewport:viewPort];

			if(renderPass.type == MetalRenderPass::Type::Convert)
			{
				RenderAPIRenderPass(submission, renderPass);
			}
			else
			{
				RN_DEBUG_ASSERT(renderPass.preparedRenderPassIndex < submission.preparedRenderPasses.size(), "Invalid prepared render pass index");
				const MetalPreparedRenderPass &preparedPass = submission.preparedRenderPasses[renderPass.preparedRenderPassIndex];
				const std::vector<MetalPreparedDrawItem> &drawItems = preparedPass.drawItems;
				uint32 stepSize = 0;
				uint32 stepSizeIndex = 0;
				for(size_t i = 0; i < drawItems.size(); i += stepSize)
				{
					stepSize = preparedPass.instanceSteps[stepSizeIndex++];

					uint32 counter = 0;
					const MetalPreparedDrawItem &baseDrawItem = drawItems[i];
					const MetalDrawable::RenderResources &renderResources = *baseDrawItem.renderResources;
					for(size_t n = 0; n < renderResources.vertexShaderUniformBuffers.size(); n++)
					{
						for(size_t instance = 0; instance < stepSize; instance++)
						{
							Shader::ArgumentBuffer *argument = renderResources.argumentBufferToUniformBufferMapping[counter];

							//TODO: Somehow find a better way to know if an argument buffer contains instance data or not
							//Assume that only storage buffers can contain per instance data
							if(instance > 0 && argument->GetType() != Shader::ArgumentBuffer::Type::StorageBuffer) break;

							const MetalPreparedDrawItem &preparedDrawItem = drawItems[i + instance];
							const RenderFrame::DrawItem &drawItem = *preparedDrawItem.drawItem;
							const MetalDrawable::RenderResources &drawableRenderResources = *preparedDrawItem.renderResources;
							const Material::Properties &mergedMaterialProperties = drawableRenderResources.mergedMaterialSnapshot.GetProperties();

							MetalUniformBufferReference *bufferReference = drawableRenderResources.vertexShaderUniformBuffers[n];
							UpdateUniformBufferReference(bufferReference, instance == 0);
							FillUniformBuffer(argument, bufferReference, drawItem, mergedMaterialProperties, framePass);
						}
						counter += 1;
					}

					for(size_t n = 0; n < renderResources.fragmentShaderUniformBuffers.size(); n++)
					{
						for(size_t instance = 0; instance < stepSize; instance++)
						{
							Shader::ArgumentBuffer *argument = renderResources.argumentBufferToUniformBufferMapping[counter];

							//TODO: Somehow find a better way to know if an argument buffer contains instance data or not
							//Assume that only storage buffers can contain per instance data
							if(instance > 0 && argument->GetType() != Shader::ArgumentBuffer::Type::StorageBuffer) break;

							const MetalPreparedDrawItem &preparedDrawItem = drawItems[i + instance];
							const RenderFrame::DrawItem &drawItem = *preparedDrawItem.drawItem;
							const MetalDrawable::RenderResources &drawableRenderResources = *preparedDrawItem.renderResources;
							const Material::Properties &mergedMaterialProperties = drawableRenderResources.mergedMaterialSnapshot.GetProperties();

							MetalUniformBufferReference *bufferReference = drawableRenderResources.fragmentShaderUniformBuffers[n];
							UpdateUniformBufferReference(bufferReference, instance == 0);
							FillUniformBuffer(argument, bufferReference, drawItem, mergedMaterialProperties, framePass);
						}

						counter += 1;
					}

					RenderDrawable(baseDrawItem, stepSize, renderPass, framePass);
				}
			}

			[_internals->commandEncoder endEncoding];
			_internals->commandEncoder = nil;
		}

		for(MetalSwapChain *swapChain : submission.swapChains)
		{
			swapChain->Finalize();
			swapChain->PresentBackBuffer(_internals->commandBuffer);
		}

		submission.renderFrame.EndPresentationStatesOnRenderThread();

		//Flush all uniform buffers to make the GPU get the latest changes from CPU
		_uniformBufferPool->FlushAllBuffers();

		[_internals->commandBuffer commit];

		for(MetalSwapChain *swapChain : submission.swapChains)
		{
			swapChain->PostPresent(_internals->commandBuffer);
		}

		RN_PROFILE_FRAME();

		_internals->commandBuffer = nil;
	}

	void MetalRenderer::RenderAPIRenderPass(const MetalFrameSubmission &submission, const MetalRenderPass &renderPass)
	{
		RN_PROFILE_SCOPE();
		switch(renderPass.type)
		{
				case MetalRenderPass::Type::Convert:
				{
					RN_DEBUG_ASSERT(renderPass.previousStoredFramebuffer, "Convert render pass requires a previous framebuffer");
					RN_DEBUG_ASSERT(renderPass.preparedRenderPassIndex < submission.preparedRenderPasses.size(), "Invalid prepared render pass index");
					const MetalPreparedRenderPass &preparedPass = submission.preparedRenderPasses[renderPass.preparedRenderPassIndex];
					RN_DEBUG_ASSERT(!preparedPass.drawItems.empty(), "Convert render pass requires a prepared draw item");
					const RenderFrame::Pass &framePass = submission.renderFrame.GetPass(renderPass.renderFramePassIndex);
					RenderDrawable(preparedPass.drawItems[0], 1, renderPass, framePass);
					break;
				}

			case MetalRenderPass::Type::Blit:
			{
				//TODO: Handle multiple and not existing textures
				MetalFramebuffer *sourceFramebuffer = renderPass.previousStoredFramebuffer;
				Texture *sourceTexture = sourceFramebuffer->GetColorTexture(0);
				MetalFramebuffer *destinationFramebuffer = renderPass.framebuffer;
				Texture *destinationTexture = destinationFramebuffer->GetColorTexture(0);

				id<MTLTexture> sourceMTLTexture = nullptr;
				id<MTLTexture> destinationMTLTexture = nullptr;

				RN::Vector3 sourceTextureSize;

				if(sourceTexture)
				{
					sourceMTLTexture = static_cast< id<MTLTexture> >(sourceTexture->Downcast<MetalTexture>()->__GetUnderlyingTexture());
					sourceTextureSize.x = sourceTexture->GetDescriptor().width;
					sourceTextureSize.y = sourceTexture->GetDescriptor().height;
					sourceTextureSize.z = sourceTexture->GetDescriptor().depth;
				}
				else
				{
					sourceMTLTexture = sourceFramebuffer->GetSwapChain()->GetMetalColorTexture();
					sourceTextureSize.x = renderPass.previousStoredRenderAreaSize.x;
					sourceTextureSize.y = renderPass.previousStoredRenderAreaSize.y;
					sourceTextureSize.z = 0;
				}

				if(destinationTexture)
				{
					destinationMTLTexture = static_cast< id<MTLTexture> >(destinationTexture->Downcast<MetalTexture>()->__GetUnderlyingTexture());
				}
				else
				{
					destinationMTLTexture = destinationFramebuffer->GetSwapChain()->GetMetalColorTexture();
				}

				const RenderFrame::Pass &framePass = submission.renderFrame.GetPass(renderPass.renderFramePassIndex);
				MTLRenderPassDescriptor *descriptor = renderPass.framebuffer->GetRenderPassDescriptor(framePass.GetDrawSnapshot(), nullptr, 0, 0);
				id<MTLBlitCommandEncoder> commandEncoder = [[_internals->commandBuffer blitCommandEncoder] retain];
				[descriptor release];

				Rect targetRect = framePass.GetCameraSnapshot().GetFrame();
				[commandEncoder copyFromTexture:sourceMTLTexture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(sourceTextureSize.x, sourceTextureSize.y, sourceTextureSize.z) toTexture:destinationMTLTexture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(targetRect.x, targetRect.y, 0)];

				[commandEncoder endEncoding];
				[commandEncoder release];

				break;
			}

			default:
				break;
		}
	}

	void MetalRenderer::SubmitCamera(Camera *camera, Function &&function)
	{
		SubmitCamera(GetActiveFrameSubmission(), camera, camera, std::move(function));
	}

	void MetalRenderer::SubmitCamera(MetalFrameSubmission &frameSubmission, Camera *camera, Camera *lightClusterCamera, Function &&function)
	{
		RN_PROFILE_SCOPE();
		const Array *multiviewCameras = camera->GetMultiviewCameras();
		if(multiviewCameras && multiviewCameras->GetCount() > 0)
		{
			//TODO: Multiview is not supported by metal, should use instanced with viewport selection instead (https://developer.apple.com/documentation/metal/mtlrenderpassdescriptor/rendering_to_multiple_texture_slices_in_a_draw_command?language=objc)
			/*if(multiviewCameras->GetCount() > 1 && GetVulkanDevice()->GetSupportsMultiview())
			{

			}
			else*/
			{
				//If multiview is not supported or there is only one multiview camera, render them individually into the correct framebuffer texture layers
				multiviewCameras->Enumerate<Camera>([&](Camera *multiviewCamera, size_t index, bool &stop){
					_currentMultiviewLayer = index;
					_currentMultiviewFallbackRenderPass = camera->GetRenderPass();

					RN::Function submission = RN::MakeFunction([&function](){ function(); });
					SubmitCamera(frameSubmission, multiviewCamera, lightClusterCamera, std::move(submission));

					_currentMultiviewLayer = 0;
					_currentMultiviewFallbackRenderPass = nullptr;
				});

				return;
			}
		}

		RenderPass *cameraRenderPass = _currentMultiviewFallbackRenderPass? _currentMultiviewFallbackRenderPass : camera->GetRenderPass();

		// Ensure subpass clearing plan is computed for root containers
		cameraRenderPass->UpdateSubpassChain();
		const Array *nextRenderPasses = cameraRenderPass->GetNextRenderPasses();
		const size_t frameStatisticsIndex = frameSubmission.renderFrame.AddCameraStatistics();

		RenderPassResources *renderPassResources = cameraRenderPass->GetRenderResources(this);
		const RenderPass::DrawSnapshot &drawSnapshot = renderPassResources->GetDrawSnapshot();

		// Set up
		MetalRenderPass renderPass;
		renderPass.renderPass = cameraRenderPass;
		renderPass.frameStatisticsIndex = frameStatisticsIndex;

		renderPass.multiviewLayer = _currentMultiviewLayer;

		renderPass.renderAreaSize = drawSnapshot.GetFrame().GetSize();

		renderPass.shaderHint = GetMetalShaderHint(drawSnapshot.GetShaderHint());
		renderPass.renderFramePassIndex = frameSubmission.renderFrame.AddPass(drawSnapshot, renderPassResources->GetOverrideMaterialSnapshot(), renderPassResources->GetIdentity(), renderPassResources->GetOverrideMaterialSnapshotVersion());

		RenderFrame::CameraSnapshot cameraSnapshot = RenderFrame::CameraSnapshot::WithCamera(camera, drawSnapshot.GetFrame());
		RenderFrame::Pass &framePass = frameSubmission.renderFrame.GetPass(renderPass.renderFramePassIndex);
		framePass.SetCameraSnapshot(cameraSnapshot);

		Framebuffer *framebuffer = drawSnapshot.GetFramebuffer();
		renderPass.framebuffer = framebuffer->Downcast<MetalFramebuffer>();
		frameSubmission.AddSwapChain(renderPass.framebuffer->GetSwapChain());

		size_t previousRenderPassIndex = frameSubmission.renderPasses.size();
		frameSubmission.activeRenderPassIndex = previousRenderPassIndex;
		frameSubmission.renderPasses.push_back(renderPass);

		nextRenderPasses->Enumerate<RenderPass>([&](RenderPass *nextPass, size_t index, bool &stop) {
			SubmitRenderPass(frameSubmission, nextPass, frameSubmission.renderPasses[previousRenderPassIndex]);
		});

		const size_t submittedRenderPassEndIndex = frameSubmission.renderPasses.size();

		// Now distribute drawables across the newly created passes for this camera.
		frameSubmission.activeRenderPassIndex = previousRenderPassIndex;
		function();

		LightManager::DrawSnapshot lightClusterSnapshot;
		if(LightManager *lightManager = lightClusterCamera ? lightClusterCamera->GetLightManager() : nullptr)
		{
			lightClusterSnapshot = lightManager->GetDrawSnapshot();
		}

		for(size_t pi = previousRenderPassIndex; pi < submittedRenderPassEndIndex; pi++)
		{
			MetalRenderPass &submittedRenderPass = frameSubmission.renderPasses[pi];
			if(!submittedRenderPass.UsesDrawItems())
				continue;

			RenderFrame::Pass &framePass = frameSubmission.renderFrame.GetPass(submittedRenderPass.renderFramePassIndex);
			framePass.SetLightClusterSnapshot(lightClusterSnapshot);
		}
	}

	void MetalRenderer::SubmitRenderPass(MetalFrameSubmission &frameSubmission, RenderPass *renderPass, MetalRenderPass &previousRenderPass)
	{
		RN_PROFILE_SCOPE();

		renderPass->UpdateSubpassChain();

		// Set up
		MetalRenderPass metalRenderPass;
		metalRenderPass.renderPass = renderPass;
		metalRenderPass.frameStatisticsIndex = previousRenderPass.frameStatisticsIndex;

		PostProcessingAPIStage *apiStage = renderPass->Downcast<PostProcessingAPIStage>();
		bool isPostProcessingStage = renderPass->IsKindOfClass(PostProcessingStage::GetMetaClass());
		Material *rendererOverrideMaterial = nullptr;
		if(apiStage)
		{
			switch(apiStage->GetType())
			{
				case PostProcessingAPIStage::Type::ResolveMSAA:
				{
					metalRenderPass.type = MetalRenderPass::Type::ResolveMSAA;
					break;
				}

				case PostProcessingAPIStage::Type::Convert:
				{
					metalRenderPass.type = MetalRenderPass::Type::Convert;

					if(!_ppConvertMaterial)
					{
						_ppConvertMaterial = Material::WithShaders(_defaultShaderLibrary->GetShaderWithName(RNCSTR("pp_vertex")), _defaultShaderLibrary->GetShaderWithName(RNCSTR("pp_blit_fragment")))->Retain();
					}
					rendererOverrideMaterial = _ppConvertMaterial;
					break;
				}

				case PostProcessingAPIStage::Type::Blit:
				{
					metalRenderPass.type = MetalRenderPass::Type::Blit;
					break;
				}
			}
		}
		Material *effectiveOverrideMaterial = rendererOverrideMaterial ? rendererOverrideMaterial : renderPass->GetEffectiveOverrideMaterial();
		RenderPassResources *renderPassResources = renderPass->GetRenderResources(this, effectiveOverrideMaterial);
		const RenderPass::DrawSnapshot &drawSnapshot = renderPassResources->GetDrawSnapshot();

		if(previousRenderPass.renderPass)
		{
			metalRenderPass.previousStoredFramebuffer = previousRenderPass.resolveFramebuffer ? previousRenderPass.resolveFramebuffer : previousRenderPass.framebuffer;
			metalRenderPass.previousStoredRenderAreaSize = previousRenderPass.resolveFramebuffer ? previousRenderPass.resolveRenderAreaSize : previousRenderPass.renderAreaSize;
		}

		//This forces passes to not use multiview
		metalRenderPass.shaderHint = GetMetalShaderHint(drawSnapshot.GetShaderHint());

		metalRenderPass.multiviewLayer = previousRenderPass.multiviewLayer;
		Framebuffer *framebuffer = nullptr;
		if(drawSnapshot.IsSubpass())
		{
			// Subpass inherits root framebuffer
			framebuffer = previousRenderPass.framebuffer;
			metalRenderPass.renderAreaSize = previousRenderPass.renderAreaSize;
		}
		else
		{
			framebuffer = drawSnapshot.GetFramebuffer();
			metalRenderPass.renderAreaSize = drawSnapshot.GetFrame().GetSize();
		}
		metalRenderPass.framebuffer = framebuffer? framebuffer->Downcast<MetalFramebuffer>() : nullptr;
		frameSubmission.AddSwapChain(metalRenderPass.framebuffer ? metalRenderPass.framebuffer->GetSwapChain() : nullptr);

		if(metalRenderPass.type != MetalRenderPass::Type::ResolveMSAA)
		{
			metalRenderPass.renderFramePassIndex = frameSubmission.renderFrame.AddPass(drawSnapshot, renderPassResources->GetOverrideMaterialSnapshot(), renderPassResources->GetIdentity(), renderPassResources->GetOverrideMaterialSnapshotVersion());
			RenderFrame::Pass &framePass = frameSubmission.renderFrame.GetPass(metalRenderPass.renderFramePassIndex);
			framePass.SetCameraSnapshot(frameSubmission.renderFrame.GetPass(previousRenderPass.renderFramePassIndex).GetCameraSnapshot());
			frameSubmission.activeRenderPassIndex = frameSubmission.renderPasses.size();
			frameSubmission.renderPasses.push_back(metalRenderPass);

			if(isPostProcessingStage || metalRenderPass.type == MetalRenderPass::Type::Convert)
			{
				//Submit fullscreen quad drawable
				if(!_defaultPostProcessingDrawable)
				{
					Mesh *planeMesh = Mesh::WithTexturedPlane(Quaternion::WithEulerAngle(Vector3(0.0f, 90.0f, 0.0f)), Vector3(0.0f), Vector2(1.0f, 1.0f));
					Material *planeMaterial = Material::WithShaders(GetDefaultShader(Shader::Type::Vertex, nullptr), GetDefaultShader(Shader::Type::Fragment, nullptr));

					_lock.Lock();
					_defaultPostProcessingDrawable = static_cast<MetalDrawable*>(CreateDrawable());
					_defaultPostProcessingDrawable->SetSources(planeMesh, planeMaterial, nullptr);
					_lock.Unlock();
				}
				SubmitDrawable(frameSubmission, _defaultPostProcessingDrawable, nullptr);
			}
		}
		else
		{
			frameSubmission.renderPasses[frameSubmission.activeRenderPassIndex].resolveFramebuffer = metalRenderPass.framebuffer;
			frameSubmission.renderPasses[frameSubmission.activeRenderPassIndex].resolveRenderAreaSize = metalRenderPass.renderAreaSize;
		}

		const Array *nextRenderPasses = renderPass->GetNextRenderPasses();
		nextRenderPasses->Enumerate<RenderPass>([&](RenderPass *nextPass, size_t index, bool &stop){
			SubmitRenderPass(frameSubmission, nextPass, frameSubmission.renderPasses[frameSubmission.activeRenderPassIndex]);
		});
	}

	//TODO: Move into an utility class
	MTLResourceOptions MetalRenderer::MetalResourceOptionsFromOptions(GPUResource::AccessOptions options)
	{
		RN_PROFILE_SCOPE();
#if RN_PLATFORM_MAC_OS
		switch(options)
		{
			case GPUResource::AccessOptions::ReadWrite:
				return MTLResourceCPUCacheModeDefaultCache | MTLResourceStorageModeManaged;
			case GPUResource::AccessOptions::WriteOnly:
				return MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeManaged;
			case GPUResource::AccessOptions::Private:
				return MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeManaged; //return  MTLResourceStorageModePrivate; //This allows creating with good flags without having to deal with a staging buffer in the metal renderer for now
		}
#else
		switch(options)
		{
			case GPUResource::AccessOptions::ReadWrite:
				return MTLResourceCPUCacheModeDefaultCache;
			case GPUResource::AccessOptions::WriteOnly:
				return MTLResourceCPUCacheModeWriteCombined;
			case GPUResource::AccessOptions::Private:
				return MTLResourceCPUCacheModeWriteCombined; //return  MTLResourceStorageModePrivate; //This allows creating with good flags without having to deal with a staging buffer in the metal renderer for now
		}
#endif
	}

	GPUBuffer *MetalRenderer::CreateBufferWithLength(size_t length, GPUResource::UsageOptions usageOptions, GPUResource::AccessOptions accessOptions, bool streameable)
	{
		RN_PROFILE_SCOPE();
		MTLResourceOptions resourceOptions = MetalResourceOptionsFromOptions(accessOptions);
		if(streameable)
		{
			// Use a small dynamic wrapper that rotates underlying MTLBuffers per FlushRange
			return (new MetalDynamicGPUBuffer(_internals->device, length, resourceOptions));
		}

		id<MTLBuffer> buffer = [_internals->device newBufferWithLength:length options:resourceOptions];
		if(!buffer) return nullptr;
		return (new MetalGPUBuffer(buffer));
	}

	MetalUniformBufferReference *MetalRenderer::GetUniformBufferReference(size_t size, size_t index)
	{
		RN_PROFILE_SCOPE();
		AssertOnRenderThread();
		LockGuard<Lockable> lock(_lock);
		return _uniformBufferPool->GetUniformBufferReference(size, index);
	}

	void MetalRenderer::UpdateUniformBufferReference(MetalUniformBufferReference *reference, bool align)
	{
		RN_PROFILE_SCOPE();
		AssertOnRenderThread();
		LockGuard<Lockable> lock(_lock);
		return _uniformBufferPool->UpdateUniformBufferReference(reference, align);
	}

	ShaderLibrary *MetalRenderer::CreateShaderLibraryWithFile(const String *file)
	{
		RN_PROFILE_SCOPE();
		MetalShaderLibrary *lib = new MetalShaderLibrary(_internals->device, file, &_internals->stateCoordinator);
		return lib;
	}
	ShaderLibrary *MetalRenderer::CreateShaderLibraryWithSource(const String *source)
	{
		RN_PROFILE_SCOPE();
		MetalShaderLibrary *lib = new MetalShaderLibrary(_internals->device, nullptr, &_internals->stateCoordinator);
		return lib;
	}

	ShaderLibrary *MetalRenderer::GetDefaultShaderLibrary()
	{
		RN_PROFILE_SCOPE();
		return _defaultShaderLibrary;
	}

	bool MetalRenderer::SupportsTextureFormat(const String *format) const
	{
		RN_PROFILE_SCOPE();
		//TODO: Fix this
		return true;
	}
	bool MetalRenderer::SupportsDrawMode(DrawMode mode) const
	{
		RN_PROFILE_SCOPE();
		return true;
	}

	// https://developer.apple.com/library/ios/documentation/Metal/Reference/MetalShadingLanguageGuide/MetalShadingLanguageGuide.pdf
	size_t MetalRenderer::GetAlignmentForType(PrimitiveType type) const
	{
		RN_PROFILE_SCOPE();
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

	size_t MetalRenderer::GetSizeForType(PrimitiveType type) const
	{
		RN_PROFILE_SCOPE();
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
				return 48; //Stored as 3 x float4

			case PrimitiveType::Matrix4x4:
				return 64;
		}

		return 1;
	}

	Texture *MetalRenderer::CreateTextureWithDescriptor(const Texture::Descriptor &descriptor)
	{
		RN_PROFILE_SCOPE();
		MTLTextureDescriptor *metalDescriptor = MetalTexture::DescriptorForTextureDescriptor(descriptor);
		id<MTLTexture> texture = [_internals->device newTextureWithDescriptor:metalDescriptor];
		[metalDescriptor release];

		return new MetalTexture(this, texture, descriptor);
	}

	Texture *MetalRenderer::CreateTextureWithDescriptorAndIOSurface(const Texture::Descriptor &descriptor, IOSurfaceRef ioSurface)
	{
		RN_PROFILE_SCOPE();
		MTLTextureDescriptor *metalDescriptor = MetalTexture::DescriptorForTextureDescriptor(descriptor, true);
		id<MTLTexture> texture = [_internals->device newTextureWithDescriptor:metalDescriptor iosurface:ioSurface plane:0];
		[metalDescriptor release];

		return new MetalTexture(this, texture, descriptor);
	}

	Framebuffer *MetalRenderer::CreateFramebuffer(const Vector2 &size)
	{
		RN_PROFILE_SCOPE();
		return new MetalFramebuffer(size);
	}

	Drawable *MetalRenderer::CreateDrawable()
	{
		RN_PROFILE_SCOPE();
		MetalDrawable *drawable = new MetalDrawable();
		return drawable;
	}

	void MetalRenderer::DeleteDrawable(Drawable *drawable)
	{
		RN_PROFILE_SCOPE();
		QueueDrawableDeletion(drawable);
	}

	void MetalRenderer::FillUniformBuffer(Shader::ArgumentBuffer *argument, MetalUniformBufferReference *uniformBufferReference, const RenderFrame::DrawItem &drawItem, const Material::Properties &materialProperties, const RenderFrame::Pass &framePass)
	{
		RN_PROFILE_SCOPE();
		GPUBuffer *gpuBuffer = uniformBufferReference->uniformBuffer->GetActiveBuffer();
		uint8 *buffer = reinterpret_cast<uint8 *>(gpuBuffer->GetBuffer()) + uniformBufferReference->offset;

		const RenderFrame::CameraSnapshot &cameraSnapshot = framePass.GetCameraSnapshot();
		const Matrix &modelMatrix = drawItem.GetModelMatrix();
		const Matrix &inverseModelMatrix = drawItem.GetInverseModelMatrix();

		argument->GetUniformDescriptors()->Enumerate<Shader::UniformDescriptor>([&](Shader::UniformDescriptor *descriptor, size_t index, bool &stop) {
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
					std::memcpy(buffer + descriptor->GetOffset(), modelMatrix.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::NormalMatrix:
				{
					Matrix normalMatrix = inverseModelMatrix.GetTransposed();
					std::memcpy(buffer + descriptor->GetOffset(), &normalMatrix.m[0], 12);
					std::memcpy(buffer + descriptor->GetOffset() + 16, &normalMatrix.m[4], 12);
					std::memcpy(buffer + descriptor->GetOffset() + 32, &normalMatrix.m[8], 12);
					break;
				}

				case Shader::UniformDescriptor::Identifier::ModelViewMatrix:
				{
					Matrix result = cameraSnapshot.GetViewMatrix() * modelMatrix;
					std::memcpy(buffer + descriptor->GetOffset(), result.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::ModelViewProjectionMatrix:
				{
					Matrix result = cameraSnapshot.GetProjectionViewMatrix() * modelMatrix;
					std::memcpy(buffer + descriptor->GetOffset(), result.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::ViewMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), cameraSnapshot.GetViewMatrix().m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::ViewProjectionMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), cameraSnapshot.GetProjectionViewMatrix().m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::ProjectionMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), cameraSnapshot.GetProjectionMatrix().m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseModelMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), inverseModelMatrix.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseModelViewMatrix:
				{
					Matrix result = cameraSnapshot.GetInverseViewMatrix() * inverseModelMatrix;
					std::memcpy(buffer + descriptor->GetOffset(), result.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseModelViewProjectionMatrix:
				{
					Matrix result = cameraSnapshot.GetInverseProjectionViewMatrix() * inverseModelMatrix;
					std::memcpy(buffer + descriptor->GetOffset(), result.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseViewMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), cameraSnapshot.GetInverseViewMatrix().m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseViewProjectionMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), cameraSnapshot.GetInverseProjectionViewMatrix().m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::InverseProjectionMatrix:
				{
					std::memcpy(buffer + descriptor->GetOffset(), cameraSnapshot.GetInverseProjectionMatrix().m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::AmbientColor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &materialProperties.ambientColor.r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::DiffuseColor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &materialProperties.diffuseColor.r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::SpecularColor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &materialProperties.specularColor.r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::EmissiveColor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &materialProperties.emissiveColor.r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CustomMatrix1:
				{
					std::memcpy(buffer + descriptor->GetOffset(), materialProperties.customMatrix1.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CustomMatrix2:
				{
					std::memcpy(buffer + descriptor->GetOffset(), materialProperties.customMatrix2.m, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::UIClippingRect:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &materialProperties.uiClippingRect.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::UIOffset:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &materialProperties.uiOffset.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::UIOutlineColor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &materialProperties.uiOutlineColor.r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::TextureTileFactor:
				{
					float temp = materialProperties.textureTileFactor;
					std::memcpy(buffer + descriptor->GetOffset(), &temp, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::AlphaToCoverageClamp:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &materialProperties.alphaToCoverageClamp.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraPosition:
				{
					Vector4 cameraPosition = Vector4(cameraSnapshot.GetViewPosition(), 0.0f);
					std::memcpy(buffer + descriptor->GetOffset(), &cameraPosition.x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraClipDistance:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &cameraSnapshot.GetClipDistance().x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraFogDistance:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &cameraSnapshot.GetFogDistance().x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraTag:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &cameraSnapshot.GetTag(), descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraViewport:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &cameraSnapshot.GetFrame().x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraAmbientColor:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &cameraSnapshot.GetAmbientColor().r, descriptor->GetSize());
					break;
				}
				case Shader::UniformDescriptor::Identifier::CameraCustomData:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &cameraSnapshot.GetCustomData().x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraFogColor0:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &cameraSnapshot.GetFogColor0().r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::CameraFogColor1:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &cameraSnapshot.GetFogColor1().r, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::DirectionalLightsCount:
				{
					uint32 lightCount = std::min(framePass.GetDirectionalLights().size(), descriptor->GetElementCount());
					std::memcpy(buffer + descriptor->GetOffset(), &lightCount, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::DirectionalLights:
				{
					size_t lightCount = std::min(framePass.GetDirectionalLights().size(), descriptor->GetElementCount());
					if(lightCount > 0)
					{
						std::memcpy(buffer + descriptor->GetOffset(), &framePass.GetDirectionalLights()[0], (16 + 16) * lightCount);
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::DirectionalShadowMatricesCount:
				{
					//TODO: Limit matrixCount to descriptor->GetElementCount() of Shader::UniformDescriptor::Identifier::DirectionalShadowMatrices
					uint32 matrixCount = framePass.GetDirectionalShadowMatrices().size();
					std::memcpy(buffer + descriptor->GetOffset(), &matrixCount, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::DirectionalShadowMatrices:
				{
					size_t matrixCount = std::min(framePass.GetDirectionalShadowMatrices().size(), descriptor->GetElementCount());
					if(matrixCount > 0)
					{
						std::memcpy(buffer + descriptor->GetOffset(), &framePass.GetDirectionalShadowMatrices()[0].m[0], 64 * matrixCount);
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::DirectionalShadowInfo:
				{
					std::memcpy(buffer + descriptor->GetOffset(), &framePass.GetDirectionalShadowInfo().x, descriptor->GetSize());
					break;
				}

				case Shader::UniformDescriptor::Identifier::PointLights:
				{
					size_t lightCount = std::min(framePass.GetPointLights().size(), descriptor->GetElementCount());
					if(lightCount > 0)
					{
						std::memcpy(buffer + descriptor->GetOffset(), &framePass.GetPointLights()[0], (12 + 4 + 16) * lightCount);
					}
					if(lightCount < descriptor->GetElementCount()) //TODO: Think about how max number of lights is filled up...
					{
						std::memset(buffer + descriptor->GetOffset() + (12 + 4 + 16) * lightCount, 0, (12 + 4 + 16) * (descriptor->GetElementCount() - lightCount));
					}
					break;
				}

				case Shader::UniformDescriptor::Identifier::SpotLights:
				{
					size_t lightCount = std::min(framePass.GetSpotLights().size(), descriptor->GetElementCount());
					if(lightCount > 0)
					{
						std::memcpy(buffer + descriptor->GetOffset(), &framePass.GetSpotLights()[0], (12 + 4 + 12 + 4 + 16) * lightCount);
					}
					if(lightCount < descriptor->GetElementCount()) //TODO: Think about how max number of lights is filled up...
					{
						std::memset(buffer + descriptor->GetOffset() + (12 + 4 + 12 + 4 + 16) * lightCount, 0, (12 + 4 + 12 + 4 + 16) * (descriptor->GetElementCount() - lightCount));
					}

					break;
				}

				case Shader::UniformDescriptor::Identifier::BoneMatrices:
				{
					const std::vector<Matrix> &boneMatrices = drawItem.GetSkeleton().GetMatrices();
					if(boneMatrices.size() > 0)
					{
						//TODO: Don't hardcode limit here
						size_t matrixCount = std::min(boneMatrices.size(), descriptor->GetElementCount());
						if(matrixCount > 0)
						{
							std::memcpy(buffer + descriptor->GetOffset(), &boneMatrices[0].m[0], 64 * matrixCount);
						}
					}
					break;
				}

				//TODO: Support arrays!
				case Shader::UniformDescriptor::Identifier::Custom:
				{
					Object *object = materialProperties.GetCustomShaderUniform(descriptor->GetNameHash());
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
		});
	}

	void MetalRenderer::SubmitLight(const Light *light)
	{
		RN_PROFILE_SCOPE();
		MetalFrameSubmission &frameSubmission = GetActiveFrameSubmission();

		// Distribute the light to all passes of the current camera
		size_t startIndex = frameSubmission.activeRenderPassIndex;

		for(size_t pi = startIndex; pi < frameSubmission.renderPasses.size(); pi++)
		{
			MetalRenderPass &renderPass = frameSubmission.renderPasses[pi];
			// Only apply to real draw passes
			if(!renderPass.UsesDrawItems()) continue;
			RenderFrame::Pass &framePass = frameSubmission.renderFrame.GetPass(renderPass.renderFramePassIndex);

			if(light->GetType() == Light::Type::DirectionalLight)
			{
				framePass.AddDirectionalLight(RenderFrame::DirectionalLight::WithLight(light));

				// Attach shadow texture/matrices to all non-shadow-camera passes
				if(light->HasShadows())
				{
					bool isShadowCamera = false;
					light->GetShadowDepthCameras()->Enumerate<Camera>([&](Camera *camera, size_t index, bool &stop) {
						Framebuffer *shadowFB = camera->GetRenderPass()->GetFramebuffer();
						if(renderPass.framebuffer == shadowFB)
						{
							isShadowCamera = true;
							stop = true;
						}
					});

					if(!isShadowCamera)
					{
						framePass.SetDirectionalShadowDepthTexture(light->GetShadowDepthTexture());
						framePass.SetDirectionalShadowMatrices(light->GetShadowMatrices());
						framePass.SetDirectionalShadowInfo(Vector2(1.0f / light->GetShadowParameters().resolution));
					}
				}
			}
			else if(light->GetType() == Light::Type::PointLight)
			{
				framePass.AddPointLight(RenderFrame::PointLight::WithLight(light));
			}
			else if(light->GetType() == Light::Type::SpotLight)
			{
				framePass.AddSpotLight(RenderFrame::SpotLight::WithLight(light));
			}
		}
	}

	void MetalRenderer::PrepareRenderFrame(MetalFrameSubmission &submission)
	{
		RN_PROFILE_SCOPE();
		AssertOnRenderThread();
		CreateMipMaps();

		submission.preparedRenderPasses.clear();
		submission.preparedRenderPasses.reserve(submission.renderFrame.GetPassCount());

		auto ensureRenderPassResources = [&](MetalRenderPass &renderPass) {
			renderPass.preparedRenderPassIndex = RenderFrame::InvalidPassIndex;

			if(!renderPass.UsesDrawItems())
			{
				return;
			}

			const RenderFrame::Pass &framePass = submission.renderFrame.GetPass(renderPass.renderFramePassIndex);
			const std::vector<size_t> &drawItemIndices = framePass.GetDrawItemIndices();
			renderPass.preparedRenderPassIndex = submission.preparedRenderPasses.size();
			submission.preparedRenderPasses.emplace_back();

			for(size_t drawItemIndex : drawItemIndices)
			{
				const RenderFrame::DrawItem &drawItem = submission.renderFrame.GetDrawItem(drawItemIndex);
				MetalDrawable *drawable = static_cast<MetalDrawable *>(drawItem.GetSourceDrawableForPreparation());
				drawable->EnsureRenderResources(renderPass.preparedRenderPassIndex);
			}
		};

		auto appendPreparedDrawItem = [](MetalPreparedRenderPass &preparedPass, const RenderFrame::DrawItem &drawItem, const MetalDrawable::RenderResources &renderResources, RenderFrame::CameraStatistics &statistics) {
			MetalPreparedDrawItem preparedDrawItem;
			preparedDrawItem.drawItem = &drawItem;
			preparedDrawItem.renderResources = &renderResources;
			preparedPass.drawItems.push_back(preparedDrawItem);

			statistics.numberOfDrawables += 1;
			statistics.numberOfVertices += drawItem.GetMesh().GetVerticesCount();
			statistics.numberOfIndices += drawItem.GetMesh().GetIndicesCount();
		};

		auto getPipelineKey = [](const RenderFrame::DrawItem &drawItem, const MetalRenderPass &renderPass, const Drawable::MergedMaterialSnapshot &mergedMaterialSnapshot) {
			Drawable::PipelineKey pipelineKey;
			pipelineKey.meshPipelineHash = drawItem.GetMesh().GetPipelineHash();
			pipelineKey.framebuffer = renderPass.framebuffer;
			pipelineKey.vertexShader = mergedMaterialSnapshot.GetVertexShader();
			pipelineKey.fragmentShader = mergedMaterialSnapshot.GetFragmentShader();
			pipelineKey.materialProperties = mergedMaterialSnapshot.GetPipelineProperties();
			pipelineKey.renderPass = renderPass.renderPass;
			return pipelineKey;
		};

		for(MetalRenderPass &renderPass : submission.renderPasses)
		{
			ensureRenderPassResources(renderPass);
		}

		auto prepareRenderPass = [&](MetalRenderPass &renderPass) {
			if(renderPass.preparedRenderPassIndex == RenderFrame::InvalidPassIndex)
				return;

			MetalPreparedRenderPass &preparedPass = submission.preparedRenderPasses[renderPass.preparedRenderPassIndex];
			const RenderFrame::Pass &framePass = submission.renderFrame.GetPass(renderPass.renderFramePassIndex);
			const RenderPass::DrawSnapshot &passDrawSnapshot = framePass.GetDrawSnapshot();
			const std::vector<size_t> &drawItemIndices = framePass.GetDrawItemIndices();
			if(drawItemIndices.empty())
				return;

			preparedPass.drawItems.reserve(drawItemIndices.size());

			const RenderFrame::DrawItem *currentInstanceDrawItem = nullptr;
			const MetalRenderingState *currentPipelineState = nullptr;
			const MetalDrawable::RenderResources *currentInstanceRenderResources = nullptr;
			RenderFrame::CameraStatistics &statistics = submission.renderFrame.GetCameraStatistics(renderPass.frameStatisticsIndex);

			for(size_t drawItemIndex : drawItemIndices)
			{
				const RenderFrame::DrawItem &drawItem = submission.renderFrame.GetDrawItem(drawItemIndex);
				MetalDrawable *drawable = static_cast<MetalDrawable *>(drawItem.GetSourceDrawableForPreparation());
				MetalDrawable::RenderResources &renderResources = drawable->GetRenderResources(renderPass.preparedRenderPassIndex);

				const Material::DrawSnapshot *overrideMaterialSnapshot = framePass.GetOverrideMaterialSnapshot();
				renderResources.mergedMaterialSnapshot.Update(drawItem.GetMaterial(), drawItem.GetMaterialSnapshotVersion(), renderPass.shaderHint, overrideMaterialSnapshot, framePass.GetOverrideMaterialCacheIdentity(), framePass.GetOverrideMaterialSnapshotVersion());
				Drawable::PipelineKey pipelineKey = getPipelineKey(drawItem, renderPass, renderResources.mergedMaterialSnapshot);

				if(!renderResources.pipelineState || renderResources.pipelineKey != pipelineKey)
				{
					_lock.Lock();
					const MetalRenderingState *state = _internals->stateCoordinator.GetRenderPipelineState(pipelineKey.vertexShader, pipelineKey.fragmentShader, drawItem.GetMesh(), renderPass.framebuffer, pipelineKey.materialProperties, passDrawSnapshot);
					_lock.Unlock();

					drawable->UpdateRenderingState(renderResources, this, state, pipelineKey);
				}

				Shader *vertexShader = renderResources.pipelineState->vertexShader;
				Shader *fragmentShader = renderResources.pipelineState->fragmentShader;
				bool canUseInstancing = vertexShader && fragmentShader && vertexShader->GetHasInstancing() && fragmentShader->GetHasInstancing();
				const RenderFrame::DrawItem *instanceDrawItem = currentInstanceDrawItem;
				const MetalDrawable::RenderResources *instanceRenderResources = currentInstanceRenderResources;

				if(canUseInstancing && instanceRenderResources)
				{
					if(renderResources.vertexShaderUniformBuffers.size() != instanceRenderResources->vertexShaderUniformBuffers.size() || renderResources.fragmentShaderUniformBuffers.size() != instanceRenderResources->fragmentShaderUniformBuffers.size())
					{
						canUseInstancing = false;
					}
					else
					{
						for(int i = 0; i < renderResources.vertexShaderUniformBuffers.size() && canUseInstancing; i++)
						{
							if(renderResources.vertexShaderUniformBuffers[i]->uniformBuffer != instanceRenderResources->vertexShaderUniformBuffers[i]->uniformBuffer)
							{
								canUseInstancing = false;
							}
						}

						for(int i = 0; i < renderResources.fragmentShaderUniformBuffers.size() && canUseInstancing; i++)
						{
							if(renderResources.fragmentShaderUniformBuffers[i]->uniformBuffer != instanceRenderResources->fragmentShaderUniformBuffers[i]->uniformBuffer)
							{
								canUseInstancing = false;
							}
						}
					}
				}

				if(canUseInstancing && currentPipelineState == renderResources.pipelineState && instanceRenderResources && drawItem.CanInstanceWith(*instanceDrawItem) && renderResources.mergedMaterialSnapshot.IsTextureSetEqual(instanceRenderResources->mergedMaterialSnapshot))
				{
					preparedPass.instanceSteps.back() += 1;
				}
				else
				{
					currentPipelineState = renderResources.pipelineState;
					currentInstanceRenderResources = &renderResources;
					currentInstanceDrawItem = &drawItem;
					preparedPass.instanceSteps.push_back(1);
					statistics.numberOfDrawCalls += 1;
				}

				appendPreparedDrawItem(preparedPass, drawItem, renderResources, statistics);
			}
		};

		for(MetalRenderPass &renderPass : submission.renderPasses)
		{
			prepareRenderPass(renderPass);
		}

		// Do this after pipeline preparation so newly created uniform references have backing buffers.
		_uniformBufferPool->Update(this); //This will also reset all reference offsets into the buffer
	}

	void MetalRenderer::SubmitDrawable(Drawable *drawable, const SceneNode *node)
	{
		SubmitDrawable(GetActiveFrameSubmission(), drawable, node);
	}

	void MetalRenderer::SubmitDrawable(MetalFrameSubmission &frameSubmission, Drawable *sourceDrawable, const SceneNode *node)
	{
		RN_PROFILE_SCOPE();
		MetalDrawable *drawable = static_cast<MetalDrawable *>(sourceDrawable);
		uint16 renderGroup = node ? node->GetRenderGroup() : 0xffff;
		size_t drawItemIndex = RenderFrame::InvalidDrawItemIndex;

		size_t startIndex = frameSubmission.activeRenderPassIndex;
		for(size_t pi = startIndex; pi < frameSubmission.renderPasses.size(); pi += 1)
		{
			MetalRenderPass &pass = frameSubmission.renderPasses[pi];
			if(!pass.UsesDrawItems()) continue;
			if((pass.type == MetalRenderPass::Type::Convert || pass.renderPass->IsKindOfClass(PostProcessingStage::GetMetaClass())) && drawable != _defaultPostProcessingDrawable) continue;

			RenderFrame::Pass &framePass = frameSubmission.renderFrame.GetPass(pass.renderFramePassIndex);
			const RenderPass::DrawSnapshot &passDrawSnapshot = framePass.GetDrawSnapshot();
			if((renderGroup & passDrawSnapshot.GetRenderGroupMask()) == 0) continue;

			if(drawItemIndex == RenderFrame::InvalidDrawItemIndex)
				drawItemIndex = frameSubmission.renderFrame.AddDrawItem(drawable, node);
			framePass.AddDrawItemIndex(drawItemIndex);
		}
	}

	void MetalRenderer::RenderDrawable(const MetalPreparedDrawItem &preparedDrawItem, uint32 instanceCount, const MetalRenderPass &renderPass, const RenderFrame::Pass &framePass)
	{
		RN_PROFILE_SCOPE();
		const RenderFrame::DrawItem &drawItem = *preparedDrawItem.drawItem;
		const MetalDrawable::RenderResources &renderResources = *preparedDrawItem.renderResources;

		id<MTLRenderCommandEncoder> encoder = _internals->commandEncoder;
		if(_internals->currentRenderState != renderResources.pipelineState)
		{
			_internals->currentRenderState = renderResources.pipelineState;
			[encoder setRenderPipelineState:_internals->currentRenderState->state];
		}

		Shader *vertexShader = _internals->currentRenderState->vertexShader;
		Shader *fragmentShader = _internals->currentRenderState->fragmentShader;
		MetalShader *metalVertexShader = nullptr;
		MetalShader *metalFragmentShader = nullptr;
		if(vertexShader)
		{
			metalVertexShader = vertexShader->Downcast<MetalShader>();
		}
		if(fragmentShader)
		{
			metalFragmentShader = fragmentShader->Downcast<MetalShader>();
		}

		const Material::PipelineProperties &mergedMaterialProperties = renderResources.pipelineKey.materialProperties;
		[encoder setDepthStencilState:_internals->stateCoordinator.GetDepthStencilStateForMaterial(mergedMaterialProperties, _internals->currentRenderState)];
		[encoder setCullMode:static_cast<MTLCullMode>(mergedMaterialProperties.cullMode)];
		if(mergedMaterialProperties.usePolygonOffset)
		{
			[encoder setDepthBias:mergedMaterialProperties.polygonOffsetUnits slopeScale:mergedMaterialProperties.polygonOffsetFactor clamp:FLT_MAX];
		}
		else
		{
			[encoder setDepthBias:0.0f slopeScale:0.0f clamp:FLT_MAX];
		}

		// Update uniform buffers and set them for rendering
		{
			for(MetalUniformBufferReference *uniformBufferReference : renderResources.vertexShaderUniformBuffers)
			{
				MetalGPUBuffer *buffer = static_cast<MetalGPUBuffer *>(uniformBufferReference->uniformBuffer->GetActiveBuffer());
				[encoder setVertexBuffer:(id <MTLBuffer>)buffer->_buffer offset:uniformBufferReference->offset atIndex:uniformBufferReference->shaderResourceIndex];
			}

			for(MetalUniformBufferReference *uniformBufferReference : renderResources.fragmentShaderUniformBuffers)
			{
				MetalGPUBuffer *buffer = static_cast<MetalGPUBuffer *>(uniformBufferReference->uniformBuffer->GetActiveBuffer());
				[encoder setFragmentBuffer:(id <MTLBuffer>)buffer->_buffer offset:uniformBufferReference->offset atIndex:uniformBufferReference->shaderResourceIndex];
			}

			// Bind LightManager buffers (reflected) by semantic
			const LightManager::DrawSnapshot &lightClusterSnapshot = framePass.GetLightClusterSnapshot();
			if(renderPass.shaderHint == Shader::UsageHint::Default && metalFragmentShader && lightClusterSnapshot.IsValid())
			{
				metalFragmentShader->GetSignature()->GetBuffers()->Enumerate<Shader::ArgumentBuffer>([&](Shader::ArgumentBuffer *arg, size_t index, bool &stop) {
					uint32 bindIndex = arg->GetIndex();
					switch(arg->GetSemantic())
					{
						case Shader::ArgumentBuffer::Semantic::LightClusterPointLights:
						{
							GPUBuffer *pl = lightClusterSnapshot.GetPointLightBuffer();
							if(pl) [encoder setFragmentBuffer:(id<MTLBuffer>)static_cast<MetalGPUBuffer *>(pl)->_buffer offset:0 atIndex:bindIndex];
							break;
						}
						case Shader::ArgumentBuffer::Semantic::LightClusterSpotLights:
						{
							GPUBuffer *sl = lightClusterSnapshot.GetSpotLightBuffer();
							if(sl) [encoder setFragmentBuffer:(id<MTLBuffer>)static_cast<MetalGPUBuffer *>(sl)->_buffer offset:0 atIndex:bindIndex];
							break;
						}
						case Shader::ArgumentBuffer::Semantic::LightClusterRecords:
						{
							GPUBuffer *cr = lightClusterSnapshot.GetClusterRecordsBuffer();
							if(cr) [encoder setFragmentBuffer:(id<MTLBuffer>)static_cast<MetalGPUBuffer *>(cr)->_buffer offset:0 atIndex:bindIndex];
							break;
						}
						case Shader::ArgumentBuffer::Semantic::LightClusterIndices:
						{
							GPUBuffer *ci = lightClusterSnapshot.GetClusterIndexBuffer();
							if(ci) [encoder setFragmentBuffer:(id<MTLBuffer>)static_cast<MetalGPUBuffer *>(ci)->_buffer offset:0 atIndex:bindIndex];
							break;
						}
						default: break;
					}
				});
			}
		}

		// Set textures
		//TODO: Support vertex shader textures
		const Array *textures = renderResources.mergedMaterialSnapshot.GetTextures();
		metalFragmentShader->GetSignature()->GetTextures()->Enumerate<Shader::ArgumentTexture>([&](Shader::ArgumentTexture *argument, size_t index, bool &stop){
			if(argument->GetMaterialTextureIndex() == Shader::ArgumentTexture::IndexDirectionalShadowTexture)
			{
				Texture *directionalShadowDepthTexture = framePass.GetDirectionalShadowDepthTexture();
				if(directionalShadowDepthTexture)
				{
					MetalTexture *metalTexture = directionalShadowDepthTexture->Downcast<MetalTexture>();
					[encoder setFragmentTexture:(id<MTLTexture>)metalTexture->__GetUnderlyingTexture() atIndex:argument->GetIndex()];
				}
				else
				{
					[encoder setFragmentTexture:nil atIndex:argument->GetIndex()];
				}
			}
			else if(argument->GetMaterialTextureIndex() == Shader::ArgumentTexture::IndexFramebufferTexture)
			{
				MetalFramebuffer *previousFramebuffer = renderPass.previousStoredFramebuffer;
				if(previousFramebuffer)
				{
					MetalSwapChain *swapChain = previousFramebuffer->GetSwapChain();
					if(swapChain)
					{
						[encoder setFragmentTexture:swapChain->GetMetalColorTexture() atIndex:argument->GetIndex()];
					}
					else
					{
						MetalTexture *colorBuffer = previousFramebuffer->GetColorTexture()->Downcast<MetalTexture>();
						[encoder setFragmentTexture:(id<MTLTexture>)colorBuffer->__GetUnderlyingTexture() atIndex:argument->GetIndex()];
					}
				}
				else
				{
					[encoder setFragmentTexture:nil atIndex:argument->GetIndex()];
				}
			}
			else
			{
				uint8 materialTextureIndex = argument->GetMaterialTextureIndex();
				if(textures && materialTextureIndex < textures->GetCount())
				{
					Object *textureObject = textures->GetObjectAtIndex(argument->GetMaterialTextureIndex());

					id<MTLTexture> texture = nullptr;
					if(textureObject->IsKindOfClass(MetalTexture::GetMetaClass()))
					{
						texture = (id<MTLTexture>)static_cast<MetalTexture*>(textureObject)->__GetUnderlyingTexture();
					}
					else
					{
						MetalFramebuffer *framebuffer = static_cast<MetalFramebuffer*>(textureObject);
						if(framebuffer->GetSwapChain()) texture = framebuffer->GetSwapChain()->GetMetalColorTexture();
						else texture = (id<MTLTexture>)framebuffer->GetColorTexture()->Downcast<MetalTexture>()->__GetUnderlyingTexture();
					}

					[encoder setFragmentTexture:texture atIndex:argument->GetIndex()];
				}
				else
				{
					[encoder setFragmentTexture:nil atIndex:argument->GetIndex()];
				}
			}
		});

		metalFragmentShader->GetSignature()->GetSubpassInputs()->Enumerate<Shader::ArgumentTexture>([&](Shader::ArgumentTexture *argument, size_t index, bool &stop){
			uint8 materialTextureIndex = argument->GetMaterialTextureIndex();
			bool isDepthInput = materialTextureIndex >= 128;
			materialTextureIndex = isDepthInput ? materialTextureIndex - 128 : materialTextureIndex;

			if(!renderPass.framebuffer)
			{
				[encoder setFragmentTexture:nil atIndex:argument->GetIndex()];
				return;
			}

			const RenderPass::DrawSnapshot &drawSnapshot = framePass.GetDrawSnapshot();
			if(!isDepthInput && drawSnapshot.IsSubpass())
			{
				//Skip unused color attachments in the assignment to match vulkan subpass behavior
				uint8 targetIndex = materialTextureIndex;
				for(uint8 i = 0; i < renderPass.framebuffer->GetColorTargetCount(); i++)
				{
					if(drawSnapshot.GetSubpass().GetColorAttachment(i).GetReads())
					{
						if(targetIndex == 0)
						{
							materialTextureIndex = i;
							break;
						}
						targetIndex -= 1;
					}
				}
			}

			Texture *texture = isDepthInput ? renderPass.framebuffer->GetDepthStencilTexture() : renderPass.framebuffer->GetColorTexture(materialTextureIndex);
			MetalTexture *framebufferTexture = texture ? texture->Downcast<MetalTexture>() : nullptr;

			if(!framebufferTexture)
			{
				[encoder setFragmentTexture:nil atIndex:argument->GetIndex()];
				return;
			}

			[encoder setFragmentTexture:(id<MTLTexture>)framebufferTexture->__GetUnderlyingTexture() atIndex:argument->GetIndex()];
		});

		//Set samplers
		size_t count = 0;
		for(void *sampler : metalVertexShader->_samplers)
		{
			id<MTLSamplerState> samplerState = static_cast<id<MTLSamplerState>>(sampler);
			[encoder setVertexSamplerState:samplerState atIndex:metalFragmentShader->_samplerToIndexMapping[count++]];
		}
		count = 0;
		for(void *sampler : metalFragmentShader->_samplers)
		{
			id<MTLSamplerState> samplerState = static_cast<id<MTLSamplerState>>(sampler);
			[encoder setFragmentSamplerState:samplerState atIndex:metalFragmentShader->_samplerToIndexMapping[count++]];
		}

		// Mesh
		const Mesh::DrawSnapshot &mesh = drawItem.GetMesh();
		const Mesh::BufferSnapshot &meshBuffers = drawItem.GetMeshBuffers();
		MetalGPUBuffer *buffer = static_cast<MetalGPUBuffer *>(meshBuffers.GetVertexBuffer());

		if(_internals->currentRenderState->vertexPositionBufferShaderResourceIndex <= 30)
		{
			[encoder setVertexBuffer:(id<MTLBuffer>)buffer->_buffer offset:0 atIndex:_internals->currentRenderState->vertexPositionBufferShaderResourceIndex];
		}

		if(_internals->currentRenderState->vertexBufferShaderResourceIndex <= 30)
		{
			[encoder setVertexBuffer:(id<MTLBuffer>)buffer->_buffer offset:mesh.GetVertexPositionsSeparatedSize() atIndex:_internals->currentRenderState->vertexBufferShaderResourceIndex];
		}

		DrawMode drawMode = mesh.GetDrawMode();
		MTLPrimitiveType primitiveType;

		switch(drawMode)
		{
			case DrawMode::Point:
				primitiveType = MTLPrimitiveTypePoint;
				break;
			case DrawMode::Line:
				primitiveType = MTLPrimitiveTypeLine;
				break;
			case DrawMode::LineStrip:
				primitiveType = MTLPrimitiveTypeLineStrip;
				break;
			case DrawMode::Triangle:
				primitiveType = MTLPrimitiveTypeTriangle;
				break;
			case DrawMode::TriangleStrip:
				primitiveType = MTLPrimitiveTypeTriangleStrip;
				break;
		}

		if(mesh.GetIndicesCount() > 0)
		{
			MetalGPUBuffer *indexBuffer = static_cast<MetalGPUBuffer *>(meshBuffers.GetIndicesBuffer());
			MTLIndexType indexType = mesh.GetIndexType() == PrimitiveType::Uint16? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;

			if(instanceCount == 1)
			{
				[encoder drawIndexedPrimitives:primitiveType indexCount:mesh.GetIndicesCount() indexType:indexType indexBuffer:(id <MTLBuffer>)indexBuffer->_buffer indexBufferOffset:0];
			}
			else
			{
				[encoder drawIndexedPrimitives:primitiveType indexCount:mesh.GetIndicesCount() indexType:indexType indexBuffer:(id <MTLBuffer>)indexBuffer->_buffer indexBufferOffset:0 instanceCount:instanceCount];
			}
		}
		else
		{
			if(instanceCount == 1)
			{
				[encoder drawPrimitives:primitiveType vertexStart:0 vertexCount:mesh.GetVerticesCount()];
			}
			else
			{
				[encoder drawPrimitives:primitiveType vertexStart:0 vertexCount:mesh.GetVerticesCount() instanceCount:instanceCount];
			}
		}
	}
}
