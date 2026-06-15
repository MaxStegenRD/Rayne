//
//  RNSceneNodeAttachment.cpp
//  Rayne
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNSceneNodeAttachment.h"

namespace RN
{
	RNDefineMeta(SceneNodeAttachment, Object)

	SceneNodeAttachment::SceneNodeAttachment() :
		_node(nullptr), _consumeChangeSets(0)
	{
	}

	SceneNodeAttachment::~SceneNodeAttachment()
	{
	}

	void SceneNodeAttachment::Update(float delta)
	{
	}

	SceneNode *SceneNodeAttachment::GetParent() const
	{
		return _node;
	}

	void SceneNodeAttachment::__WillUpdate(SceneNode::ChangeSet changeSet)
	{
		if(_consumeChangeSets)
		{
			changeSet &= ~_consumeChangeSets;

			if(changeSet == 0)
				return;
		}

		WillUpdate(changeSet);
	}
	void SceneNodeAttachment::__DidUpdate(SceneNode::ChangeSet changeSet)
	{
		if(_consumeChangeSets)
		{
			changeSet &= ~_consumeChangeSets;

			if(changeSet == 0)
				return;
		}

		DidUpdate(changeSet);
	}


	void SceneNodeAttachment::SetWorldPosition(const Vector3 &position)
	{
		RN_DEBUG_ASSERT(_node, "SceneNodeAttachment must be attached to a SceneNode before setting world position.");
		_consumeChangeSets |= SceneNode::ChangeSet::Position;
		_node->SetWorldPosition(position);
		_consumeChangeSets &= ~SceneNode::ChangeSet::Position;
	}
	void SceneNodeAttachment::SetUniversePosition(const DVector3 &position)
	{
		RN_DEBUG_ASSERT(_node, "SceneNodeAttachment must be attached to a SceneNode before setting universe position.");
		_consumeChangeSets |= SceneNode::ChangeSet::Position;
		_node->SetUniversePosition(position);
		_consumeChangeSets &= ~SceneNode::ChangeSet::Position;
	}
	void SceneNodeAttachment::SetWorldScale(const Vector3 &scale)
	{
		RN_DEBUG_ASSERT(_node, "SceneNodeAttachment must be attached to a SceneNode before setting world scale.");
		_consumeChangeSets |= SceneNode::ChangeSet::Position;
		_node->SetWorldScale(scale);
		_consumeChangeSets &= ~SceneNode::ChangeSet::Position;
	}
	void SceneNodeAttachment::SetWorldRotation(const Quaternion &rotation)
	{
		RN_DEBUG_ASSERT(_node, "SceneNodeAttachment must be attached to a SceneNode before setting world rotation.");
		_consumeChangeSets |= SceneNode::ChangeSet::Position;
		_node->SetWorldRotation(rotation);
		_consumeChangeSets &= ~SceneNode::ChangeSet::Position;
	}

	Vector3 SceneNodeAttachment::GetWorldPosition() const
	{
		RN_DEBUG_ASSERT(_node, "SceneNodeAttachment must be attached to a SceneNode before getting world position.");
		return _node->GetWorldPosition();
	}
	DVector3 SceneNodeAttachment::GetUniversePosition() const
	{
		RN_DEBUG_ASSERT(_node, "SceneNodeAttachment must be attached to a SceneNode before getting universe position.");
		return _node->GetUniversePosition();
	}
	Vector3 SceneNodeAttachment::GetWorldScale() const
	{
		RN_DEBUG_ASSERT(_node, "SceneNodeAttachment must be attached to a SceneNode before getting world scale.");
		return _node->GetWorldScale();
	}
	Quaternion SceneNodeAttachment::GetWorldRotation() const
	{
		RN_DEBUG_ASSERT(_node, "SceneNodeAttachment must be attached to a SceneNode before getting world rotation.");
		return _node->GetWorldRotation();
	}

	Vector3 SceneNodeAttachment::GetForward() const
	{
		RN_DEBUG_ASSERT(_node, "SceneNodeAttachment must be attached to a SceneNode before getting forward vector.");
		return _node->GetForward();
	}
	Vector3 SceneNodeAttachment::GetUp() const
	{
		RN_DEBUG_ASSERT(_node, "SceneNodeAttachment must be attached to a SceneNode before getting up vector.");
		return _node->GetUp();
	}
	Vector3 SceneNodeAttachment::GetRight() const
	{
		RN_DEBUG_ASSERT(_node, "SceneNodeAttachment must be attached to a SceneNode before getting right vector.");
		return _node->GetRight();
	}
} // namespace RN
