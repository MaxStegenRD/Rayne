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
#include <Jolt/Physics/Constraints/MotorSettings.h>
#include <Jolt/Physics/Constraints/SpringSettings.h>

namespace RN
{
	RNDefineMeta(JoltConstraint, Object)
	RNDefineMeta(JoltPointConstraint, JoltConstraint)
	RNDefineMeta(JoltFixedConstraint, JoltConstraint)
	RNDefineMeta(JoltDistanceConstraint, JoltConstraint)
	RNDefineMeta(JoltSixDOFConstraint, JoltConstraint)

	JoltConstraint::JoltConstraint() : _constraint(nullptr)
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
	}

	void JoltConstraint::SetEnabled(bool enabled)
	{
		if(_constraint) _constraint->SetEnabled(enabled);
	}

	JoltPointConstraint::JoltPointConstraint(JoltDynamicBody *body1, const Vector3 &localAnchor1, JoltDynamicBody *body2, const Vector3 &localAnchor2)
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
		settings.mPoint1 = JPH::Vec3(localAnchor1.x, localAnchor1.y, localAnchor1.z);
		settings.mPoint2 = JPH::Vec3(localAnchor2.x, localAnchor2.y, localAnchor2.z);
		SetConstraint(settings.Create(*b1, *b2));
	}

	JoltPointConstraint *JoltPointConstraint::WithBodiesAndOffsets(JoltDynamicBody *body1, const Vector3 &localAnchor1, JoltDynamicBody *body2, const Vector3 &localAnchor2)
	{
		JoltPointConstraint *constraint = new JoltPointConstraint(body1, localAnchor1, body2, localAnchor2);
		return constraint->Autorelease();
	}

	JoltFixedConstraint::JoltFixedConstraint(JoltDynamicBody *body1, const Vector3 &localAnchor1, const Quaternion &localRot1, JoltDynamicBody *body2, const Vector3 &localAnchor2, const Quaternion &localRot2)
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
		settings.mAutoDetectPoint = false;
		settings.mPoint1 = JPH::Vec3(localAnchor1.x, localAnchor1.y, localAnchor1.z);
		settings.mPoint2 = JPH::Vec3(localAnchor2.x, localAnchor2.y, localAnchor2.z);
		settings.mAxisX1 = JPH::Vec3(1.0f, 0.0f, 0.0f);
		settings.mAxisY1 = JPH::Vec3(0.0f, 1.0f, 0.0f);
		settings.mAxisX2 = JPH::Vec3(1.0f, 0.0f, 0.0f);
		settings.mAxisY2 = JPH::Vec3(0.0f, 1.0f, 0.0f);
		SetConstraint(settings.Create(*b1, *b2));
	}

	JoltFixedConstraint *JoltFixedConstraint::WithBodiesAndOffsets(JoltDynamicBody *body1, const Vector3 &localAnchor1, const Quaternion &localRot1, JoltDynamicBody *body2, const Vector3 &localAnchor2, const Quaternion &localRot2)
	{
		JoltFixedConstraint *constraint = new JoltFixedConstraint(body1, localAnchor1, localRot1, body2, localAnchor2, localRot2);
		return constraint->Autorelease();
	}

	JoltDistanceConstraint::JoltDistanceConstraint(JoltDynamicBody *body1, const Vector3 &localAnchor1, JoltDynamicBody *body2, const Vector3 &localAnchor2, float minDistance, float maxDistance)
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
		settings.mPoint1 = JPH::Vec3(localAnchor1.x, localAnchor1.y, localAnchor1.z);
		settings.mPoint2 = JPH::Vec3(localAnchor2.x, localAnchor2.y, localAnchor2.z);
		settings.mMinDistance = minDistance;
		settings.mMaxDistance = maxDistance;
		SetConstraint(settings.Create(*b1, *b2));
	}

	JoltDistanceConstraint *JoltDistanceConstraint::WithBodiesAndOffsets(JoltDynamicBody *body1, const Vector3 &localAnchor1, JoltDynamicBody *body2, const Vector3 &localAnchor2, float minDistance, float maxDistance)
	{
		JoltDistanceConstraint *constraint = new JoltDistanceConstraint(body1, localAnchor1, body2, localAnchor2, minDistance, maxDistance);
		return constraint->Autorelease();
	}

	static JPH::Quat ToJoltQuat(const Quaternion &q)
	{
		return JPH::Quat(q.x, q.y, q.z, q.w);
	}
	static JPH::Vec3 ToJoltVec3(const Vector3 &v)
	{
		return JPH::Vec3(v.x, v.y, v.z);
	}

	JoltSixDOFConstraint::JoltSixDOFConstraint(JoltDynamicBody *body1, const Vector3 &localAnchor1, const Quaternion &localRot1, JoltDynamicBody *body2, const Vector3 &localAnchor2, const Quaternion &localRot2)
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
		settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
		settings.mPosition1 = JPH::RVec3(localAnchor1.x, localAnchor1.y, localAnchor1.z);
		settings.mAxisX1 = ToJoltVec3(localRot1.GetRotatedVector(Vector3(1.0f, 0.0f, 0.0f)));
		settings.mAxisY1 = ToJoltVec3(localRot1.GetRotatedVector(Vector3(0.0f, 1.0f, 0.0f)));
		settings.mPosition2 = JPH::RVec3(localAnchor2.x, localAnchor2.y, localAnchor2.z);
		settings.mAxisX2 = ToJoltVec3(localRot2.GetRotatedVector(Vector3(1.0f, 0.0f, 0.0f)));
		settings.mAxisY2 = ToJoltVec3(localRot2.GetRotatedVector(Vector3(0.0f, 1.0f, 0.0f)));

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

	JoltSixDOFConstraint *JoltSixDOFConstraint::WithBodiesAndOffsets(JoltDynamicBody *body1, const Vector3 &localAnchor1, const Quaternion &localRot1, JoltDynamicBody *body2, const Vector3 &localAnchor2, const Quaternion &localRot2)
	{
		JoltSixDOFConstraint *c = new JoltSixDOFConstraint(body1, localAnchor1, localRot1, body2, localAnchor2, localRot2);
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
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		six->SetTranslationLimits(JPH::Vec3(limitMin.x, limitMin.y, limitMin.z), JPH::Vec3(limitMax.x, limitMax.y, limitMax.z));
	}

	void JoltSixDOFConstraint::SetRotationLimits(const Vector3 &limitMin, const Vector3 &limitMax)
	{
		if(!_constraint) return;
		JPH::SixDOFConstraint *six = static_cast<JPH::SixDOFConstraint *>(_constraint);
		six->SetRotationLimits(JPH::Vec3(limitMin.x, limitMin.y, limitMin.z), JPH::Vec3(limitMax.x, limitMax.y, limitMax.z));
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
