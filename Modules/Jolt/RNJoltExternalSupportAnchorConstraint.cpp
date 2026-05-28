//
//  RNJoltExternalSupportAnchorConstraint.cpp
//  Rayne-Jolt
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNJoltExternalSupportAnchorConstraint.h"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Constraints/ConstraintPart/ContactConstraintPart.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/StateRecorder.h>

namespace RN
{
	constexpr float ExternalSupportAnchorBiasScale = 1.0f;
	constexpr float ExternalSupportAnchorDeltaEpsilon = 0.000001f;

	class JoltExternalSupportAnchorConstraint;

	// This anchor follows a moving support body without applying the equal and opposite impulse back into it.
	class JoltExternalSupportAnchorConstraintSettings : public JPH::TwoBodyConstraintSettings
	{
	public:
		JPH::RVec3 _position1 = JPH::RVec3::sZero();
		JPH::RVec3 _position2 = JPH::RVec3::sZero();
		float _maxForce = 0.0f;

		JPH::TwoBodyConstraint *Create(JPH::Body &body1, JPH::Body &body2) const override;
	};

	class JoltExternalSupportAnchorConstraint : public JPH::TwoBodyConstraint
	{
	public:
		JoltExternalSupportAnchorConstraint(JPH::Body &body1, JPH::Body &body2, const JoltExternalSupportAnchorConstraintSettings &settings) :
			JPH::TwoBodyConstraint(body1, body2, settings),
			_localSpacePosition1(JPH::Vec3(body1.GetInverseCenterOfMassTransform() * settings._position1)),
			_localSpacePosition2(JPH::Vec3(body2.GetInverseCenterOfMassTransform() * settings._position2)),
			_maxForce(settings._maxForce)
		{}

		JPH::EConstraintSubType GetSubType() const override { return JPH::EConstraintSubType::User1; }
		void NotifyShapeChanged(const JPH::BodyID &bodyID, JPH::Vec3Arg deltaCOM) override;
		void SetupVelocityConstraint(float deltaTime) override;
		void ResetWarmStart() override;
		void WarmStartVelocityConstraint(float) override;
		bool SolveVelocityConstraint(float deltaTime) override;
		bool SolvePositionConstraint(float, float) override;
		void SaveState(JPH::StateRecorder &stream) const override;
		void RestoreState(JPH::StateRecorder &stream) override;
		JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override;
		JPH::Mat44 GetConstraintToBody1Matrix() const override { return JPH::Mat44::sTranslation(_localSpacePosition1); }
		JPH::Mat44 GetConstraintToBody2Matrix() const override { return JPH::Mat44::sTranslation(_localSpacePosition2); }

#ifdef JPH_DEBUG_RENDERER
		void DrawConstraint(JPH::DebugRenderer *) const override {}
#endif

	private:
		JPH::Vec3 _localSpacePosition1;
		JPH::Vec3 _localSpacePosition2;
		float _maxForce;
		JPH::Vec3 _axis[3];
		JPH::ContactConstraintPart<JPH::EMotionType::Kinematic, JPH::EMotionType::Dynamic> _axisConstraint[3];
		float _axisConstraintPadding = 0.0f;
	};

	JPH::TwoBodyConstraint *JoltExternalSupportAnchorConstraintSettings::Create(JPH::Body &body1, JPH::Body &body2) const
	{
		return new JoltExternalSupportAnchorConstraint(body1, body2, *this);
	}

	void JoltExternalSupportAnchorConstraint::NotifyShapeChanged(const JPH::BodyID &bodyID, JPH::Vec3Arg deltaCOM)
	{
		if(mBody1->GetID() == bodyID)
		{
			_localSpacePosition1 -= deltaCOM;
		}
		else if(mBody2->GetID() == bodyID)
		{
			_localSpacePosition2 -= deltaCOM;
		}
	}

	void JoltExternalSupportAnchorConstraint::SetupVelocityConstraint(float deltaTime)
	{
		JPH::RVec3 worldPosition1 = mBody1->GetCenterOfMassTransform() * _localSpacePosition1;
		JPH::RVec3 worldPosition2 = mBody2->GetCenterOfMassTransform() * _localSpacePosition2;
		JPH::Vec3 delta = JPH::Vec3(worldPosition2 - worldPosition1);
		JPH::Vec3 r1PlusU = JPH::Vec3(worldPosition2 - mBody1->GetCenterOfMassPosition());
		JPH::Vec3 r2 = JPH::Vec3(worldPosition2 - mBody2->GetCenterOfMassPosition());
		float inverseDeltaTime = (deltaTime > ExternalSupportAnchorDeltaEpsilon) ? 1.0f / deltaTime : 0.0f;

		_axis[0] = JPH::Vec3::sAxisX();
		_axis[1] = JPH::Vec3::sAxisY();
		_axis[2] = JPH::Vec3::sAxisZ();
		JPH::MotionProperties *motionProperties2 = mBody2->IsDynamic() ? mBody2->GetMotionPropertiesUnchecked() : nullptr;

		for(int axis = 0; axis < 3; axis += 1)
		{
			if(motionProperties2)
			{
				float bias = _axis[axis].Dot(delta) * inverseDeltaTime * ExternalSupportAnchorBiasScale;
				_axisConstraint[axis].CalculateConstraintProperties(0.0f, JPH::Mat44(), r1PlusU, motionProperties2->GetInverseMass(), mBody2->GetInverseInertia(), r2, _axis[axis], bias);
				_axisConstraint[axis].SetTotalLambda(0.0f);
			}
			else
			{
				_axisConstraint[axis].Deactivate();
			}
		}
	}

