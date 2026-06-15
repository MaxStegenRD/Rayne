//
//  RNSceneNode.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNSceneNode.h"
#include "RNScene.h"
#include "RNSceneNodeAttachment.h"

namespace RN
{
	RNDefineMeta(SceneNode, Object)

	std::atomic<uint64> __SceneNodeIDs;

	SceneNode::SceneNode() :
		_sceneUpdateEntry(this),
		_sceneRenderEntry(this),
		_scheduledForRemovalFromScene(false),
		_uid(__SceneNodeIDs.fetch_add(1)),
		_lid(static_cast<uint64>(-1)),
		_sceneInfo(nullptr),
		_tag("tag", 0, &SceneNode::GetTag, &SceneNode::SetTag),
		_position("position", &SceneNode::GetPosition, &SceneNode::SetPosition),
		_scale("scale", Vector3(1.0), &SceneNode::GetScale, &SceneNode::SetScale),
		_rotation("rotation", &SceneNode::GetRotation, &SceneNode::SetRotation),
		_attachments(nullptr)
	{
		Initialize();
		AddObservables({&_tag, &_position, &_rotation, &_scale});
		SetBoundingBox(AABB(Vector3(-1.0f), Vector3(1.0f)));
	}

	SceneNode::SceneNode(const SceneNode::PositionType &position) :
		SceneNode()
	{
		SetPosition(position);
	}

	SceneNode::SceneNode(const SceneNode::PositionType &position, const Quaternion &rotation) :
		SceneNode(position)
	{
		SetRotation(rotation);
	}

	SceneNode::SceneNode(const SceneNode *other) :
		SceneNode()
	{
		SceneNode *temp = const_cast<SceneNode *>(other);

		LockWrapper<Object *> wrapper(temp);
		LockGuard<LockWrapper<Object *>> lock(wrapper);

		SetPosition(other->GetPosition());
		SetRotation(other->GetRotation());
		SetScale(other->GetScale());

		_renderGroup = other->_renderGroup;
		_collisionGroup = other->_collisionGroup;

		_updatePriority = other->_updatePriority;
		_renderPriority = other->_renderPriority;
		_flags = other->_flags.load();
		_tag = other->_tag;

		other->GetChildren()->Enumerate<RN::SceneNode>([&](RN::SceneNode *node, size_t index, bool &stop) {
			SceneNode *nodeCopy = node->Copy();
			AddChild(nodeCopy);
			nodeCopy->Release();
		});
	}

	SceneNode::~SceneNode()
	{
		//Set parent of child nodes to null when it gets released
		_children->Enumerate<SceneNode>([](SceneNode *child, size_t index, bool &stop) {
			child->WillUpdate(ChangeSet::Parent);
			child->_parent = nullptr;
			child->DidUpdate(ChangeSet::Parent);
			child->__CompleteAttachmentWithScene(nullptr);
		});
		_children->Release();

		if(_attachments)
		{
			_attachments->Enumerate<SceneNodeAttachment>([](SceneNodeAttachment *attachment, size_t index, bool &stop) {
				attachment->_node = nullptr;
			});
			_attachments->Release();
		}
	}


	SceneNode::SceneNode(Deserializer *deserializer) :
		SceneNode()
	{
#if RN_ENABLE_UNIVERSE_SCALE
		SetPosition(deserializer->DecodeDVector3());
#else
		SetPosition(deserializer->DecodeVector3());
#endif
		SetScale(deserializer->DecodeVector3());
		SetRotation(deserializer->DecodeQuaternion());

		uint32 groups = static_cast<uint32>(deserializer->DecodeInt32());

		_renderGroup = (groups & 0xffff);
		_collisionGroup = (groups >> 16);

		_updatePriority = static_cast<UpdatePriority>(deserializer->DecodeInt32());
		_renderPriority = deserializer->DecodeInt32();
		_flags = static_cast<Flags>(deserializer->DecodeInt32());

		_tag = static_cast<Tag>(deserializer->DecodeInt64());
		_lid = deserializer->DecodeInt64();

		size_t count = static_cast<size_t>(deserializer->DecodeInt64());
		for(size_t i = 0; i < count; i++)
		{
			SceneNode *child = static_cast<SceneNode *>(deserializer->DecodeObject());

			if(child)
				AddChild(child->Autorelease());
		}
	}


