
//  RNAABB.h
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_AABB_H__
#define __RAYNE_AABB_H__

#include "../Base/RNBase.h"
#include "RNMatrix.h"
#include "RNQuaternion.h"
#include "RNVector.h"

namespace RN
{
	class AABB
	{
	public:
		AABB() = default;
		AABB(const Vector3 &min, const Vector3 &max);
		AABB(const Vector3 &pos, const float radius);

		AABB operator+(const AABB &other) const;
		AABB &operator+=(const AABB &other);

		AABB operator*(const Vector3 &other) const;
		AABB &operator*=(const Vector3 &other);

		bool Intersects(const AABB &other) const;
		bool Contains(const PositionType &position) const;

		void Rotate(const Quaternion &rotation);

		PositionType position;
		Vector3 minExtend;
		Vector3 maxExtend;
	};

	RN_INLINE AABB::AABB(const Vector3 &tmin, const Vector3 &tmax)
	{
		minExtend.x = std::min(tmin.x, tmax.x);
		minExtend.y = std::min(tmin.y, tmax.y);
		minExtend.z = std::min(tmin.z, tmax.z);

		maxExtend.x = std::max(tmin.x, tmax.x);
		maxExtend.y = std::max(tmin.y, tmax.y);
		maxExtend.z = std::max(tmin.z, tmax.z);
	}

	RN_INLINE AABB::AABB(const Vector3 &pos, const float radius)
	{
		Vector3 dist(radius);

		minExtend = -dist;
		maxExtend = dist;

		position = pos;
	}

	RN_INLINE AABB AABB::operator+(const AABB &other) const
	{
		AABB result(*this);
		result += other;
		return result;
	}

	RN_INLINE AABB &AABB::operator+=(const AABB &other)
	{
		const Vector3 relativePosition(other.position - position);

		minExtend.x = std::min(minExtend.x, relativePosition.x + other.minExtend.x);
		minExtend.y = std::min(minExtend.y, relativePosition.y + other.minExtend.y);
		minExtend.z = std::min(minExtend.z, relativePosition.z + other.minExtend.z);

		maxExtend.x = std::max(maxExtend.x, relativePosition.x + other.maxExtend.x);
		maxExtend.y = std::max(maxExtend.y, relativePosition.y + other.maxExtend.y);
		maxExtend.z = std::max(maxExtend.z, relativePosition.z + other.maxExtend.z);

		return *this;
	}

	RN_INLINE AABB AABB::operator*(const Vector3 &other) const
	{
		AABB result = *this;

		result.minExtend *= other;
		result.maxExtend *= other;

		return result;
	}

	RN_INLINE AABB &AABB::operator*=(const Vector3 &other)
	{
		minExtend *= other;
		maxExtend *= other;

		return *this;
	}

	RN_INLINE bool AABB::Intersects(const AABB &other) const
	{
		const PositionType relativePosition = other.position - position;

		if(minExtend.x > relativePosition.x + other.maxExtend.x || relativePosition.x + other.minExtend.x > maxExtend.x)
			return false;
		if(minExtend.y > relativePosition.y + other.maxExtend.y || relativePosition.y + other.minExtend.y > maxExtend.y)
			return false;
		if(minExtend.z > relativePosition.z + other.maxExtend.z || relativePosition.z + other.minExtend.z > maxExtend.z)
			return false;

		return true;
	}

	RN_INLINE bool AABB::Contains(const PositionType &position) const
	{
		const PositionType relativePosition = position - this->position;

		if(relativePosition.x > maxExtend.x)
			return false;
		if(relativePosition.x < minExtend.x)
			return false;
		if(relativePosition.y > maxExtend.y)
			return false;
		if(relativePosition.y < minExtend.y)
			return false;
		if(relativePosition.z > maxExtend.z)
			return false;
		if(relativePosition.z < minExtend.z)
			return false;

		return true;
	}

	RN_INLINE void AABB::Rotate(const Quaternion &rotation)
	{
		Matrix matrix = rotation.GetRotationMatrix();

		Vector3 corners[8];
		corners[0] = matrix * Vector3(minExtend.x, minExtend.y, minExtend.z);
		corners[1] = matrix * Vector3(minExtend.x, minExtend.y, maxExtend.z);
		corners[2] = matrix * Vector3(minExtend.x, maxExtend.y, minExtend.z);
		corners[3] = matrix * Vector3(minExtend.x, maxExtend.y, maxExtend.z);
		corners[4] = matrix * Vector3(maxExtend.x, minExtend.y, minExtend.z);
		corners[5] = matrix * Vector3(maxExtend.x, minExtend.y, maxExtend.z);
		corners[6] = matrix * Vector3(maxExtend.x, maxExtend.y, minExtend.z);
		corners[7] = matrix * Vector3(maxExtend.x, maxExtend.y, maxExtend.z);

		minExtend = matrix * minExtend;
		maxExtend = matrix * maxExtend;

		for(Vector3 &corner : corners)
		{
			//This used to use std::min, but could trigger an exception in visual studio debug builds
			//if both values are equal, even though the standard explicitly allows it...
			minExtend.x = corner.x < minExtend.x ? corner.x : minExtend.x;
			minExtend.y = corner.y < minExtend.y ? corner.y : minExtend.y;
			minExtend.z = corner.z < minExtend.z ? corner.z : minExtend.z;

			maxExtend.x = corner.x > maxExtend.x ? corner.x : maxExtend.x;
			maxExtend.y = corner.y > maxExtend.y ? corner.y : maxExtend.y;
			maxExtend.z = corner.z > maxExtend.z ? corner.z : maxExtend.z;
		}
	}
} // namespace RN

#endif /* __RAYNE_AABB_H__ */
