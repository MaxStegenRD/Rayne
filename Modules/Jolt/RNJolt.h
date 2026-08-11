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

	// Positions inside the Jolt simulation frame. Geometry, offsets, and vectors remain float.
	using JoltPosition = PositionType;
} // namespace RN

#endif /* __RAYNE_JOLT_H_ */
