//
//  RNVRCamera.cpp
//  Rayne-VR
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNVRCamera.h"

namespace RN
{
	RNDefineMeta(VRCamera, SceneNode)

	VRCamera::VRCamera(VRWindow *window, RenderPass *previewRenderPass, uint8 msaaSampleCount, Window *debugWindow, bool supportInputAttachments) :
		_window(window ? window->Retain() : nullptr),
		_debugWindow(debugWindow ? debugWindow->Retain() : nullptr),
		_head(new Camera()),
		_eye {nullptr, nullptr},
		_hiddenAreaEntity {nullptr, nullptr},
		_previewRenderPass(previewRenderPass ? previewRenderPass->Retain() : nullptr),
		_msaaSampleCount(msaaSampleCount),
		_supportInputAttachments(supportInputAttachments),
		_didUpdateVRWindow(false),
		_followsTracking(true)
	{
		SetUpdatePriority(SceneNode::UpdatePriority::UpdateEarly);
		AddChild(_head);
		SetupCameras();
	}

	VRCamera::~VRCamera()
	{
		NotificationManager::GetSharedInstance()->RemoveSubscriber(kRNWindowDidChangeSize, this);
		NotificationManager::GetSharedInstance()->RemoveSubscriber(kRNVRVisibilityMaskChanged, this);

		SafeRelease(_previewRenderPass);
		SafeRelease(_window);
		SafeRelease(_debugWindow);
		for(size_t i = 0; i < 2; i++)
		{
			RemoveHiddenAreaEntity(i);
		}
		SafeRelease(_head);
		SafeRelease(_eye[0]);
		SafeRelease(_eye[1]);
	}

	void VRCamera::SetupCameras()
	{
		if(!_window && !_debugWindow) return;

		if(_window && !_window->IsRendering()) return;

		if(_debugWindow)
		{
			_debugWindow->SetTitle(RNCSTR("VR Debug Window"));
			_debugWindow->Show();
		}

		//_head->AddFlags(Camera::Flags::UseSimpleCulling);
		_head->GetRenderPass()->SetShaderHint(Shader::UsageHint::Multiview);
		_head->SetRenderGroup(0x1 | GetHiddenAreaRenderGroupMask());
		_head->SetFOV(110.0f);

		for(size_t i = 0; i < _window->GetEyeCount() && i < 2; i++)
		{
			_eye[i] = new Camera();
			_eye[i]->SetRenderGroup(0x1 | GetHiddenAreaRenderGroup(i));
			_head->AddChild(_eye[i]);
			_head->AddMultiviewCamera(_eye[i]);
			_hiddenAreaEntity[i] = nullptr;
		}

		if(_eye[0]) _eye[0]->SetPosition(Vector3(-0.032f, 0.0f, 0.0f));
		if(_eye[1]) _eye[1]->SetPosition(Vector3(0.032f, 0.0f, 0.0f));

		//RebuildHiddenAreaMeshes(); //TODO: Bring back and fix once quest supports it again
		CreatePostprocessingPipeline();

		NotificationManager::GetSharedInstance()->AddSubscriber(kRNWindowDidChangeSize, [this](Notification *notification) {
			if(notification->GetInfo<VRWindow>() == _window)
			{
				CreatePostprocessingPipeline();
			} }, this);

		/*NotificationManager::GetSharedInstance()->AddSubscriber(kRNVRVisibilityMaskChanged, [this](Notification *notification) {
			if(notification->GetInfo<VRWindow>() == _window)
			{
				RebuildHiddenAreaMeshes();
			} }, this);*/ //TODO: Bring back and fix once quest supports it again
	}

