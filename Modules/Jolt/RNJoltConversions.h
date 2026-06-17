//
//  RNJoltConversions.h
//  Rayne-Jolt
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_JOLTCONVERSIONS_H_
#define __RAYNE_JOLTCONVERSIONS_H_

#include "RNJolt.h"

#include <Jolt/Jolt.h>

namespace RN
{
	class JoltConversions
	{
	public:
		static JPH::Vec3 ToJoltVec3(const Vector3 &vector)
		{
			return JPH::Vec3(vector.x, vector.y, vector.z);
		}

		static JPH::RVec3 ToJoltRVec3(const Vector3 &vector)
		{
			return JPH::RVec3(vector.x, vector.y, vector.z);
		}

		static JPH::RVec3 ToJoltRVec3(const DVector3 &vector)
		{
			return JPH::RVec3(vector.x, vector.y, vector.z);
		}

		static JPH::Quat ToJoltQuat(const Quaternion &quaternion)
		{
			return JPH::Quat(quaternion.x, quaternion.y, quaternion.z, quaternion.w);
		}

		static JPH::RMat44 ToJoltRMat44(const Quaternion &rotation, const JPH::RVec3 &position)
		{
			return JPH::RMat44::sRotationTranslation(ToJoltQuat(rotation), position);
		}

		static JPH::RMat44 ToJoltRMat44(const JPH::Quat &rotation, const JPH::RVec3 &position)
		{
			return JPH::RMat44::sRotationTranslation(rotation, position);
		}

		static JPH::RVec3 ToJoltPosition(const JoltPosition &position)
		{
			return ToJoltRVec3(position);
		}

		static JPH::Vec3 ToJoltVector(const Vector3 &vector)
		{
			return ToJoltVec3(vector);
		}

		static Vector3 ToVector3(const JPH::Vec3 &vector)
		{
			return Vector3(vector.GetX(), vector.GetY(), vector.GetZ());
		}

		static Vector3 ToVector3FromRVec3(const JPH::RVec3 &vector)
		{
			return Vector3(vector.GetX(), vector.GetY(), vector.GetZ());
		}

		static DVector3 ToDVector3(const JPH::RVec3 &vector)
		{
			return DVector3(vector.GetX(), vector.GetY(), vector.GetZ());
		}

		static JoltPosition ToPosition(const JPH::RVec3 &vector)
		{
#if RN_ENABLE_UNIVERSE_SCALE
			return ToDVector3(vector);
#else
			return ToVector3FromRVec3(vector);
#endif
		}

		static Vector3 ToEngineVector(const JPH::Vec3 &vector)
		{
			return ToVector3(vector);
		}

		static Vector3 ToVector3(const JoltPosition &vector)
		{
#if RN_ENABLE_UNIVERSE_SCALE
			return vector.ToVector3();
#else
			return vector;
#endif
		}

		static JPH::RVec3 GetAttachmentPosition(const SceneNodeAttachment *attachment)
		{
#if RN_ENABLE_UNIVERSE_SCALE
			return ToJoltRVec3(attachment->GetUniversePosition());
#else
			return ToJoltRVec3(attachment->GetWorldPosition());
#endif
		}

		static JPH::RVec3 GetAttachmentPosition(const SceneNodeAttachment *attachment, const Vector3 &offset)
		{
#if RN_ENABLE_UNIVERSE_SCALE
			return ToJoltRVec3(attachment->GetUniversePosition() + DVector3(offset));
#else
			return ToJoltRVec3(attachment->GetWorldPosition() + offset);
#endif
		}

		static void SetAttachmentPosition(SceneNodeAttachment *attachment, const JPH::RVec3 &position)
		{
#if RN_ENABLE_UNIVERSE_SCALE
			attachment->SetUniversePosition(ToDVector3(position));
#else
			attachment->SetWorldPosition(ToVector3FromRVec3(position));
#endif
		}

		static void SetAttachmentPosition(SceneNodeAttachment *attachment, const JPH::RVec3 &position, const Vector3 &offset)
		{
#if RN_ENABLE_UNIVERSE_SCALE
			attachment->SetUniversePosition(ToDVector3(position) + DVector3(offset));
#else
			attachment->SetWorldPosition(ToVector3FromRVec3(position) + offset);
#endif
		}
	};
} // namespace RN

#endif /* __RAYNE_JOLTCONVERSIONS_H_ */
