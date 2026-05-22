//
//  RNJoltConstraint.cpp
//  Rayne-Jolt
//
//  Copyright 2023 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//
#include "RNJoltConstraint.h"
#include "RNJoltInternals.h"
#include "RNJoltWorld.h"

#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/Constraints/MotorSettings.h>
#include <Jolt/Physics/Constraints/SpringSettings.h>

namespace RN
{
	RNDefineMeta(JoltConstraint, Object)
	RNDefineMeta(JoltPointConstraint, JoltConstraint)
	RNDefineMeta(JoltFixedConstraint, JoltConstraint)
	RNDefineMeta(JoltDistanceConstraint, JoltConstraint)
	RNDefineMeta(JoltSixDOFConstraint, JoltConstraint)

	JoltConstraint::JoltConstraint() :
		_constraint(nullptr),
		_bodyPairCollisionBody1(JPH::BodyID::cInvalidBodyID),
		_bodyPairCollisionBody2(JPH::BodyID::cInvalidBodyID),
		_bodyPairCollisionDisabled(false),
		_collisionsEnabled(false)
	{
	}

	void JoltConstraint::SetSolverIterationCount(uint32 positionIterations, uint32 velocityIterations)
	{
		if(!_constraint) return;
		if(velocityIterations > 0) _constraint->SetNumVelocityStepsOverride(velocityIterations);
		if(positionIterations > 0) _constraint->SetNumPositionStepsOverride(positionIterations);
	}

	JoltConstraint::~JoltConstraint()
	{
		if(_bodyPairCollisionDisabled)
		{
			SetStoredBodyPairCollisionEnabled(true);
		}

		if(_constraint)
		{
			//Remove from physics system which will also release the constraint
			if(JoltWorld *world = JoltWorld::GetSharedInstance())
			{
				_constraint->SetEnabled(false);
				world->GetJoltInstance()->RemoveConstraint(_constraint);
			}
		}
	}

	void JoltConstraint::SetConstraint(JPH::Constraint *constraint)
	{
		if(_constraint == constraint) return;
		ResetStoredBodyPairCollisionState();

		if(_constraint)
		{
			JPH::PhysicsSystem *physics = JoltWorld::GetSharedInstance()->GetJoltInstance();
			physics->RemoveConstraint(_constraint);
			_constraint = nullptr;
		}
		_constraint = constraint;
		if(_constraint)
		{
			JPH::PhysicsSystem *physics = JoltWorld::GetSharedInstance()->GetJoltInstance();
			physics->AddConstraint(_constraint);
		}

		if(_constraint && _constraint->GetType() == JPH::EConstraintType::TwoBodyConstraint)
		{
			JPH::TwoBodyConstraint *twoBodyConstraint = static_cast<JPH::TwoBodyConstraint *>(_constraint);
			JPH::BodyID bodyID1 = twoBodyConstraint->GetBody1()->GetID();
			JPH::BodyID bodyID2 = twoBodyConstraint->GetBody2()->GetID();
			_bodyPairCollisionBody1 = bodyID1.GetIndexAndSequenceNumber();
			_bodyPairCollisionBody2 = bodyID2.GetIndexAndSequenceNumber();
			UpdateBodyPairCollisionState();
		}
	}

	void JoltConstraint::SetEnabled(bool enabled)
	{
		if(_constraint) _constraint->SetEnabled(enabled);
	}

	void JoltConstraint::SetCollisionsEnabled(bool enabled)
	{
		if(_collisionsEnabled == enabled) return;

		_collisionsEnabled = enabled;
		UpdateBodyPairCollisionState();
	}

	void JoltConstraint::ResetStoredBodyPairCollisionState()
	{
		if(_bodyPairCollisionDisabled)
		{
			SetStoredBodyPairCollisionEnabled(true);
			_bodyPairCollisionDisabled = false;
		}

		_bodyPairCollisionBody1 = JPH::BodyID::cInvalidBodyID;
		_bodyPairCollisionBody2 = JPH::BodyID::cInvalidBodyID;
	}

