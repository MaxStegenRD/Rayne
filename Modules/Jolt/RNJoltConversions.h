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
#include "RNJoltWorld.h"

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

		static JPH::Quat ToJoltRotation(const Quaternion &rotation)
		{
			Quaternion result(rotation);
			result.Normalize();
			return ToJoltQuat(result);
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

		static Quaternion ToEngineRotation(const JPH::Quat &rotation)
		{
			Quaternion result(rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW());
			result.Normalize();
			return result;
		}

		static JPH::RVec3 ToJoltScenePosition(const JoltPosition &position)
		{
			JoltWorld *world = JoltWorld::GetSharedInstance();
			return world ? ToJoltPosition(world->ConvertPositionToPhysicsWorld(position)) : ToJoltPosition(position);
		}

		static JPH::Quat ToJoltSceneRotation(const Quaternion &rotation)
		{
			JoltWorld *world = JoltWorld::GetSharedInstance();
			return world ? ToJoltRotation(world->ConvertRotationToPhysicsWorld(rotation)) : ToJoltRotation(rotation);
		}

		static JoltPosition ToScenePosition(const JPH::RVec3 &vector)
		{
			JoltWorld *world = JoltWorld::GetSharedInstance();
			return world ? world->ConvertPositionFromPhysicsWorld(ToPosition(vector)) : ToPosition(vector);
		}

		static Quaternion ToSceneRotation(const JPH::Quat &rotation)
		{
			JoltWorld *world = JoltWorld::GetSharedInstance();
			return world ? world->ConvertRotationFromPhysicsWorld(ToEngineRotation(rotation)) : ToEngineRotation(rotation);
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
			return ToJoltScenePosition(attachment->GetUniversePosition());
#else
			return ToJoltScenePosition(attachment->GetWorldPosition());
#endif
		}

		static JPH::RVec3 GetAttachmentPosition(const SceneNodeAttachment *attachment, const Vector3 &offset)
		{
#if RN_ENABLE_UNIVERSE_SCALE
			return ToJoltScenePosition(attachment->GetUniversePosition() + DVector3(offset));
#else
			return ToJoltScenePosition(attachment->GetWorldPosition() + offset);
#endif
		}

		static void SetAttachmentPosition(SceneNodeAttachment *attachment, const JPH::RVec3 &position)
		{
#if RN_ENABLE_UNIVERSE_SCALE
			attachment->SetUniversePosition(ToScenePosition(position));
#else
			attachment->SetWorldPosition(ToScenePosition(position));
#endif
		}

		static void SetAttachmentPosition(SceneNodeAttachment *attachment, const JPH::RVec3 &position, const Vector3 &offset)
		{
#if RN_ENABLE_UNIVERSE_SCALE
			attachment->SetUniversePosition(ToScenePosition(position) + DVector3(offset));
#else
			attachment->SetWorldPosition(ToScenePosition(position) + offset);
#endif
		}
	};
} // namespace RN

#endif /* __RAYNE_JOLTCONVERSIONS_H_ */