	void SceneNode::Serialize(Serializer *serializer) const
	{
		UpdateInternalData();

#if RN_ENABLE_UNIVERSE_SCALE
		serializer->EncodeDVector3(_position);
#else
		serializer->EncodeVector3(_position);
#endif
		serializer->EncodeVector3(_scale);
		serializer->EncodeQuarternion(_rotation);

		serializer->EncodeInt32(_renderGroup | (_collisionGroup << 16));
		serializer->EncodeInt32(static_cast<int32>(_updatePriority));
		serializer->EncodeInt32(_renderPriority);
		serializer->EncodeInt32(_flags);
		serializer->EncodeInt64(_tag);
		serializer->EncodeInt64(_lid);

		serializer->EncodeInt64(static_cast<uint64>(_children->GetCount()));

		_children->Enumerate<SceneNode>([&](SceneNode *node, size_t index, bool &stop) {
			bool noSave = (node->_flags & Flags::NoSave);

			if(noSave)
				serializer->EncodeConditionalObject(node);
			else
				serializer->EncodeObject(node);
		});
	}


	void SceneNode::Initialize()
	{
		_children = new Array();
		_parent = nullptr;
		_sceneInfo = nullptr;
		_updated = 1;
		_lastUpdatedVersion = 0;
		_lastTransformUpdatedVersion = 0;
		_lastInverseTransformUpdatedVersion = 0;
		_lastBoundsUpdatedVersion = 0;
		_flags = 0;

		_updatePriority = UpdatePriority::UpdateNormal;
		_renderPriority = RenderPriority::RenderNormal;
		_renderGroup = 1;
		_collisionGroup = 0;
	}


	// -------------------
	// MARK: -
	// MARK: Setter
	// -------------------

	void SceneNode::SetFlags(Flags flags)
	{
		WillUpdate(ChangeSet::Flags);
		_flags = flags;
		DidUpdate(ChangeSet::Flags);
	}

	void SceneNode::SetTag(Tag tag)
	{
		WillUpdate(ChangeSet::Tag);
		_tag = tag;
		DidUpdate(ChangeSet::Tag);
	}

	void SceneNode::SetRenderGroup(uint16 group)
	{
		_renderGroup = group;
	}

	void SceneNode::SetCollisionGroup(uint8 group)
	{
		_collisionGroup = group;
	}

	void SceneNode::SetUpdatePriority(UpdatePriority priority)
	{
		RN_ASSERT(_sceneInfo == nullptr, "SetUpdatePriority() must be called before adding the node to the scene.");

		WillUpdate(ChangeSet::UpdatePriority);
		_updatePriority = priority;
		DidUpdate(ChangeSet::UpdatePriority);
	}

	void SceneNode::SetRenderPriority(int32 priority)
	{
		if(_renderPriority == priority) return;
		
		WillUpdate(ChangeSet::RenderPriority);
		_renderPriority = priority;

		if(_sceneInfo && !_scheduledForRemovalFromScene)
		{
			//Readd to scene to update the render priority
			_sceneInfo->GetScene()->RemoveNode(this);
			_sceneInfo->GetScene()->AddNode(this);
		}

		DidUpdate(ChangeSet::RenderPriority);
	}

	void SceneNode::SetBoundingBox(const AABB &boundingBox, bool calculateBoundingSphere)
	{
		WillUpdate(ChangeSet::Position);
		_boundingBox = boundingBox;

		if(calculateBoundingSphere)
			_boundingSphere = Sphere(_boundingBox);

		DidUpdate(ChangeSet::Position);
	}

	void SceneNode::SetBoundingSphere(const Sphere &boundingSphere)
	{
		WillUpdate(ChangeSet::Position);
		_boundingSphere = boundingSphere;
		DidUpdate(ChangeSet::Position);
	}

	void SceneNode::LookAt(const RN::Vector3 &target, bool keepUpAxis)
	{
		const RN::Vector3 &worldPos = GetWorldPosition();

		RN::Quaternion rotation;
		rotation = Quaternion::WithLookAt(worldPos - target, GetUp(), keepUpAxis);

		SetWorldRotation(rotation);
	}