	void JoltConstraint::UpdateBodyPairCollisionState()
	{
		if(!HasStoredBodyPair()) return;

		bool shouldDisable = !_collisionsEnabled;
		if(_bodyPairCollisionDisabled == shouldDisable) return;

		SetStoredBodyPairCollisionEnabled(!shouldDisable);
		_bodyPairCollisionDisabled = shouldDisable;
	}

	void JoltConstraint::SetStoredBodyPairCollisionEnabled(bool enabled)
	{
		if(!HasStoredBodyPair()) return;

		if(JoltWorld *world = JoltWorld::GetSharedInstance())
		{
			world->SetBodyPairCollisionEnabled(JPH::BodyID(_bodyPairCollisionBody1), JPH::BodyID(_bodyPairCollisionBody2), enabled);
		}
	}

	bool JoltConstraint::HasStoredBodyPair() const
	{
		return _bodyPairCollisionBody1 != JPH::BodyID::cInvalidBodyID &&
			_bodyPairCollisionBody2 != JPH::BodyID::cInvalidBodyID &&
			_bodyPairCollisionBody1 != _bodyPairCollisionBody2;
	}

	static JPH::Quat ToJoltQuat(const Quaternion &q)
	{
		return JPH::Quat(q.x, q.y, q.z, q.w);
	}

	static JPH::Vec3 ToJoltVec3(const Vector3 &v)
	{
		return JPH::Vec3(v.x, v.y, v.z);
	}

	JoltPointConstraint::JoltPointConstraint(JoltDynamicBody *body1, const Vector3 &worldPoint1, JoltDynamicBody *body2, const Vector3 &worldPoint2)
	{
		JPH::PhysicsSystem *physics = JoltWorld::GetSharedInstance()->GetJoltInstance();
		const JPH::BodyLockInterface &lockInterface = physics->GetBodyLockInterface();

		JPH::Body *b1 = nullptr;
		JPH::Body *b2 = nullptr;
		{
			JPH::BodyLockRead lock1(lockInterface, *body1->GetJoltActor());
			if(lock1.Succeeded()) b1 = const_cast<JPH::Body*>(&lock1.GetBody());
		}
		{
			JPH::BodyLockRead lock2(lockInterface, *body2->GetJoltActor());
			if(lock2.Succeeded()) b2 = const_cast<JPH::Body*>(&lock2.GetBody());
		}

		RN_ASSERT(b1 && b2, "Invalid bodies for constraint creation");

		JPH::PointConstraintSettings settings;
		settings.mSpace = JPH::EConstraintSpace::WorldSpace;
		settings.mPoint1 = JPH::Vec3(worldPoint1.x, worldPoint1.y, worldPoint1.z);
		settings.mPoint2 = JPH::Vec3(worldPoint2.x, worldPoint2.y, worldPoint2.z);
		SetConstraint(settings.Create(*b1, *b2));
	}

	JoltPointConstraint *JoltPointConstraint::WithBodiesAndWorldPoints(JoltDynamicBody *body1, const Vector3 &worldPoint1, JoltDynamicBody *body2, const Vector3 &worldPoint2)
	{
		JoltPointConstraint *constraint = new JoltPointConstraint(body1, worldPoint1, body2, worldPoint2);
		return constraint->Autorelease();
	}

