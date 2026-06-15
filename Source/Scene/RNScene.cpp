//
//  RNScene.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNScene.h"

namespace RN
{
	RNDefineMeta(Scene, Object)
	RNDefineMeta(SceneInfo, Object)

	Scene::Scene() :
		_isPreparedForShutdown(false),
#if RN_ENABLE_UNIVERSE_SCALE
		_universeOrigin(0.0),
		_universeOriginVersion(0),
#endif
		_attachments(nullptr)
	{
	}

	Scene::~Scene()
	{
		if(_attachments)
			_attachments->Release();
	}

	void Scene::Update(float delta)
	{
		//Update scene attachments
		if(_attachments)
		{
			_attachments->Enumerate<SceneAttachment>([delta](SceneAttachment *attachment, size_t index, bool &stop) {
				attachment->Update(delta);
			});
		}
	}

	void Scene::PrepareForShutdown()
	{
		_isPreparedForShutdown = true;
	}

	void Scene::UpdateNode(SceneNode *node, float delta)
	{
		if(!node->HasFlags(RN::SceneNode::Flags::Static))
		{
			node->Update(delta);
		}
	}

	DVector3 Scene::GetUniverseOrigin() const
	{
#if RN_ENABLE_UNIVERSE_SCALE
		return _universeOrigin;
#else
		return DVector3(0.0);
#endif
	}

	uint64 Scene::GetUniverseOriginVersion() const
	{
#if RN_ENABLE_UNIVERSE_SCALE
		return _universeOriginVersion;
#else
		return 0;
#endif
	}

	void Scene::SetUniverseOrigin(const DVector3 &origin)
	{
#if RN_ENABLE_UNIVERSE_SCALE
		if(!origin.IsValid()) return;
		if(origin == _universeOrigin) return;

		_universeOrigin = origin;
		_universeOriginVersion += 1;
#else
		(void)origin;
#endif
	}

	void Scene::ShiftUniverseOrigin(const DVector3 &shift)
	{
#if RN_ENABLE_UNIVERSE_SCALE
		if(!shift.IsValid()) return;
		SetUniverseOrigin(_universeOrigin + shift);
#else
		(void)shift;
#endif
	}

	Vector3 Scene::ConvertUniversePositionToWorldPosition(const DVector3 &position) const
	{
#if RN_ENABLE_UNIVERSE_SCALE
		return (position - _universeOrigin).ToVector3();
#else
		return position.ToVector3();
#endif
	}

	DVector3 Scene::ConvertWorldPositionToUniversePosition(const Vector3 &position) const
	{
#if RN_ENABLE_UNIVERSE_SCALE
		return _universeOrigin + DVector3(position);
#else
		return DVector3(position);
#endif
	}

	void Scene::AddAttachment(SceneAttachment *attachment)
	{
		RN_ASSERT(attachment->_scene == nullptr, "AddAttachment() must be called on an Attachment not owned by the scene");

		if(!_attachments)
			_attachments = new Array();

		_attachments->AddObject(attachment);
		attachment->_scene = this;
	}

	void Scene::RemoveAttachment(SceneAttachment *attachment)
	{
		RN_ASSERT(attachment->_scene == this, "RemoveAttachment() must be called on an Attachment owned by the scene");

		attachment->_scene = nullptr;
		_attachments->RemoveObject(attachment);
	}

	void Scene::WillBecomeActive()
	{}
	void Scene::DidBecomeActive()
	{}

	void Scene::WillResignActive()
	{}
	void Scene::DidResignActive()
	{}

	void Scene::WillUpdate(float delta)
	{
		//Update scene attachments
		if(_attachments)
		{
			_attachments->Enumerate<SceneAttachment>([delta](SceneAttachment *attachment, size_t index, bool &stop) {
				attachment->WillUpdate(delta);
			});
		}
	}
	void Scene::DidUpdate(float delta)
	{
		//Update scene attachments
		if(_attachments)
		{
			_attachments->Enumerate<SceneAttachment>([delta](SceneAttachment *attachment, size_t index, bool &stop) {
				attachment->DidUpdate(delta);
			});
		}
	}
	void Scene::WillRender(Renderer *renderer)
	{
		if(_attachments)
		{
			_attachments->Enumerate<SceneAttachment>([renderer](SceneAttachment *attachment, size_t index, bool &stop) {
				attachment->__AttachToRenderer(renderer);
				attachment->WillRender(renderer);
			});
		}
	}
	void Scene::DidRender(Renderer *renderer)
	{
		if(_attachments)
		{
			_attachments->Enumerate<SceneAttachment>([renderer](SceneAttachment *attachment, size_t index, bool &stop) {
				attachment->DidRender(renderer);
			});
		}
	}

	void Scene::SubmitCameraPassAttachmentSnapshots(Renderer *renderer, Camera *camera, const SceneCameraPassContext &context)
	{
		if(_attachments)
		{
			_attachments->Enumerate<SceneAttachment>([renderer, camera, &context](SceneAttachment *attachment, size_t index, bool &stop) {
				attachment->SubmitCameraPassAttachmentSnapshots(renderer, camera, context);
			});
		}
	}


	SceneInfo::SceneInfo(Scene *scene) :
		_scene(scene)
	{
	}

	SceneInfo::~SceneInfo()
	{
	}

	Scene *SceneInfo::GetScene() const
	{
		return _scene;
	}
} // namespace RN
