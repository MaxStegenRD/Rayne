//
//  RNSceneAttachment.h
//  Rayne
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_SCENEATTACHMENT_H__
#define __RAYNE_SCENEATTACHMENT_H__

#include "../Base/RNBase.h"
#include "../Objects/RNObject.h"

namespace RN
{
	class Scene;
	class Camera;
	class Light;
	class Renderer;

	struct SceneCameraPassContext
	{
		SceneCameraPassContext(const std::vector<Light *> &visibleLights) :
			visibleLights(visibleLights)
		{}

		const std::vector<Light *> &visibleLights;
	};

	class SceneAttachment : public Object
	{
	public:
		friend class SceneManager;
		friend class Scene;

		RNAPI ~SceneAttachment();

		RNAPI Scene *GetParent() const { return _scene; }

	protected:
		RNAPI SceneAttachment();
		RNAPI virtual void DidAttachToRenderer(Renderer *renderer);
		RNAPI virtual void Update(float delta);
		RNAPI virtual void WillUpdate(float delta);
		RNAPI virtual void DidUpdate(float delta);
		RNAPI virtual void WillRender(Renderer *renderer);
		RNAPI virtual void DidRender(Renderer *renderer);
		RNAPI virtual void SubmitCameraPassAttachmentSnapshots(Renderer *renderer, Camera *camera, const SceneCameraPassContext &context);

	private:
		void __AttachToRenderer(Renderer *renderer);

		Scene *_scene;
		Renderer *_attachedRenderer;

		__RNDeclareMetaInternal(SceneAttachment)
	};
} // namespace RN


#endif /* __RAYNE_SCENEATTACHMENT_H__ */