	JoltFixedConstraint::JoltFixedConstraint(JoltDynamicBody *body1, const Vector3 &worldPosition1, const Quaternion &worldRotation1, JoltDynamicBody *body2, const Vector3 &worldPosition2, const Quaternion &worldRotation2)
	{
		JPH::PhysicsSystem *physics = JoltWorld::GetSharedInstance()->GetJoltInstance();
		const JPH::BodyLockInterface &lockInterface = physics->GetBodyLockInterface();

		JPH::Body *b1 = nullptr;
		JPH::Body *b2 = nullptr;
		{
			JPH::BodyLockRead lock1(lockInterface, *body1->GetJoltActor());
			if(lock1.Succeeded()) b1 = const_cast<JPH::Body*>(&lock1.GetBody());
		}
		{
			JPH::BodyLockRead lock2(lockInterface, *body2->GetJoltActor());
			if(lock2.Succeeded()) b2 = const_cast<JPH::Body*>(&lock2.GetBody());
		}

		RN_ASSERT(b1 && b2, "Invalid bodies for constraint creation");

		JPH::FixedConstraintSettings settings;
		settings.mSpace = JPH::EConstraintSpace::WorldSpace;
		settings.mAutoDetectPoint = false;
		settings.mPoint1 = JPH::Vec3(worldPosition1.x, worldPosition1.y, worldPosition1.z);
		settings.mPoint2 = JPH::Vec3(worldPosition2.x, worldPosition2.y, worldPosition2.z);
		settings.mAxisX1 = ToJoltVec3(worldRotation1.GetRotatedVector(Vector3(1.0f, 0.0f, 0.0f)));
		settings.mAxisY1 = ToJoltVec3(worldRotation1.GetRotatedVector(Vector3(0.0f, 1.0f, 0.0f)));
		settings.mAxisX2 = ToJoltVec3(worldRotation2.GetRotatedVector(Vector3(1.0f, 0.0f, 0.0f)));
		settings.mAxisY2 = ToJoltVec3(worldRotation2.GetRotatedVector(Vector3(0.0f, 1.0f, 0.0f)));
		SetConstraint(settings.Create(*b1, *b2));
	}

	JoltFixedConstraint *JoltFixedConstraint::WithBodiesAndWorldFrames(JoltDynamicBody *body1, const Vector3 &worldPosition1, const Quaternion &worldRotation1, JoltDynamicBody *body2, const Vector3 &worldPosition2, const Quaternion &worldRotation2)
	{
		JoltFixedConstraint *constraint = new JoltFixedConstraint(body1, worldPosition1, worldRotation1, body2, worldPosition2, worldRotation2);
		return constraint->Autorelease();
	}

	JoltDistanceConstraint::JoltDistanceConstraint(JoltDynamicBody *body1, const Vector3 &worldPoint1, JoltDynamicBody *body2, const Vector3 &worldPoint2, float minDistance, float maxDistance)
	{
		JPH::PhysicsSystem *physics = JoltWorld::GetSharedInstance()->GetJoltInstance();
		const JPH::BodyLockInterface &lockInterface = physics->GetBodyLockInterface();

		JPH::Body *b1 = nullptr;
		JPH::Body *b2 = nullptr;
		{
			JPH::BodyLockRead lock1(lockInterface, *body1->GetJoltActor());
			if(lock1.Succeeded()) b1 = const_cast<JPH::Body*>(&lock1.GetBody());
		}
		{
			JPH::BodyLockRead lock2(lockInterface, *body2->GetJoltActor());
			if(lock2.Succeeded()) b2 = const_cast<JPH::Body*>(&lock2.GetBody());
		}

		RN_ASSERT(b1 && b2, "Invalid bodies for constraint creation");

		JPH::DistanceConstraintSettings settings;
		settings.mSpace = JPH::EConstraintSpace::WorldSpace;
		settings.mPoint1 = JPH::Vec3(worldPoint1.x, worldPoint1.y, worldPoint1.z);
		settings.mPoint2 = JPH::Vec3(worldPoint2.x, worldPoint2.y, worldPoint2.z);
		settings.mMinDistance = minDistance;
		settings.mMaxDistance = maxDistance;
		SetConstraint(settings.Create(*b1, *b2));
	}

	JoltDistanceConstraint *JoltDistanceConstraint::WithBodiesAndWorldPoints(JoltDynamicBody *body1, const Vector3 &worldPoint1, JoltDynamicBody *body2, const Vector3 &worldPoint2, float minDistance, float maxDistance)
	{
		JoltDistanceConstraint *constraint = new JoltDistanceConstraint(body1, worldPoint1, body2, worldPoint2, minDistance, maxDistance);
		return constraint->Autorelease();
	}

