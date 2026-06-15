//
//  RNScene.h
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_SCENE_H__
#define __RAYNE_SCENE_H__

#include "../Base/RNBase.h"
#include "../Objects/RNObject.h"
#include "../Rendering/RNRenderer.h"
#include "RNCamera.h"
#include "RNLight.h"
#include "RNSceneAttachment.h"
#include "RNSceneNode.h"

namespace RN
{
	class Scene : public Object
	{
	public:
		friend class Kernel;
		friend class SceneManager;
		friend class SceneInfo;

		RNAPI ~Scene();

		RNAPI virtual void AddNode(SceneNode *node) = 0;
		RNAPI virtual void RemoveNode(SceneNode *node) = 0;

		RNAPI void AddAttachment(SceneAttachment *attachment);
		RNAPI void RemoveAttachment(SceneAttachment *attachment);

		RNAPI void SetUniverseOrigin(const DVector3 &origin);
		RNAPI void ShiftUniverseOrigin(const DVector3 &shift);
		RNAPI DVector3 GetUniverseOrigin() const;
		RNAPI uint64 GetUniverseOriginVersion() const;
		RNAPI Vector3 ConvertUniversePositionToWorldPosition(const DVector3 &position) const;
		RNAPI DVector3 ConvertWorldPositionToUniversePosition(const Vector3 &position) const;

	protected:
		RNAPI Scene();

		RNAPI virtual void WillBecomeActive();
		RNAPI virtual void DidBecomeActive();

		RNAPI virtual void WillResignActive();
		RNAPI virtual void DidResignActive();

		RNAPI virtual void WillUpdate(float delta);
		RNAPI virtual void DidUpdate(float delta);

		RNAPI virtual void WillRender(Renderer *renderer);
		RNAPI virtual void DidRender(Renderer *renderer);
		RNAPI void SubmitCameraPassAttachmentSnapshots(Renderer *renderer, Camera *camera, const SceneCameraPassContext &context);

		RNAPI virtual void PrepareForShutdown();
		RNAPI virtual void Update(float delta);
		RNAPI virtual void Render(Renderer *renderer) = 0;

		RNAPI void UpdateNode(SceneNode *node, float delta);

		bool _isPreparedForShutdown;

	private:
#if RN_ENABLE_UNIVERSE_SCALE
		DVector3 _universeOrigin;
		uint64 _universeOriginVersion;
#endif
		Array *_attachments;

		__RNDeclareMetaInternal(Scene)
	};

	class SceneInfo : public Object
	{
	public:
		RNAPI SceneInfo(Scene *scene);
		RNAPI ~SceneInfo();

		RNAPI Scene *GetScene() const;
		uint64 GetUniverseOriginVersion() const
		{
#if RN_ENABLE_UNIVERSE_SCALE
			return _scene->_universeOriginVersion;
#else
			return 0;
#endif
		}

	private:
		Scene *_scene;

		__RNDeclareMetaInternal(SceneInfo)
	};
} // namespace RN


#endif /* __RAYNE_SCENE_H__ */
