//
//  RNSceneAttachment.cpp
//  Rayne
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNSceneAttachment.h"

namespace RN
{
	RNDefineMeta(SceneAttachment, Object)

	SceneAttachment::SceneAttachment() :
		_scene(nullptr),
		_attachedRenderer(nullptr)
	{}
	SceneAttachment::~SceneAttachment()
	{}

	void SceneAttachment::__AttachToRenderer(Renderer *renderer)
	{
		RN_ASSERT(renderer, "Renderer mustn't be NULL");

		if(_attachedRenderer)
		{
			RN_ASSERT(_attachedRenderer == renderer, "SceneAttachment is already attached to a different renderer");
			return;
		}

		_attachedRenderer = renderer;
		DidAttachToRenderer(renderer);
	}

	void SceneAttachment::DidAttachToRenderer(Renderer *renderer)
	{
	}

	void SceneAttachment::Update(float delta)
	{
	}

	void SceneAttachment::WillUpdate(float delta)
	{
	}

	void SceneAttachment::DidUpdate(float delta)
	{
	}

	void SceneAttachment::WillRender(Renderer *renderer)
	{
	}

	void SceneAttachment::DidRender(Renderer *renderer)
	{
	}

	void SceneAttachment::SubmitCameraPassAttachmentSnapshots(Renderer *renderer, Camera *camera)
	{
	}
} // namespace RN