	JoltSixDOFConstraint::JoltSixDOFConstraint(JoltDynamicBody *body1, const Vector3 &worldPosition1, const Quaternion &worldRotation1, JoltDynamicBody *body2, const Vector3 &worldPosition2, const Quaternion &worldRotation2)
	{
		JPH::PhysicsSystem *physics = JoltWorld::GetSharedInstance()->GetJoltInstance();
		const JPH::BodyLockInterface &lockInterface = physics->GetBodyLockInterface();

		JPH::Body *b1 = nullptr;
		JPH::Body *b2 = nullptr;
		{
			JPH::BodyLockRead lock1(lockInterface, *body1->GetJoltActor());
			if(lock1.Succeeded()) b1 = const_cast<JPH::Body*>(&lock1.GetBody());
		}
		{
			JPH::BodyLockRead lock2(lockInterface, *body2->GetJoltActor());
			if(lock2.Succeeded()) b2 = const_cast<JPH::Body*>(&lock2.GetBody());
		}
		RN_ASSERT(b1 && b2, "Invalid bodies for constraint creation");

		JPH::SixDOFConstraintSettings settings;
		settings.mSpace = JPH::EConstraintSpace::WorldSpace;
		settings.mPosition1 = JPH::RVec3(worldPosition1.x, worldPosition1.y, worldPosition1.z);
		settings.mAxisX1 = ToJoltVec3(worldRotation1.GetRotatedVector(Vector3(1.0f, 0.0f, 0.0f)));
		settings.mAxisY1 = ToJoltVec3(worldRotation1.GetRotatedVector(Vector3(0.0f, 1.0f, 0.0f)));
		settings.mPosition2 = JPH::RVec3(worldPosition2.x, worldPosition2.y, worldPosition2.z);
		settings.mAxisX2 = ToJoltVec3(worldRotation2.GetRotatedVector(Vector3(1.0f, 0.0f, 0.0f)));
		settings.mAxisY2 = ToJoltVec3(worldRotation2.GetRotatedVector(Vector3(0.0f, 1.0f, 0.0f)));

		// Allow all DOFs; motors will drive to targets each tick
		settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::TranslationX);
		settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::TranslationY);
		settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::TranslationZ);
		settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::RotationX);
		settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::RotationY);
		settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::RotationZ);

		SetConstraint(settings.Create(*b1, *b2));

		// Default motor params
		SetLinearMotorParams(30.0f, 6.0f, 5000.0f);
		SetAngularMotorParams(40.0f, 8.0f, 2000.0f);

		// Enable motors (position)
		SetMotorState(Axis::TranslationX, 2);
		SetMotorState(Axis::TranslationY, 2);
		SetMotorState(Axis::TranslationZ, 2);
		SetMotorState(Axis::RotationX, 2);
		SetMotorState(Axis::RotationY, 2);
		SetMotorState(Axis::RotationZ, 2);
	}

	JoltSixDOFConstraint *JoltSixDOFConstraint::WithBodiesAndWorldFrames(JoltDynamicBody *body1, const Vector3 &worldPosition1, const Quaternion &worldRotation1, JoltDynamicBody *body2, const Vector3 &worldPosition2, const Quaternion &worldRotation2)
	{
		JoltSixDOFConstraint *c = new JoltSixDOFConstraint(body1, worldPosition1, worldRotation1, body2, worldPosition2, worldRotation2);
		return c->Autorelease();
	}

	void JoltSixDOFConstraint::SetMotorState(Axis axis, int state)
	{
		if(!_constraint) return;
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		JPH::EMotorState s = JPH::EMotorState::Off;
		if(state == 1) s = JPH::EMotorState::Velocity; else if(state == 2) s = JPH::EMotorState::Position;
		six->SetMotorState(static_cast<JPH::SixDOFConstraintSettings::EAxis>(static_cast<int>(axis)), s);
	}

	void JoltSixDOFConstraint::SetTargetPositionCS(const Vector3 &p_cs)
	{
		if(!_constraint) return;
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		six->SetTargetPositionCS(ToJoltVec3(p_cs));
	}
	void JoltSixDOFConstraint::SetTargetVelocityCS(const Vector3 &v_cs)
	{
		if(!_constraint) return;
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		six->SetTargetVelocityCS(ToJoltVec3(v_cs));
	}
	void JoltSixDOFConstraint::SetTargetAngularVelocityCS(const Vector3 &w_cs)
	{
		if(!_constraint) return;
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		six->SetTargetAngularVelocityCS(ToJoltVec3(w_cs));
	}
	void JoltSixDOFConstraint::SetTargetOrientationCS(const Quaternion &q_cs)
	{
		if(!_constraint) return;
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		six->SetTargetOrientationCS(ToJoltQuat(q_cs));
	}
	void JoltSixDOFConstraint::SetTargetOrientationBS(const Quaternion &q_bs)
	{
		if(!_constraint) return;
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		six->SetTargetOrientationBS(ToJoltQuat(q_bs));
	}

	void JoltSixDOFConstraint::SetLinearMotorParams(float frequency, float damping, float maxForce)
	{
		if(!_constraint) return;
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		for(int a = 0; a < 3; ++a)
		{
			JPH::MotorSettings &m = six->GetMotorSettings(static_cast<JPH::SixDOFConstraint::EAxis>(a));
			m.mSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
			m.mSpringSettings.mFrequency = frequency;
			m.mSpringSettings.mDamping = damping;
			m.SetForceLimit(maxForce);
		}
	}
	void JoltSixDOFConstraint::SetAngularMotorParams(float frequency, float damping, float maxTorque)
	{
		if(!_constraint) return;
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		for(int a = 3; a < 6; ++a)
		{
			JPH::MotorSettings &m = six->GetMotorSettings(static_cast<JPH::SixDOFConstraint::EAxis>(a));
			m.mSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
			m.mSpringSettings.mFrequency = frequency;
			m.mSpringSettings.mDamping = damping;
			m.SetTorqueLimit(maxTorque);
		}
	}

	void JoltSixDOFConstraint::SetTranslationLimits(const Vector3 &limitMin, const Vector3 &limitMax)
	{
		if(!_constraint) return;
		RebuildWithLimits(&limitMin, &limitMax, nullptr, nullptr);
	}

	void JoltSixDOFConstraint::SetRotationLimits(const Vector3 &limitMin, const Vector3 &limitMax)
	{
		if(!_constraint) return;
		RebuildWithLimits(nullptr, nullptr, &limitMin, &limitMax);
	}

	void JoltSixDOFConstraint::RebuildWithLimits(const Vector3 *translationLimitMin, const Vector3 *translationLimitMax, const Vector3 *rotationLimitMin, const Vector3 *rotationLimitMax)
	{
		if(!_constraint) return;

		auto configureAxis = [](JPH::SixDOFConstraintSettings *settings, JPH::SixDOFConstraintSettings::EAxis axis, float limitMin, float limitMax) {
			if(limitMin <= -1000000.0f && limitMax >= 1000000.0f)
			{
				settings->MakeFreeAxis(axis);
				return;
			}

			if(limitMin >= limitMax || limitMin > 0.0f || limitMax < 0.0f)
			{
				settings->MakeFixedAxis(axis);
				return;
			}

			settings->SetLimitedAxis(axis, limitMin, limitMax);
		};

		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		JPH::Body *body1 = six->GetBody1();
		JPH::Body *body2 = six->GetBody2();

		JPH::EMotorState motorState[6];
		for(int i = 0; i < 6; i++)
		{
			motorState[i] = six->GetMotorState(static_cast<JPH::SixDOFConstraint::EAxis>(i));
		}

		JPH::Vec3 targetVelocity = six->GetTargetVelocityCS();
		JPH::Vec3 targetAngularVelocity = six->GetTargetAngularVelocityCS();
		JPH::Vec3 targetPosition = six->GetTargetPositionCS();
		JPH::Quat targetOrientation = six->GetTargetOrientationCS();
		JPH::SpringSettings limitsSpringSettings[3] = {
			six->GetLimitsSpringSettings(JPH::SixDOFConstraint::EAxis::TranslationX),
			six->GetLimitsSpringSettings(JPH::SixDOFConstraint::EAxis::TranslationY),
			six->GetLimitsSpringSettings(JPH::SixDOFConstraint::EAxis::TranslationZ)
		};

		JPH::Ref<JPH::ConstraintSettings> constraintSettings = six->GetConstraintSettings();
		JPH::SixDOFConstraintSettings *settings = static_cast<JPH::SixDOFConstraintSettings *>(constraintSettings.GetPtr());
		for(int i = 0; i < 3; i++)
		{
			settings->mLimitsSpringSettings[i] = limitsSpringSettings[i];
		}

		if(translationLimitMin && translationLimitMax)
		{
			configureAxis(settings, JPH::SixDOFConstraintSettings::TranslationX, translationLimitMin->x, translationLimitMax->x);
			configureAxis(settings, JPH::SixDOFConstraintSettings::TranslationY, translationLimitMin->y, translationLimitMax->y);
			configureAxis(settings, JPH::SixDOFConstraintSettings::TranslationZ, translationLimitMin->z, translationLimitMax->z);
		}

		if(rotationLimitMin && rotationLimitMax)
		{
			configureAxis(settings, JPH::SixDOFConstraintSettings::RotationX, rotationLimitMin->x, rotationLimitMax->x);
			configureAxis(settings, JPH::SixDOFConstraintSettings::RotationY, rotationLimitMin->y, rotationLimitMax->y);
			configureAxis(settings, JPH::SixDOFConstraintSettings::RotationZ, rotationLimitMin->z, rotationLimitMax->z);
		}

		SetConstraint(settings->Create(*body1, *body2));

		six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		for(int i = 0; i < 6; i++)
		{
			six->SetMotorState(static_cast<JPH::SixDOFConstraint::EAxis>(i), motorState[i]);
		}

		six->SetTargetVelocityCS(targetVelocity);
		six->SetTargetAngularVelocityCS(targetAngularVelocity);
		six->SetTargetPositionCS(targetPosition);
		six->SetTargetOrientationCS(targetOrientation);
	}

	void JoltSixDOFConstraint::SetTranslationSpringParams(float frequency, float damping)
	{
		if(!_constraint) return;
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		JPH::SpringSettings s;
		s.mMode = JPH::ESpringMode::FrequencyAndDamping;
		s.mFrequency = frequency;
		s.mDamping = damping;
		for(int a = 0; a < 3; ++a)
		{
			six->SetLimitsSpringSettings(static_cast<JPH::SixDOFConstraint::EAxis>(a), s);
		}
	}

	void JoltSixDOFConstraint::SetTranslationSpringParamsAxis(Axis axis, float frequency, float damping)
	{
		if(!_constraint) return;
		int a = static_cast<int>(axis);
		if(a < 0 || a > 2) return; // only translation axes support spring limits
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		JPH::SpringSettings s;
		s.mMode = JPH::ESpringMode::FrequencyAndDamping;
		s.mFrequency = frequency;
		s.mDamping = damping;
		six->SetLimitsSpringSettings(static_cast<JPH::SixDOFConstraint::EAxis>(a), s);
	}

	void JoltSixDOFConstraint::SetMaxFriction(Axis axis, float maxFriction)
	{
		if(!_constraint) return;
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		six->SetMaxFriction(static_cast<JPH::SixDOFConstraint::EAxis>(static_cast<int>(axis)), maxFriction);
	}
}
