//
//  RNJoltExternalSupportAnchorConstraint.h
//  Rayne-Jolt
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_JOLTEXTERNALSUPPORTANCHORCONSTRAINT_H_
#define __RAYNE_JOLTEXTERNALSUPPORTANCHORCONSTRAINT_H_

#include <Jolt/Jolt.h>
#include <Jolt/Math/Real.h>

namespace JPH
{
	class Body;
	class Constraint;
}

namespace RN
{
	JPH::Constraint *CreateJoltExternalSupportAnchorConstraint(JPH::Body &supportBody, JPH::Body &controllerBody, JPH::RVec3Arg supportAnchorPosition, JPH::RVec3Arg controllerAnchorPosition, float maxForce);
}

#endif /* defined(__RAYNE_JOLTEXTERNALSUPPORTANCHORCONSTRAINT_H_) */