	void VRCamera::CreatePostprocessingPipeline()
	{
		Framebuffer *resolvedFramebuffer = _window->GetFramebuffer();
		RN_ASSERT(resolvedFramebuffer, "The VRWindow has no framebuffer!");

		RenderPass *headRenderPass = _head->GetRenderPass();
		headRenderPass->RemoveAllFramePasses();

		Vector2 windowSize;
		Texture::Format colorFormat = Texture::Format::RGBA_8_SRGB;
		Texture::Format depthFormat = Texture::Format::Invalid;
		uint8 layerCount = 1;

		if(_window)
		{
			windowSize = _window->GetSize();

			colorFormat = _window->GetSwapChainDescriptor().colorFormat;
			depthFormat = _window->GetSwapChainDescriptor().depthStencilFormat;

			layerCount = _window->GetSwapChainDescriptor().layerCount;
		}
		bool useSubsampledLayout = _window->GetUsesSubsampledLayout();

		PostProcessingStage *sideBySideDebugPass = nullptr;
		if(_debugWindow)
		{
			//windowSize = _debugWindow->GetSize();

			//colorFormat = _debugWindow->GetSwapChainDescriptor().colorFormat;
			//depthFormat = _debugWindow->GetSwapChainDescriptor().depthStencilFormat;

			sideBySideDebugPass = new PostProcessingStage();
			Material *copyMultiviewToSideBySideDebugMaterial = Material::WithShaders(Renderer::GetActiveRenderer()->GetDefaultShaderLibrary()->GetShaderWithName(RNCSTR("pp_vertex")), Renderer::GetActiveRenderer()->GetDefaultShaderLibrary()->GetShaderWithName(RNCSTR("pp_blit_fragment"), Shader::Options::WithNone()->AddDefine("RN_PP_VR", "1")));
			sideBySideDebugPass->SetFramebuffer(_debugWindow->GetFramebuffer());
			sideBySideDebugPass->SetMaterial(copyMultiviewToSideBySideDebugMaterial);
			sideBySideDebugPass->Autorelease();
		}

		Framebuffer *msaaFramebuffer = nullptr;
		PostProcessingAPIStage *resolvePass = nullptr;

		if(depthFormat == Texture::Format::Invalid)
		{
			depthFormat = Texture::Format::Depth_32F;
		}

		if(_msaaSampleCount > 1)
		{
			Texture::Descriptor msaaColorTextureDescriptor = Texture::Descriptor::With2DRenderTargetFormatAndMSAA(colorFormat, windowSize.x, windowSize.y, _msaaSampleCount, 0, useSubsampledLayout);
			if(_supportInputAttachments) msaaColorTextureDescriptor.usageHint |= Texture::UsageHint::InputAttachment;
			msaaColorTextureDescriptor.depth = layerCount;
			msaaColorTextureDescriptor.type = layerCount > 1 ? Texture::Type::Type2DArray : Texture::Type::Type2D;
			Texture *msaaTexture = Texture::WithDescriptor(msaaColorTextureDescriptor);

			Texture::Descriptor msaaDepthTextureDescriptor = Texture::Descriptor::With2DRenderTargetFormatAndMSAA(depthFormat, windowSize.x, windowSize.y, _msaaSampleCount, 0, useSubsampledLayout);
			if(_supportInputAttachments) msaaDepthTextureDescriptor.usageHint |= Texture::UsageHint::InputAttachment;
			msaaDepthTextureDescriptor.depth = layerCount;
			msaaDepthTextureDescriptor.type = layerCount > 1 ? Texture::Type::Type2DArray : Texture::Type::Type2D;
			Texture *msaaDepthTexture = Texture::WithDescriptor(msaaDepthTextureDescriptor);

			msaaFramebuffer = Renderer::GetActiveRenderer()->CreateFramebuffer(windowSize);
			msaaFramebuffer->SetColorTarget(Framebuffer::TargetView::WithTexture(msaaTexture));
			msaaFramebuffer->SetDepthStencilTarget(Framebuffer::TargetView::WithTexture(msaaDepthTexture));
		}

		//TODO: Depth buffer handling for Android with 2 resolve buffers
		if(_msaaSampleCount <= 1 && (!_window || _window->GetSwapChainDescriptor().depthStencilFormat == Texture::Format::Invalid || _debugWindow))
		{
			Texture::Descriptor depthTextureDescriptor = Texture::Descriptor::With2DRenderTargetFormat(depthFormat, windowSize.x, windowSize.y, useSubsampledLayout);
			if(_supportInputAttachments) depthTextureDescriptor.usageHint |= Texture::UsageHint::InputAttachment;
			depthTextureDescriptor.depth = layerCount;
			depthTextureDescriptor.type = layerCount > 1 ? Texture::Type::Type2DArray : Texture::Type::Type2D;

			Texture *resolvedDepthTexture = Texture::WithDescriptor(depthTextureDescriptor);
			resolvedFramebuffer->SetDepthStencilTarget(Framebuffer::TargetView::WithTexture(resolvedDepthTexture));
		}

		if(_msaaSampleCount > 1)
		{
			resolvePass = new PostProcessingAPIStage(PostProcessingAPIStage::Type::ResolveMSAA);
			resolvePass->SetFramebuffer(resolvedFramebuffer);

			headRenderPass->SetFramebuffer(msaaFramebuffer->Autorelease());
			headRenderPass->AddFramePass(resolvePass->Autorelease());
		}
		else
		{
			headRenderPass->SetFramebuffer(resolvedFramebuffer);
		}

		if(sideBySideDebugPass)
		{
			if(resolvePass)
			{
				resolvePass->AddFramePass(sideBySideDebugPass);
			}
			else
			{
				headRenderPass->AddFramePass(sideBySideDebugPass);
			}
		}

		if(_previewRenderPass)
		{
			if(resolvePass)
			{
				resolvePass->AddFramePass(_previewRenderPass);
			}
			else
			{
				headRenderPass->AddFramePass(_previewRenderPass);
			}
		}
	}