	void SceneNode::SetWorldPosition(const Vector3 &pos)
	{
		if(!_parent)
		{
#if RN_ENABLE_UNIVERSE_SCALE
			Scene *scene = _sceneInfo ? _sceneInfo->GetScene() : nullptr;
			SetPosition(scene ? scene->ConvertWorldPositionToUniversePosition(pos) : DVector3(pos));
#else
			SetPosition(pos);
#endif
			return;
		}

		WillUpdate(ChangeSet::Position);
		Vector3 tempPosition = pos - _parent->GetWorldPosition();
		Quaternion tempRotation = Quaternion() / _parent->GetWorldRotation();
		_position = tempRotation.GetRotatedVector(tempPosition) / _parent->GetWorldScale();
		DidUpdate(ChangeSet::Position);
	}

	void SceneNode::SetUniversePosition(const DVector3 &pos)
	{
#if RN_ENABLE_UNIVERSE_SCALE
		if(_parent)
		{
			DVector3 localPosition = pos - _parent->GetUniversePosition();
			Vector3 localPositionFloat = _parent->GetWorldRotation().GetConjugated().GetRotatedVector(localPosition.ToVector3()) / _parent->GetWorldScale();
			SetPosition(localPositionFloat);
			return;
		}

		SetPosition(pos);
#else
		SetWorldPosition(pos.ToVector3());
#endif
	}

	DVector3 SceneNode::GetUniversePosition() const
	{
#if RN_ENABLE_UNIVERSE_SCALE
		if(_parent)
		{
			DVector3 parentPosition = _parent->GetUniversePosition();
			Vector3 localPosition = _position->ToVector3();
			Vector3 localOffset = _parent->GetWorldScale() * _parent->GetWorldRotation().GetRotatedVector(localPosition);
			return parentPosition + localOffset;
		}

		return _position;
#else
		return DVector3(GetWorldPosition());
#endif
	}

	uint64 SceneNode::GetTransformVersion() const
	{
#if RN_ENABLE_UNIVERSE_SCALE
		return _updated + (_sceneInfo ? _sceneInfo->GetUniverseOriginVersion() : 0);
#else
		return _updated;
#endif
	}

	Vector3 SceneNode::GetPositionForTransform() const
	{
#if RN_ENABLE_UNIVERSE_SCALE
		Scene *scene = _sceneInfo ? _sceneInfo->GetScene() : nullptr;
		if(!_parent && scene)
			return scene->ConvertUniversePositionToWorldPosition(_position);
#endif
		return Vector3(_position);
	}

	SceneNode::Flags SceneNode::RemoveFlags(Flags flags)
	{
		WillUpdate(ChangeSet::Flags);
		return _flags.fetch_and(~flags, std::memory_order_acq_rel) & ~flags;
		DidUpdate(ChangeSet::Flags);
	}
	SceneNode::Flags SceneNode::AddFlags(Flags flags)
	{
		WillUpdate(ChangeSet::Flags);
		return _flags.fetch_or(flags, std::memory_order_acq_rel) | flags;
		DidUpdate(ChangeSet::Flags);
	}

	// -------------------
	// MARK: -
	// MARK: Scene
	// -------------------

	void SceneNode::UpdateSceneInfo(SceneInfo *sceneInfo)
	{
		WillUpdate(ChangeSet::World);

		if(_sceneInfo)
		{
			Scene *scene = _sceneInfo->GetScene();
			_children->Enumerate<SceneNode>([&](SceneNode *node, size_t index, bool &stop) {
				if(node->_sceneInfo != nullptr)
					scene->RemoveNode(node);
			});
		}

		SafeRelease(_sceneInfo);
		_sceneInfo = sceneInfo;
		SafeRetain(_sceneInfo);

		if(_sceneInfo)
		{
			_children->Enumerate<SceneNode>([&](SceneNode *node, size_t index, bool &stop) {
				sceneInfo->GetScene()->AddNode(node);
			});
		}

		DidUpdate(ChangeSet::World);
	}

	void SceneNode::__CompleteAttachmentWithScene(SceneInfo *sceneInfo)
	{
		if((!_scheduledForRemovalFromScene && (_sceneInfo && sceneInfo && _sceneInfo->GetScene() == sceneInfo->GetScene())) || (!_sceneInfo && !sceneInfo))
			return;

		if(_sceneInfo && !_scheduledForRemovalFromScene)
		{
			_sceneInfo->GetScene()->RemoveNode(this);
		}

		if(sceneInfo)
		{
			//If not removed from scene yet, but _scheduledForRemovalFromScene is true, it will just stay where it is (and is removed from the deletion queue)!
			sceneInfo->GetScene()->AddNode(this);
		}
	}

