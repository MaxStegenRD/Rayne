//
//  RNJoltConstraint.h
//  Rayne-Jolt
//
//  Copyright 2023 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_JOLTCONSTRAINT_H_
#define __RAYNE_JOLTCONSTRAINT_H_

#include "RNJolt.h"
#include "RNJoltDynamicBody.h"

namespace JPH
{
	class Constraint;
}

namespace RN
{
	class JoltConstraint : public Object
	{
	public:
		JTAPI JPH::Constraint *GetJoltConstraint() const { return _constraint; }
		JTAPI void SetEnabled(bool enabled);
		JTAPI void SetCollisionsEnabled(bool enabled);
		JTAPI bool GetCollisionsEnabled() const { return _collisionsEnabled; }
		JTAPI void SetSolverIterationCount(uint32 positionIterations, uint32 velocityIterations); //0 resets to the default

	protected:
		JoltConstraint();
		~JoltConstraint() override;

		void SetConstraint(JPH::Constraint *constraint);
		void ResetStoredBodyPairCollisionState();
		void UpdateBodyPairCollisionState();
		void SetStoredBodyPairCollisionEnabled(bool enabled);
		bool HasStoredBodyPair() const;

		JPH::Constraint *_constraint;
		uint32 _bodyPairCollisionBody1;
		uint32 _bodyPairCollisionBody2;
		bool _bodyPairCollisionDisabled;
		bool _collisionsEnabled;

		RNDeclareMetaAPI(JoltConstraint, JTAPI)
	};

	class JoltPointConstraint : public JoltConstraint
	{
	public:
		JTAPI JoltPointConstraint(JoltDynamicBody *body1, const Vector3 &worldPoint1, JoltDynamicBody *body2, const Vector3 &worldPoint2);
		JTAPI static JoltPointConstraint *WithBodiesAndWorldPoints(JoltDynamicBody *body1, const Vector3 &worldPoint1, JoltDynamicBody *body2, const Vector3 &worldPoint2);

		RNDeclareMetaAPI(JoltPointConstraint, JTAPI)
	};

	class JoltFixedConstraint : public JoltConstraint
	{
	public:
		JTAPI JoltFixedConstraint(JoltDynamicBody *body1, const Vector3 &worldPosition1, const Quaternion &worldRotation1, JoltDynamicBody *body2, const Vector3 &worldPosition2, const Quaternion &worldRotation2);
		JTAPI static JoltFixedConstraint *WithBodiesAndWorldFrames(JoltDynamicBody *body1, const Vector3 &worldPosition1, const Quaternion &worldRotation1, JoltDynamicBody *body2, const Vector3 &worldPosition2, const Quaternion &worldRotation2);

		RNDeclareMetaAPI(JoltFixedConstraint, JTAPI)
	};

	class JoltDistanceConstraint : public JoltConstraint
	{
	public:
		JTAPI JoltDistanceConstraint(JoltDynamicBody *body1, const Vector3 &worldPoint1, JoltDynamicBody *body2, const Vector3 &worldPoint2, float minDistance, float maxDistance);
		JTAPI static JoltDistanceConstraint *WithBodiesAndWorldPoints(JoltDynamicBody *body1, const Vector3 &worldPoint1, JoltDynamicBody *body2, const Vector3 &worldPoint2, float minDistance, float maxDistance);

		RNDeclareMetaAPI(JoltDistanceConstraint, JTAPI)
	};

	class JoltSixDOFConstraint : public JoltConstraint
	{
	public:
		enum class Axis
		{
			TranslationX,
			TranslationY,
			TranslationZ,
			RotationX,
			RotationY,
			RotationZ
		};

		JTAPI JoltSixDOFConstraint(JoltDynamicBody *body1, const Vector3 &worldPosition1, const Quaternion &worldRotation1, JoltDynamicBody *body2, const Vector3 &worldPosition2, const Quaternion &worldRotation2);
		JTAPI static JoltSixDOFConstraint *WithBodiesAndWorldFrames(JoltDynamicBody *body1, const Vector3 &worldPosition1, const Quaternion &worldRotation1, JoltDynamicBody *body2, const Vector3 &worldPosition2, const Quaternion &worldRotation2);

		JTAPI void SetMotorState(Axis axis, int state); // 0=Off,1=Velocity,2=Position
		JTAPI void SetTargetPositionCS(const Vector3 &p_cs);
		JTAPI void SetTargetVelocityCS(const Vector3 &v_cs);
		JTAPI void SetTargetAngularVelocityCS(const Vector3 &w_cs);
		JTAPI void SetTargetOrientationCS(const Quaternion &q_cs);
		JTAPI void SetTargetOrientationBS(const Quaternion &q_bs);

		JTAPI void SetLinearMotorParams(float frequency, float damping, float maxForce);
		JTAPI void SetAngularMotorParams(float frequency, float damping, float maxTorque);

		// Limits and springs/friction wrappers
		JTAPI void SetTranslationLimits(const Vector3 &limitMin, const Vector3 &limitMax);
		JTAPI void SetRotationLimits(const Vector3 &limitMin, const Vector3 &limitMax);
		JTAPI void SetTranslationSpringParams(float frequency, float damping); // apply to X/Y/Z
		JTAPI void SetTranslationSpringParamsAxis(Axis axis, float frequency, float damping); // axis must be TranslationX/Y/Z
		JTAPI void SetMaxFriction(Axis axis, float maxFriction);

		RNDeclareMetaAPI(JoltSixDOFConstraint, JTAPI)
	};
} // namespace RN

#endif /* defined(__RAYNE_JOLTCONSTRAINT_H_) */
