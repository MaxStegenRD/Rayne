//
//  RNJoltBodyMotion.h
//  Rayne-Jolt
//
//  Copyright 2026 by Überpixel. All rights reserved.
//

#ifndef __RAYNE_JOLTBODYMOTION_H_
#define __RAYNE_JOLTBODYMOTION_H_

#include "RNJolt.h"

namespace RN
{
	struct JoltPointMotionProperties
	{
		Vector3 velocity = Vector3();
		Vector3 centerOffset = Vector3();
		Vector3 inverseInertiaColumnX = Vector3();
		Vector3 inverseInertiaColumnY = Vector3();
		Vector3 inverseInertiaColumnZ = Vector3();
		float inverseMass = 0.0f;
		bool isDynamic = false;

		float GetPointImpulseEffectiveMass(const Vector3 &direction) const
		{
			if(!isDynamic || !direction.IsValid() || direction.GetSquaredLength() <= k::EpsilonFloat) return 0.0f;

			Vector3 normalizedDirection = direction.GetNormalized();
			Vector3 angularJacobian = centerOffset.GetCrossProduct(normalizedDirection);
			Vector3 angularVelocityPerImpulse = inverseInertiaColumnX * angularJacobian.x + inverseInertiaColumnY * angularJacobian.y + inverseInertiaColumnZ * angularJacobian.z;
			float denominator = inverseMass + angularJacobian.GetDotProduct(angularVelocityPerImpulse);
			return denominator > k::EpsilonFloat ? 1.0f / denominator : 0.0f;
		}
	};
} // namespace RN

#endif /* __RAYNE_JOLTBODYMOTION_H_ */