	void VRCamera::RemoveHiddenAreaEntity(size_t eye)
	{
		if(_hiddenAreaEntity[eye])
		{
			_hiddenAreaEntity[eye]->RemoveFromParent();
			SafeRelease(_hiddenAreaEntity[eye]);
			_hiddenAreaEntity[eye] = nullptr;
		}
	}

	void VRCamera::RebuildHiddenAreaMeshes()
	{
#if !RN_PLATFORM_WINDOWS
		if(!_window || !Renderer::GetActiveRenderer()) return;

		ShaderLibrary *shaderLibrary = Renderer::GetActiveRenderer()->GetDefaultShaderLibrary();
		for(size_t i = 0; i < _window->GetEyeCount() && i < 2; i++)
		{
			RemoveHiddenAreaEntity(i);

			Mesh *hiddenAreaMesh = _window->GetHiddenAreaMesh(i);
			if(!hiddenAreaMesh || !_eye[i]) continue;

			Shader::Options *shaderOptions = Shader::Options::WithMesh(hiddenAreaMesh);
			Shader::Options *multiviewShaderOptions = shaderOptions->Copy()->Autorelease();
			multiviewShaderOptions->EnableMultiview();

			Material *hiddenAreaMaterial = Material::WithShaders(shaderLibrary->GetShaderWithName(RNCSTR("pp_mask_vertex"), shaderOptions), shaderLibrary->GetShaderWithName(RNCSTR("pp_mask_fragment")));
			hiddenAreaMaterial->SetVertexShader(shaderLibrary->GetShaderWithName(RNCSTR("pp_mask_vertex"), multiviewShaderOptions), Shader::UsageHint::Multiview);
			hiddenAreaMaterial->SetFragmentShader(shaderLibrary->GetShaderWithName(RNCSTR("pp_mask_fragment")), Shader::UsageHint::Multiview);
			hiddenAreaMaterial->SetCustomShaderUniform(RNCSTR("maskEyeIndex"), Number::WithUint32(static_cast<uint32>(i)));
			hiddenAreaMaterial->SetCullMode(CullMode::None);
			hiddenAreaMaterial->SetDepthMode(DepthMode::Always);
			hiddenAreaMaterial->SetDepthWriteEnabled(true);
			hiddenAreaMaterial->SetColorWriteMask(false, false, false, false);

			Model *hiddenAreaModel = new Model(hiddenAreaMesh, hiddenAreaMaterial);
			_hiddenAreaEntity[i] = new Entity(hiddenAreaModel->Autorelease());
			_hiddenAreaEntity[i]->SetRenderPriority(SceneNode::RenderEarly);
			_hiddenAreaEntity[i]->AddFlags(SceneNode::Flags::NoCulling);
			_hiddenAreaEntity[i]->SetRenderGroup(GetHiddenAreaRenderGroup(i));

			_eye[i]->AddChild(_hiddenAreaEntity[i]);
		}
#endif
	}

