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

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/MotorSettings.h>
#include <Jolt/Physics/Constraints/SpringSettings.h>

namespace RN
{
	RNDefineMeta(JoltRigidBodyController, JoltCollisionObject)

	constexpr uint32 InvalidSupportBodyID = 0xffffffff;
	constexpr float ExternalSupportAnchorLinearFrequency = 30.0f;
	constexpr float ExternalSupportAnchorLinearDamping = 6.0f;

	JoltRigidBodyController::JoltRigidBodyController(float radius, float height, float groundTolerance, float mass, float stepOffset) :
		_radius(radius), _height(height), _groundTolerance(groundTolerance), _stepOffset(stepOffset), _objectBelow(nullptr), _groundVelocity(), _groundAngularVelocity(), _groundNormal(), _isFalling(false), _externalSupportAnchorValid(false), _externalSupportCollisionFilteringEnabled(false), _externalSupportBodyID(InvalidSupportBodyID), _externalSupportLocalPosition(), _externalSupportAnchorMaxForce(0.0f), _externalSupportConstraint(nullptr)
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
		ClearExternalSupportAnchor();
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

		Vector3 adjustedVelocity = GetGroundAdjustedVelocity(velocity);
		ApplyStepOffset(adjustedVelocity, delta);
		_controller->SetLinearVelocity(JPH::Vec3Arg(adjustedVelocity.x, adjustedVelocity.y, adjustedVelocity.z));
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

