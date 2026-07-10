//
//  RNJolt.h
//  Rayne-Jolt
//
//  Copyright 2023 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_JOLT_H_
#define __RAYNE_JOLT_H_

#include <Rayne.h>

#if defined(RN_BUILD_JOLT)
	#define JTAPI RN_EXPORT
#else
	#define JTAPI RN_IMPORT
#endif

namespace JPH
{
	class Constraint;
}

namespace RN
{
	class JoltConstraintOwner
	{
	public:
		virtual ~JoltConstraintOwner() = default;
		virtual void InvalidateJoltConstraint(JPH::Constraint *constraint) = 0;
	};

#if RN_ENABLE_UNIVERSE_SCALE
	using JoltPosition = DVector3;
#else
	using JoltPosition = Vector3;
#endif
} // namespace RN

#endif /* __RAYNE_JOLT_H_ */