	void VRCamera::UpdateVRWindow(float delta)
	{
		if(_didUpdateVRWindow || !_window || !_eye[0]) return;
		_window->Update(delta, _eye[0]->GetClipNear(), _eye[0]->GetClipFar());
		_didUpdateVRWindow = true;
	}

	void VRCamera::UpdateHeadFrustumPlaneOffsets()
	{
		_head->SetFrustumPlaneOffset(0.0f, 0.0f, 0.0f, 0.0f);

		Vector4 headFrustumPlanes[6];
		_head->GetFrustumPlanes(headFrustumPlanes);

		// Top, bottom, left, right; bottom/right are negated when passed back to Camera.
		const size_t planeIndices[4] = {2, 3, 0, 1};
		Vector3 headPlaneNormals[4];
		for(size_t plane = 0; plane < 4; plane++)
		{
			headPlaneNormals[plane] = Vector3(headFrustumPlanes[planeIndices[plane]]);
		}

		float outsideDistances[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		auto includePoint = [&](const Vector3 &point) {
			for(size_t plane = 0; plane < 4; plane++)
			{
				const Vector4 &headPlane = headFrustumPlanes[planeIndices[plane]];
				float distance = point.GetDotProduct(headPlaneNormals[plane]) + headPlane.w;
				if(distance < outsideDistances[plane]) outsideDistances[plane] = distance;
			}
		};
		auto includeInfiniteRay = [&](const Vector3 &ray) {
			for(size_t plane = 0; plane < 4; plane++)
			{
				if(ray.GetDotProduct(headPlaneNormals[plane]) < -k::EpsilonFloat)
				{
					outsideDistances[plane] = -FLT_MAX;
				}
			}
		};

		for(size_t i = 0; i < _window->GetEyeCount() && i < 2; i++)
		{
			Camera *eye = _eye[i];
			if(!eye) continue;

			const bool hasInfiniteFarPlane = isinf(eye->GetClipFar());
			const float depths[2] = {eye->GetClipNear(), eye->GetClipFar()};
			const size_t depthCount = hasInfiniteFarPlane ? 1 : 2;
			for(size_t depthIndex = 0; depthIndex < depthCount; depthIndex++)
			{
				for(int x = -1; x <= 1; x += 2)
				{
					for(int y = -1; y <= 1; y += 2)
					{
						Vector3 corner = eye->ToRender(Vector3(static_cast<float>(x), static_cast<float>(y), depths[depthIndex]));
						includePoint(corner);

						if(hasInfiniteFarPlane)
						{
							includeInfiniteRay(corner - eye->GetRenderPosition());
						}
					}
				}
			}
		}

		_head->SetFrustumPlaneOffset(outsideDistances[0], -outsideDistances[1], outsideDistances[2], -outsideDistances[3]);
	}

	void VRCamera::Update(float delta)
	{
		SceneNode::Update(delta);

		if(!_window) return;

		UpdateVRWindow(delta);

		const VRHMDTrackingState &hmdState = GetHMDTrackingState();

		for(size_t i = 0; i < _window->GetEyeCount() && i < 2; i++)
		{
			_eye[i]->SetPosition(hmdState.eyeOffset[i]);
			_eye[i]->SetRotation(hmdState.eyeRotation[i]);
			_eye[i]->SetProjectionMatrix(hmdState.eyeProjection[i]);
		}

		if(_followsTracking)
		{
			_head->SetRotation(hmdState.rotation);
			_head->SetPosition(hmdState.position);
		}

		UpdateHeadFrustumPlaneOffsets();

		_didUpdateVRWindow = false;
	}

	VRHMDTrackingState VRCamera::GetHMDTrackingState() const
	{
		VRHMDTrackingState trackingState = _window->GetHMDTrackingState();
		trackingState.position += _originPositionOffset;
		trackingState.position = _originalOrientationOffset.GetRotatedVector(trackingState.position);
		trackingState.rotation = _originalOrientationOffset * trackingState.rotation;

		return trackingState;
	}

	VRControllerTrackingState VRCamera::GetControllerTrackingState(uint8 index) const
	{
		VRControllerTrackingState trackingState = _window->GetControllerTrackingState(index);
		trackingState.positionAim += _originPositionOffset;
		trackingState.positionAim = _originalOrientationOffset.GetRotatedVector(trackingState.positionAim);
		trackingState.rotationAim = _originalOrientationOffset * trackingState.rotationAim;
		trackingState.positionGrip += _originPositionOffset;
		trackingState.positionGrip = _originalOrientationOffset.GetRotatedVector(trackingState.positionGrip);
		trackingState.rotationGrip = _originalOrientationOffset * trackingState.rotationGrip;

		trackingState.velocityLinear = _originalOrientationOffset.GetRotatedVector(trackingState.velocityLinear);

		return trackingState;
	}

	VRControllerTrackingState VRCamera::GetTrackerTrackingState(uint8 index) const
	{
		VRControllerTrackingState trackingState = _window->GetTrackerTrackingState(index);
		trackingState.positionAim += _originPositionOffset;
		trackingState.positionAim = _originalOrientationOffset.GetRotatedVector(trackingState.positionAim);
		trackingState.rotationAim = _originalOrientationOffset * trackingState.rotationAim;
		trackingState.positionGrip += _originPositionOffset;
		trackingState.positionGrip = _originalOrientationOffset.GetRotatedVector(trackingState.positionGrip);
		trackingState.rotationGrip = _originalOrientationOffset * trackingState.rotationGrip;

		trackingState.velocityLinear = _originalOrientationOffset.GetRotatedVector(trackingState.velocityLinear);

		return trackingState;
	}

	VRHandTrackingState VRCamera::GetHandTrackingState(uint8 index) const
	{
		VRHandTrackingState trackingState = _window->GetHandTrackingState(index);
		for(size_t jointIndex = 0; jointIndex < VRHandTrackingState::Joint::_JointCount; jointIndex++)
		{
			trackingState.joints[jointIndex].position += _originPositionOffset;
			trackingState.joints[jointIndex].position = _originalOrientationOffset.GetRotatedVector(trackingState.joints[jointIndex].position);
			trackingState.joints[jointIndex].rotation = _originalOrientationOffset * trackingState.joints[jointIndex].rotation;
		}

		return trackingState;
	}

	void VRCamera::SubmitControllerHaptics(uint8 index, VRControllerHaptics &haptics) const
	{
		_window->SubmitControllerHaptics(index, haptics);
	}

	const VRWindow::Origin VRCamera::GetOrigin() const
	{
		return _window->GetOrigin();
	}

	void VRCamera::SetClipFar(float clipFar)
	{
		_head->SetClipFar(clipFar);

		if(_eye[0]) _eye[0]->SetClipFar(clipFar);
		if(_eye[1]) _eye[1]->SetClipFar(clipFar);
	}

	void VRCamera::SetClipFarUnlimited()
	{
		_head->SetClipFarUnlimited();

		if(_eye[0]) _eye[0]->SetClipFarUnlimited();
		if(_eye[1]) _eye[1]->SetClipFarUnlimited();
	}

	void VRCamera::SetClipNear(float clipNear)
	{
		_head->SetClipNear(clipNear);

		if(_eye[0]) _eye[0]->SetClipNear(clipNear);
		if(_eye[1]) _eye[1]->SetClipNear(clipNear);
	}

	void VRCamera::SetOriginOffset(const Vector3 &positionOffset, const Quaternion &orientationOffset)
	{
		_originPositionOffset = positionOffset;
		_originalOrientationOffset = orientationOffset;
	}
} // namespace RN
