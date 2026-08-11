//
//  RNShadowSceneAttachment.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNShadowSceneAttachment.h"
#include "RNCamera.h"
#include "RNLight.h"
#include "RNScene.h"
#include "../Rendering/RNRenderer.h"
#include "../Rendering/RNShadowPassSnapshot.h"
#include "../Rendering/RNShadowRendererAttachment.h"

namespace RN
{
	RNDefineMeta(ShadowSceneAttachment, SceneAttachment)

	void ShadowSceneAttachment::AttachToScene(Scene *scene)
	{
		RN_ASSERT(scene, "Scene mustn't be NULL");

		ShadowSceneAttachment *attachment = new ShadowSceneAttachment();
		scene->AddAttachment(attachment);
		SafeRelease(attachment);
	}

	ShadowSceneAttachment::ShadowSceneAttachment()
	{}

	ShadowSceneAttachment::~ShadowSceneAttachment()
	{}

	void ShadowSceneAttachment::DidAttachToRenderer(Renderer *renderer)
	{
		if(renderer->HasAttachment(ShadowRendererAttachment::GetMetaClass()))
			return;

		ShadowRendererAttachment *attachment = new ShadowRendererAttachment();
		renderer->AddAttachment(attachment);
		SafeRelease(attachment);
	}

	void ShadowSceneAttachment::SubmitCameraPassAttachmentSnapshots(Renderer *renderer, Camera *camera, const SceneCameraPassContext &context)
	{
		RN_ASSERT(renderer, "Renderer mustn't be NULL");

		Light *shadowLight = nullptr;
		for(Light *light : context.visibleLights)
		{
			if(light->GetType() == Light::Type::DirectionalLight && light->HasShadows())
				shadowLight = light;
		}

		if(!shadowLight)
			return;

		std::vector<Matrix> directionalShadowMatrices;
		std::vector<uint64> shadowCameraUIDs;
		Matrix projectionCorrection = renderer->GetProjectionCorrectionMatrix();

		shadowLight->GetShadowDepthCameras()->Enumerate<Camera>([&](Camera *shadowCamera, size_t index, bool &stop) {
			Matrix shadowMatrix = projectionCorrection * shadowCamera->GetProjectionMatrix();
			shadowMatrix = shadowMatrix * shadowCamera->GetInverseWorldTransform(camera->GetRenderOrigin());

			directionalShadowMatrices.push_back(shadowMatrix);
			shadowCameraUIDs.push_back(shadowCamera->GetUID());
		});

		if(Camera *shadowParentCamera = shadowLight->GetMultiviewShadowParentCamera())
		{
			shadowCameraUIDs.push_back(shadowParentCamera->GetUID());
		}

		Vector2 directionalShadowInfo(1.0f / shadowLight->GetShadowParameters().resolution);
		ShadowPassSnapshot *snapshot = new ShadowPassSnapshot(shadowLight->GetShadowDepthTexture(), directionalShadowMatrices, directionalShadowInfo, shadowCameraUIDs);
		renderer->SubmitCameraPassAttachmentSnapshot(snapshot);
		SafeRelease(snapshot);
	}
}
