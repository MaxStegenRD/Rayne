//
//  RNJoltRigidBodyController.cpp
//  Rayne-Jolt
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNJoltRigidBodyController.h"
#include "RNJoltInternals.h"
#include "RNJoltWorld.h"

#include <Jolt/Physics/Character/Character.h>

namespace RN
{
	RNDefineMeta(JoltRigidBodyController, JoltCollisionObject)

	JoltRigidBodyController::JoltRigidBodyController(float radius, float height, float groundTolerance, float mass) :
		_radius(radius), _height(height), _groundTolerance(groundTolerance), _objectBelow(nullptr), _groundVelocity(), _isFalling(false)
	{
		JoltWorld *world = JoltWorld::GetSharedInstance();
		JPH::PhysicsSystem *physics = world->GetJoltInstance();

		_shape = JoltCapsuleShape::WithRadius(radius, height)->Retain();

		JPH::CharacterSettings settings;
		settings.mMaxSlopeAngle = JPH::DegreesToRadians(70.0f);
		settings.mMass = mass;
		settings.mFriction = 0.2f;
		settings.mGravityFactor = 0.0f;
		settings.mLayer = world->GetObjectLayer(_collisionFilterGroup, _collisionFilterMask, 1);
		settings.mShape = _shape->GetJoltShape();
		settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius);

		_controller = new JPH::Character(&settings, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), reinterpret_cast<uint64>(this), physics);
		_controller->AddToPhysicsSystem(JPH::EActivation::DontActivate);
	}

	JoltRigidBodyController::~JoltRigidBodyController()
	{
		_controller->RemoveFromPhysicsSystem();
		delete _controller;
		SafeRelease(_shape);
	}

	void JoltRigidBodyController::Move(const Vector3 &velocity, float delta)
	{
		if(delta <= k::EpsilonFloat)
		{
			UpdateGroundState();
			return;
		}

		_controller->SetLinearVelocity(JPH::Vec3Arg(velocity.x, velocity.y, velocity.z));
		_controller->Activate();
	}

	bool JoltRigidBodyController::Resize(float height, bool checkIfBlocked)
	{
		float heightDifference = height - _height;
		if(((heightDifference < 0.0f) ? -heightDifference : heightDifference) < k::EpsilonFloat) return true;

		JoltShape *newShape = JoltCapsuleShape::WithRadius(_radius, height);
		bool didSetShape = _controller->SetShape(newShape->GetJoltShape(), checkIfBlocked ? 0.01f : 1000000.0f);
		if(!didSetShape) return false;

		SafeRelease(_shape);
		_shape = newShape->Retain();
		_height = height;
		return true;
	}

	void JoltRigidBodyController::SetCollisionFilter(uint32 group, uint32 mask)
	{
		JoltCollisionObject::SetCollisionFilter(group, mask);

		JoltWorld *world = JoltWorld::GetSharedInstance();
		_controller->SetLayer(world->GetObjectLayer(_collisionFilterGroup, _collisionFilterMask, 1));
	}

	Vector3 JoltRigidBodyController::GetFeetOffset() const
	{
		return Vector3(0.0f, -(_height * 0.5f + _radius), 0.0f);
	}

	void JoltRigidBodyController::UpdateControllerTransform()
	{
		Vector3 positionOffset = GetWorldRotation().GetRotatedVector(_positionOffset);
		Vector3 position = GetWorldPosition() - positionOffset;
		Quaternion rotation = GetWorldRotation() * _rotationOffset;

		_controller->SetPositionAndRotation(JPH::RVec3Arg(position.x, position.y, position.z), JPH::QuatArg(rotation.x, rotation.y, rotation.z, rotation.w), JPH::EActivation::DontActivate);
	}

	void JoltRigidBodyController::UpdateGroundState()
	{
		if(!_controller->IsSupported())
		{
			_objectBelow = nullptr;
			_groundVelocity = Vector3();
			_isFalling = true;
			return;
		}

		JoltCollisionObject *collisionObject = reinterpret_cast<JoltCollisionObject *>(_controller->GetGroundUserData());
		_objectBelow = collisionObject ? collisionObject->GetParent() : nullptr;
		if(_objectBelow) _objectBelow->Retain()->Autorelease();

		JPH::Vec3 groundVelocity = _controller->GetGroundVelocity();
		_groundVelocity = Vector3(groundVelocity.GetX(), groundVelocity.GetY(), groundVelocity.GetZ());
		_isFalling = false;
	}

	void JoltRigidBodyController::DidUpdate(SceneNode::ChangeSet changeSet)
	{
		JoltCollisionObject::DidUpdate(changeSet);

		if(changeSet & SceneNode::ChangeSet::Position)
		{
			UpdateControllerTransform();
		}

		if(changeSet & SceneNode::ChangeSet::Attachments)
		{
			if(!_owner && GetParent())
			{
				UpdateControllerTransform();
			}

			_owner = GetParent();
		}
	}

	void JoltRigidBodyController::UpdatePosition()
	{
		if(!_owner)
		{
			return;
		}

		_controller->PostSimulation(_groundTolerance);
		UpdateGroundState();

		JPH::RVec3 position;
		JPH::Quat rotation;
		_controller->GetPositionAndRotation(position, rotation);

		Quaternion rotationResult = Quaternion(rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW()) * _rotationOffset.GetConjugated();
		Vector3 positionOffset = rotationResult.GetRotatedVector(_positionOffset);
		SetWorldPosition(Vector3(position.GetX(), position.GetY(), position.GetZ()) + positionOffset);
		SetWorldRotation(rotationResult);
	}
} // namespace RN
