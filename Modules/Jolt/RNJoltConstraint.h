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

    protected:
        JoltConstraint();
        ~JoltConstraint() override;

        void SetConstraint(JPH::Constraint *constraint);

        JPH::Constraint *_constraint;

        RNDeclareMetaAPI(JoltConstraint, JTAPI)
    };

    class JoltPointConstraint : public JoltConstraint
    {
    public:
        JTAPI JoltPointConstraint(JoltDynamicBody *body1, const Vector3 &localAnchor1, JoltDynamicBody *body2, const Vector3 &localAnchor2);
        JTAPI static JoltPointConstraint *WithBodiesAndOffsets(JoltDynamicBody *body1, const Vector3 &localAnchor1, JoltDynamicBody *body2, const Vector3 &localAnchor2);

        RNDeclareMetaAPI(JoltPointConstraint, JTAPI)
    };

    class JoltFixedConstraint : public JoltConstraint
    {
    public:
        JTAPI JoltFixedConstraint(JoltDynamicBody *body1, const Vector3 &localAnchor1, const Quaternion &localRot1, JoltDynamicBody *body2, const Vector3 &localAnchor2, const Quaternion &localRot2);
        JTAPI static JoltFixedConstraint *WithBodiesAndOffsets(JoltDynamicBody *body1, const Vector3 &localAnchor1, const Quaternion &localRot1, JoltDynamicBody *body2, const Vector3 &localAnchor2, const Quaternion &localRot2);

        RNDeclareMetaAPI(JoltFixedConstraint, JTAPI)
    };

    class JoltDistanceConstraint : public JoltConstraint
    {
    public:
        JTAPI JoltDistanceConstraint(JoltDynamicBody *body1, const Vector3 &localAnchor1, JoltDynamicBody *body2, const Vector3 &localAnchor2, float minDistance, float maxDistance);
        JTAPI static JoltDistanceConstraint *WithBodiesAndOffsets(JoltDynamicBody *body1, const Vector3 &localAnchor1, JoltDynamicBody *body2, const Vector3 &localAnchor2, float minDistance, float maxDistance);

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

        JTAPI JoltSixDOFConstraint(JoltDynamicBody *body1, const Vector3 &localAnchor1, const Quaternion &localRot1,
                                   JoltDynamicBody *body2, const Vector3 &localAnchor2, const Quaternion &localRot2);
        JTAPI static JoltSixDOFConstraint *WithBodiesAndOffsets(JoltDynamicBody *body1, const Vector3 &localAnchor1, const Quaternion &localRot1,
                                                                JoltDynamicBody *body2, const Vector3 &localAnchor2, const Quaternion &localRot2);

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