	// -------------------
	// MARK: -
	// MARK: Children
	// -------------------

	void SceneNode::AddChild(SceneNode *child)
	{
		child->RemoveFromParent();

		WillAddChild(child);
		child->WillUpdate(ChangeSet::Parent);

		_children->AddObject(child);

		child->_parent = this;
		child->DidUpdate(ChangeSet::Parent);
		child->__CompleteAttachmentWithScene(_sceneInfo);

		DidAddChild(child);
	}

	void SceneNode::RemoveChild(SceneNode *child)
	{
		if(child->_parent == this)
		{
			WillRemoveChild(child);
			child->WillUpdate(ChangeSet::Parent);

			child->Retain();
			child->_parent = nullptr;

			_children->RemoveObject(child);

			child->DidUpdate(ChangeSet::Parent);
			child->__CompleteAttachmentWithScene(nullptr);

			DidRemoveChild(child);

			child->Release();
		}
	}

	void SceneNode::RemoveFromParent()
	{
		SceneNode *parent = GetParent();
		if(parent)
			parent->RemoveChild(this);
	}

	void SceneNode::AddAttachment(SceneNodeAttachment *attachment)
	{
		WillUpdate(ChangeSet::Attachments);

		if(!_attachments)
			_attachments = new Array();

		_attachments->AddObject(attachment);
		attachment->_node = this;

		DidUpdate(ChangeSet::Attachments);
	}

	void SceneNode::RemoveAttachment(SceneNodeAttachment *attachment)
	{
		WillUpdate(ChangeSet::Attachments);

		attachment->_node = nullptr;
		attachment->Retain();
		_attachments->RemoveObject(attachment);
		attachment->__DidUpdate(ChangeSet::Attachments); //Usually gets called by the SceneNode::DidUpdate() below, but not if removed from the attachments
		attachment->Release();

		DidUpdate(ChangeSet::Attachments);
	}

	const Array *SceneNode::GetAttachments() const
	{
		return _attachments;
	}

	const Array *SceneNode::GetChildren() const
	{
		return _children;
	}

	SceneNode *SceneNode::GetParent() const
	{
		return _parent;
	}

	void SceneNode::Traverse(const std::function<void(SceneNode *)> &callback)
	{
		callback(this);

		Object **children = const_cast<Object **>(_children->GetData());

		for (size_t i = 0; i < _children->GetCount(); i++) {
			static_cast<SceneNode *>(children[i])->Traverse(callback);
		}
	}

	// -------------------
	// MARK: -
	// MARK: Rendering
	// ------------------

	bool SceneNode::CanRenderUtil(Renderer *renderer, Camera *camera) const
	{
		if((_renderGroup & camera->GetRenderGroup()) == 0)
			return false;

		uint32 flags = _flags.load(std::memory_order_acquire);
		if(flags & Flags::Hidden)
			return false;

		if(flags & Flags::NoCulling)
			return true;

		return camera->InFrustum(GetBoundingSphere());
	}

	bool SceneNode::CanRender(Renderer *renderer, Camera *camera) const
	{
		return false;
	}

	void SceneNode::Render(Renderer *renderer, Camera *camera) const
	{}

	// -------------------
	// MARK: -
	// MARK: Updates
	// ------------------

	void SceneNode::Update(float delta)
	{
		if(_attachments)
		{
			_attachments->Enumerate<SceneNodeAttachment>([delta](SceneNodeAttachment *attachment, size_t index, bool &stop) {
				attachment->Update(delta);
			});
		}
	}

	void SceneNode::WillUpdate(ChangeSet changeSet)
	{
		if(_parent)
			_parent->ChildWillUpdate(this, changeSet);

		if(_attachments)
		{
			_attachments->Enumerate<SceneNodeAttachment>([changeSet](SceneNodeAttachment *attachment, size_t index, bool &stop) {
				attachment->__WillUpdate(changeSet);
			});
		}
	}

