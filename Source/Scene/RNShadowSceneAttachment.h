//
//  RNShadowSceneAttachment.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_SHADOW_SCENE_ATTACHMENT_H__
#define __RAYNE_SHADOW_SCENE_ATTACHMENT_H__

#include "RNSceneAttachment.h"

namespace RN
{
	class Scene;

	class ShadowSceneAttachment : public SceneAttachment
	{
	public:
		RNAPI static void AttachToScene(Scene *scene);

		RNAPI ShadowSceneAttachment();
		RNAPI ~ShadowSceneAttachment() override;

	protected:
		RNAPI void DidAttachToRenderer(Renderer *renderer) override;
		RNAPI void SubmitCameraPassAttachmentSnapshots(Renderer *renderer, Camera *camera, const SceneCameraPassContext &context) override;

	private:
		__RNDeclareMetaInternal(ShadowSceneAttachment)
	};
}

#endif /* __RAYNE_SHADOW_SCENE_ATTACHMENT_H__ */