	void JoltRigidBodyController::SetExternalSupportAnchor(uint32 bodyID, const Vector3 &localPosition, float maxForce)
	{
		if(bodyID == InvalidSupportBodyID || !localPosition.IsValid() || maxForce <= k::EpsilonFloat)
		{
			ClearExternalSupportAnchor();
			return;
		}

		if(_externalSupportAnchorValid && _externalSupportConstraint && _externalSupportBodyID == bodyID && _externalSupportLocalPosition == localPosition && _externalSupportAnchorMaxForce == maxForce)
		{
			return;
		}

		ClearExternalSupportAnchor();

		Vector3 supportPosition;
		Quaternion supportRotation;
		if(!GetSupportBodyTransform(bodyID, supportPosition, supportRotation))
		{
			return;
		}

		Vector3 supportAnchorPosition = supportPosition + supportRotation.GetRotatedVector(localPosition);
		Vector3 controllerAnchorPosition = GetWorldPosition();
		if(!supportAnchorPosition.IsValid() || !controllerAnchorPosition.IsValid())
		{
			return;
		}

		JPH::PhysicsSystem *physics = JoltWorld::GetSharedInstance()->GetJoltInstance();
		const JPH::BodyLockInterface &lockInterface = physics->GetBodyLockInterface();

		JPH::Body *supportBody = nullptr;
		JPH::Body *controllerBody = nullptr;
		{
			JPH::BodyLockRead lock(lockInterface, JPH::BodyID(bodyID));
			if(lock.Succeeded()) supportBody = const_cast<JPH::Body *>(&lock.GetBody());
		}
		{
			JPH::BodyLockRead lock(lockInterface, _controller->GetBodyID());
			if(lock.Succeeded()) controllerBody = const_cast<JPH::Body *>(&lock.GetBody());
		}

		if(!supportBody || !controllerBody)
		{
			return;
		}

		JPH::SixDOFConstraintSettings settings;
		settings.mSpace = JPH::EConstraintSpace::WorldSpace;
		settings.mPosition1 = JPH::RVec3(supportAnchorPosition.x, supportAnchorPosition.y, supportAnchorPosition.z);
		settings.mAxisX1 = JPH::Vec3::sAxisX();
		settings.mAxisY1 = JPH::Vec3::sAxisY();
		settings.mPosition2 = JPH::RVec3(controllerAnchorPosition.x, controllerAnchorPosition.y, controllerAnchorPosition.z);
		settings.mAxisX2 = JPH::Vec3::sAxisX();
		settings.mAxisY2 = JPH::Vec3::sAxisY();
		settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::TranslationX);
		settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::TranslationY);
		settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::TranslationZ);
		settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::RotationX);
		settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::RotationY);
		settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::RotationZ);

		JPH::SixDOFConstraint *constraint = static_cast<JPH::SixDOFConstraint *>(settings.Create(*supportBody, *controllerBody));
		if(!constraint)
		{
			return;
		}

		for(int axis = 0; axis < 3; axis += 1)
		{
			JPH::MotorSettings &motorSettings = constraint->GetMotorSettings(static_cast<JPH::SixDOFConstraint::EAxis>(axis));
			motorSettings.mSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
			motorSettings.mSpringSettings.mFrequency = ExternalSupportAnchorLinearFrequency;
			motorSettings.mSpringSettings.mDamping = ExternalSupportAnchorLinearDamping;
			motorSettings.SetForceLimit(maxForce);
			constraint->SetMotorState(static_cast<JPH::SixDOFConstraintSettings::EAxis>(axis), JPH::EMotorState::Position);
		}
		constraint->SetTargetPositionCS(JPH::Vec3::sZero());
		constraint->SetNumVelocityStepsOverride(10);
		constraint->SetNumPositionStepsOverride(10);
		physics->AddConstraint(constraint);
		JoltWorld::GetSharedInstance()->SetConnectedBodyCollisionFilteringEnabled(JPH::BodyID(bodyID), _controller->GetBodyID(), true);

		_externalSupportAnchorValid = true;
		_externalSupportCollisionFilteringEnabled = true;
		_externalSupportBodyID = bodyID;
		_externalSupportLocalPosition = localPosition;
		_externalSupportAnchorMaxForce = maxForce;
		_externalSupportConstraint = constraint;

		physics->GetBodyInterface().ActivateBody(JPH::BodyID(bodyID));
		_controller->Activate();
	}

	void JoltRigidBodyController::ClearExternalSupportAnchor()
	{
		if(JoltWorld *world = JoltWorld::GetSharedInstance())
		{
			if(_externalSupportCollisionFilteringEnabled)
			{
				world->SetConnectedBodyCollisionFilteringEnabled(JPH::BodyID(_externalSupportBodyID), _controller->GetBodyID(), false);
			}

			if(_externalSupportConstraint)
			{
				_externalSupportConstraint->SetEnabled(false);
				world->GetJoltInstance()->RemoveConstraint(_externalSupportConstraint);
			}
		}

		_externalSupportAnchorValid = false;
		_externalSupportCollisionFilteringEnabled = false;
		_externalSupportBodyID = InvalidSupportBodyID;
		_externalSupportLocalPosition = Vector3();
		_externalSupportAnchorMaxForce = 0.0f;
		_externalSupportConstraint = nullptr;
	}

	Vector3 JoltRigidBodyController::GetGroundAdjustedVelocity(const Vector3 &velocity) const
	{
		if(_controller->GetGroundState() != JPH::Character::EGroundState::OnGround)
		{
			return velocity;
		}

		Vector3 relativeVelocity = velocity - _groundVelocity;
		Vector3 horizontalVelocity(relativeVelocity.x, 0.0f, relativeVelocity.z);
		float horizontalSpeed = horizontalVelocity.GetLength();

		if(horizontalSpeed <= k::EpsilonFloat)
		{
			return velocity;
		}

		Vector3 groundNormal = _groundNormal;
		if(groundNormal.GetSquaredLength() <= k::EpsilonFloat || groundNormal.y <= k::EpsilonFloat)
		{
			return velocity;
		}

		groundNormal.Normalize();
		Vector3 slopeVelocity = horizontalVelocity - groundNormal * horizontalVelocity.GetDotProduct(groundNormal);
		if(slopeVelocity.GetSquaredLength() <= k::EpsilonFloat)
		{
			return velocity;
		}

		slopeVelocity.Normalize(horizontalSpeed);
		if(relativeVelocity.y > 0.0f)
		{
			slopeVelocity.y += relativeVelocity.y;
		}

		return _groundVelocity + slopeVelocity;
	}

	void JoltRigidBodyController::ApplyStepOffset(const Vector3 &velocity, float delta)
	{
		if(_stepOffset <= k::EpsilonFloat || !_controller->IsSupported())
		{
			return;
		}

		Vector3 relativeVelocity = velocity - _groundVelocity;
		Vector3 horizontalVelocity(relativeVelocity.x, 0.0f, relativeVelocity.z);
		if(horizontalVelocity.GetSquaredLength() <= k::EpsilonFloat)
		{
			return;
		}

		RN::Vector3 movementDirection = horizontalVelocity;
		movementDirection.Normalize();

		JPH::RVec3 position = _controller->GetPosition();
		JPH::Quat rotation = _controller->GetRotation();
		Vector3 currentPosition(position.GetX(), position.GetY(), position.GetZ());
		Quaternion currentRotation(rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW());
		Vector3 horizontalMovement = horizontalVelocity * delta;
		float horizontalDistance = horizontalMovement.GetLength();
		if(horizontalDistance <= k::EpsilonFloat)
		{
			return;
		}

		Vector3 stepProbeMovement = relativeVelocity * delta;
		if(horizontalDistance < 0.02f)
		{
			stepProbeMovement *= 0.02f / horizontalDistance;
		}

		if(!HasBlockingCollisionAt(currentPosition + stepProbeMovement, currentRotation, movementDirection))
		{
			return;
		}

		constexpr int StepSearchCount = 8;
		constexpr int StepRefinementCount = 5;
		constexpr float StepClearance = 0.005f;

		float blockedHeight = 0.0f;
		float clearHeight = -1.0f;
		for(int i = 1; i <= StepSearchCount; i++)
		{
			float candidateHeight = _stepOffset * (static_cast<float>(i) / static_cast<float>(StepSearchCount));
			Vector3 candidateStep(0.0f, candidateHeight, 0.0f);
			if(!HasPenetrationAt(currentPosition + candidateStep, currentRotation, movementDirection) && !HasPenetrationAt(currentPosition + candidateStep + stepProbeMovement, currentRotation, movementDirection))
			{
				clearHeight = candidateHeight;
				break;
			}

			blockedHeight = candidateHeight;
		}

		if(clearHeight < 0.0f)
		{
			return;
		}

		for(int i = 0; i < StepRefinementCount; i++)
		{
			float candidateHeight = (blockedHeight + clearHeight) * 0.5f;
			Vector3 candidateStep(0.0f, candidateHeight, 0.0f);
			if(!HasPenetrationAt(currentPosition + candidateStep, currentRotation, movementDirection) && !HasPenetrationAt(currentPosition + candidateStep + stepProbeMovement, currentRotation, movementDirection))
			{
				clearHeight = candidateHeight;
			}
			else
			{
				blockedHeight = candidateHeight;
			}
		}

		float stepHeight = clearHeight + StepClearance;
		if(stepHeight > _stepOffset) stepHeight = _stepOffset;

		Vector3 steppedPosition = currentPosition + Vector3(0.0f, stepHeight, 0.0f);
		_controller->SetPosition(JPH::RVec3Arg(steppedPosition.x, steppedPosition.y, steppedPosition.z), JPH::EActivation::Activate);
	}

	bool JoltRigidBodyController::HasBlockingCollisionAt(const Vector3 &position, const Quaternion &rotation, const Vector3 &movementDirection) const
	{
		JPH::RVec3 joltPosition(position.x, position.y, position.z);
		JPH::Quat joltRotation(rotation.x, rotation.y, rotation.z, rotation.w);
		JPH::Vec3 joltMovementDirection(movementDirection.x, movementDirection.y, movementDirection.z);
		JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
		_controller->CheckCollision(joltPosition, joltRotation, joltMovementDirection, 0.0f, _shape->GetJoltShape(), joltPosition, hits);

		for(const JPH::CollideShapeResult &hit : hits.mHits)
		{
			if(hit.mPenetrationDepth <= k::EpsilonFloat)
			{
				continue;
			}

			JPH::Vec3 normal = -hit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero());
			if(normal.GetY() < 0.5f)
			{
				return true;
			}
		}

		return false;
	}

	bool JoltRigidBodyController::HasPenetrationAt(const Vector3 &position, const Quaternion &rotation, const Vector3 &movementDirection) const
	{
		JPH::RVec3 joltPosition(position.x, position.y, position.z);
		JPH::Quat joltRotation(rotation.x, rotation.y, rotation.z, rotation.w);
		JPH::Vec3 joltMovementDirection(movementDirection.x, movementDirection.y, movementDirection.z);
		JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
		_controller->CheckCollision(joltPosition, joltRotation, joltMovementDirection, 0.0f, _shape->GetJoltShape(), joltPosition, hits);

		for(const JPH::CollideShapeResult &hit : hits.mHits)
		{
			if(hit.mPenetrationDepth > k::EpsilonFloat)
			{
				return true;
			}
		}

		return false;
	}

	bool JoltRigidBodyController::GetSupportBodyTransform(uint32 bodyID, Vector3 &position, Quaternion &rotation) const
	{
		JPH::BodyID joltBodyID(bodyID);
		if(joltBodyID.IsInvalid())
		{
			return false;
		}

		JPH::PhysicsSystem *physics = JoltWorld::GetSharedInstance()->GetJoltInstance();
		JPH::BodyLockRead lock(physics->GetBodyLockInterface(), joltBodyID);
		if(!lock.Succeeded())
		{
			return false;
		}

		const JPH::Body &body = lock.GetBody();
		JPH::RVec3 joltPosition = body.GetPosition();
		JPH::Quat joltRotation = body.GetRotation();
		position = Vector3(joltPosition.GetX(), joltPosition.GetY(), joltPosition.GetZ());
		rotation = Quaternion(joltRotation.GetX(), joltRotation.GetY(), joltRotation.GetZ(), joltRotation.GetW());
		if(!position.IsValid() || !rotation.IsValid())
		{
			return false;
		}

		rotation.Normalize();
		return true;
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
			_groundAngularVelocity = Vector3();
			_groundNormal = Vector3();
			_isFalling = true;
			return;
		}

		JoltCollisionObject *collisionObject = reinterpret_cast<JoltCollisionObject *>(_controller->GetGroundUserData());
		_objectBelow = collisionObject ? collisionObject->GetParent() : nullptr;
		if(_objectBelow) _objectBelow->Retain()->Autorelease();

		JPH::Vec3 groundVelocity = _controller->GetGroundVelocity();
		_groundVelocity = Vector3(groundVelocity.GetX(), groundVelocity.GetY(), groundVelocity.GetZ());
		JPH::PhysicsSystem *physics = JoltWorld::GetSharedInstance()->GetJoltInstance();
		JPH::Vec3 groundAngularVelocity = physics->GetBodyInterface().GetAngularVelocity(_controller->GetGroundBodyID());
		_groundAngularVelocity = Vector3(groundAngularVelocity.GetX(), groundAngularVelocity.GetY(), groundAngularVelocity.GetZ());
		JPH::Vec3 groundNormal = _controller->GetGroundNormal();
		_groundNormal = Vector3(groundNormal.GetX(), groundNormal.GetY(), groundNormal.GetZ());
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