	void JoltExternalSupportAnchorConstraint::ResetWarmStart()
	{
		for(int axis = 0; axis < 3; axis += 1)
		{
			_axisConstraint[axis].Deactivate();
		}
	}

	void JoltExternalSupportAnchorConstraint::WarmStartVelocityConstraint(float)
	{
	}

	bool JoltExternalSupportAnchorConstraint::SolveVelocityConstraint(float deltaTime)
	{
		float maxLambda = _maxForce * deltaTime;
		JPH::MotionProperties *motionProperties1 = !mBody1->IsStatic() ? mBody1->GetMotionPropertiesUnchecked() : nullptr;
		JPH::MotionProperties *motionProperties2 = mBody2->IsDynamic() ? mBody2->GetMotionPropertiesUnchecked() : nullptr;
		bool didApplyImpulse = false;

		if(!motionProperties2)
		{
			return false;
		}

		JPH::Vec3 linearVelocity1 = motionProperties1 ? motionProperties1->GetLinearVelocity() : JPH::Vec3::sZero();
		JPH::Vec3 angularVelocity1 = motionProperties1 ? motionProperties1->GetAngularVelocity() : JPH::Vec3::sZero();

		for(int axis = 0; axis < 3; axis += 1)
		{
			if(_axisConstraint[axis].IsActive())
			{
				JPH::Vec3 linearVelocity2 = motionProperties2->GetLinearVelocity();
				JPH::Vec3 angularVelocity2 = motionProperties2->GetAngularVelocity();
				if(_axisConstraint[axis].SolveVelocityConstraint(linearVelocity1, angularVelocity1, linearVelocity2, angularVelocity2, 0.0f, motionProperties2->GetInverseMass(), _axis[axis], -maxLambda, maxLambda))
				{
					motionProperties2->ApplyLinearVelocityStep(linearVelocity2);
					motionProperties2->ApplyAngularVelocityStep(angularVelocity2);
					didApplyImpulse = true;
				}
			}
		}

		return didApplyImpulse;
	}

	bool JoltExternalSupportAnchorConstraint::SolvePositionConstraint(float, float baumgarte)
	{
		if(!mBody2->IsDynamic())
		{
			return false;
		}

		JPH::RVec3 worldPosition1 = mBody1->GetCenterOfMassTransform() * _localSpacePosition1;
		JPH::RVec3 worldPosition2 = mBody2->GetCenterOfMassTransform() * _localSpacePosition2;
		JPH::Vec3 delta = JPH::Vec3(worldPosition2 - worldPosition1);
		float inverseMass2 = mBody2->GetMotionPropertiesUnchecked()->GetInverseMass();
		bool didApplyCorrection = false;

		for(int axis = 0; axis < 3; axis += 1)
		{
			if(_axisConstraint[axis].IsActive())
			{
				didApplyCorrection |= _axisConstraint[axis].SolvePositionConstraint(*mBody1, 0.0f, *mBody2, inverseMass2, _axis[axis], _axis[axis].Dot(delta), baumgarte);
			}
		}

		return didApplyCorrection;
	}

	void JoltExternalSupportAnchorConstraint::SaveState(JPH::StateRecorder &stream) const
	{
		JPH::TwoBodyConstraint::SaveState(stream);

		for(int axis = 0; axis < 3; axis += 1)
		{
			stream.Write(_axisConstraint[axis].GetTotalLambda());
		}
	}

	void JoltExternalSupportAnchorConstraint::RestoreState(JPH::StateRecorder &stream)
	{
		JPH::TwoBodyConstraint::RestoreState(stream);

		for(int axis = 0; axis < 3; axis += 1)
		{
			float totalLambda = 0.0f;
			stream.Read(totalLambda);
			_axisConstraint[axis].SetTotalLambda(totalLambda);
		}
	}

	JPH::Ref<JPH::ConstraintSettings> JoltExternalSupportAnchorConstraint::GetConstraintSettings() const
	{
		JoltExternalSupportAnchorConstraintSettings *settings = new JoltExternalSupportAnchorConstraintSettings;
		ToConstraintSettings(*settings);
		settings->_position1 = mBody1->GetCenterOfMassTransform() * _localSpacePosition1;
		settings->_position2 = mBody2->GetCenterOfMassTransform() * _localSpacePosition2;
		settings->_maxForce = _maxForce;
		return settings;
	}

	JPH::Constraint *CreateJoltExternalSupportAnchorConstraint(JPH::Body &supportBody, JPH::Body &controllerBody, JPH::RVec3Arg supportAnchorPosition, JPH::RVec3Arg controllerAnchorPosition, float maxForce)
	{
		JoltExternalSupportAnchorConstraintSettings settings;
		settings._position1 = supportAnchorPosition;
		settings._position2 = controllerAnchorPosition;
		settings._maxForce = maxForce;
		return settings.Create(supportBody, controllerBody);
	}
}