	void SceneNode::DidUpdate(ChangeSet changeSet)
	{
		if(changeSet & ChangeSet::Position)
		{
			_updated += 1;

			//Updated flag Needs to be passed on to all children and their children
			_children->Enumerate<SceneNode>([](SceneNode *child, size_t index, bool &stop) {
				child->DidUpdate(ChangeSet::Position);
			});
		}

		if(changeSet & ChangeSet::Parent)
		{
			_updated += 1;
			if(_parent == nullptr)
			{
				SetWorldPosition(Vector3(_position));
				SetWorldRotation(_rotation);
				SetWorldScale(_scale);
			}

			//Updated flag Needs to be passed on to all children and their children
			_children->Enumerate<SceneNode>([](SceneNode *child, size_t index, bool &stop) {
				child->DidUpdate(ChangeSet::Parent);
			});
		}

		if(_parent)
			_parent->ChildDidUpdate(this, changeSet);

		if(_attachments)
		{
			_attachments->Enumerate<SceneNodeAttachment>([changeSet](SceneNodeAttachment *attachment, size_t index, bool &stop) {
				attachment->__DidUpdate(changeSet);
			});
		}
	}

	void SceneNode::UpdateInternalData() const
	{
		uint64 transformVersion = GetTransformVersion();
		if(_lastUpdatedVersion != transformVersion)
		{
			Vector3 position = GetPositionForTransform();
			if(_parent)
			{
				_parent->UpdateInternalData();

				_worldPosition = _parent->GetWorldPosition() + _parent->GetWorldScale() * _parent->GetWorldRotation().GetRotatedVector(position);
				_worldRotation = _parent->GetWorldRotation() * _rotation;
				_worldScale = _parent->GetWorldScale() * _scale;
				_worldEuler = _parent->GetWorldEulerAngle() + _euler;
			}
			else
			{
				_worldPosition = position;
				_worldRotation = _rotation;
				_worldScale = _scale;
				_worldEuler = _euler;
			}

			_lastUpdatedVersion = transformVersion;
		}
	}

	void SceneNode::UpdateInternalTransformData() const
	{
		uint64 transformVersion = GetTransformVersion();
		if(_lastTransformUpdatedVersion != transformVersion)
		{
			Vector3 position = GetPositionForTransform();
			_localTransform = Matrix::WithTranslation(position);
			_localTransform.Rotate(_rotation);
			_localTransform.Scale(_scale);

			if(_parent)
			{
				_parent->UpdateInternalTransformData();
				_worldTransform = _parent->GetWorldTransform() * _localTransform;
			}
			else
			{
				_worldTransform = _localTransform;
			}

			_lastTransformUpdatedVersion = transformVersion;
		}
	}

	void SceneNode::UpdateInternalInverseTransformData() const
	{
		uint64 transformVersion = GetTransformVersion();
		if(_lastInverseTransformUpdatedVersion != transformVersion)
		{
			Vector3 position = GetPositionForTransform();
			_inverseLocalTransform = Matrix::WithScaling(_scale != 0.0f ? (Vector3(1.0f, 1.0f, 1.0f) / _scale) : Vector3(0.0f, 0.0f, 0.0f));
			_inverseLocalTransform.Rotate(_rotation->GetConjugated());
			_inverseLocalTransform.Translate(position * -1.0f);

			if(_parent)
			{
				_parent->UpdateInternalInverseTransformData();
				_inverseWorldTransform = _inverseLocalTransform * _parent->GetInverseWorldTransform();
			}
			else
			{
				_inverseWorldTransform = _inverseLocalTransform;
			}

			_lastInverseTransformUpdatedVersion = transformVersion;
		}
	}

	void SceneNode::UpdateInternalBoundsData() const
	{
		uint64 transformVersion = GetTransformVersion();
		if(_lastBoundsUpdatedVersion != transformVersion)
		{
			UpdateInternalData();

			_transformedBoundingBox = _boundingBox;
			_transformedBoundingBox.position = _worldPosition;
			_transformedBoundingBox *= _worldScale;
			_transformedBoundingBox.Rotate(_worldRotation);

			_transformedBoundingSphere = _boundingSphere;
			_transformedBoundingSphere.position = _worldPosition;
			_transformedBoundingSphere *= _worldScale;
			_transformedBoundingSphere.SetRotation(_worldRotation);

			_lastBoundsUpdatedVersion = transformVersion;
		}
	}
} // namespace RN
