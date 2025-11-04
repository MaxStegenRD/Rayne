//
//  RNJoltInternals.cpp
//  Rayne-Jolt
//
//  Copyright 2023 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNJoltInternals.h"
#include "RNJoltCollisionObject.h"
#include "RNJoltKinematicController.h"
#include "RNJoltWorld.h"

namespace RN
{
	void JoltCharacterContactListener::OnAdjustBodyVelocity(const JPH::CharacterVirtual *inCharacter, const JPH::Body &inBody2, JPH::Vec3 &ioLinearVelocity, JPH::Vec3 &ioAngularVelocity)
	{
		/* Do nothing, the linear and angular velocity are already filled in */
	}

	bool JoltCharacterContactListener::OnContactValidate(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2, const JPH::SubShapeID &inSubShapeID2)
	{
		return true;
	}

	void JoltCharacterContactListener::OnContactAdded(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2, const JPH::SubShapeID &inSubShapeID2, JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal, JPH::CharacterContactSettings &ioSettings)
	{
		/* Default do nothing */
		JoltContactInfo info;

		info.collisionObject = reinterpret_cast<JoltCollisionObject *>(JoltWorld::GetSharedInstance()->GetJoltInstance()->GetBodyInterface().GetUserData(inBodyID2));
		if(info.collisionObject) info.node = info.collisionObject->GetParent();
		if(info.node) info.node->Retain()->Autorelease();

		info.position.x = inContactPosition.GetX();
		info.position.y = inContactPosition.GetY();
		info.position.z = inContactPosition.GetZ();
		info.normal.x = -inContactNormal.GetX();
		info.normal.y = -inContactNormal.GetY();
		info.normal.z = -inContactNormal.GetZ();
		info.distance = 0.0f;
		if(controller->_contactCallback) controller->_contactCallback(info.collisionObject, info, JoltCollisionObject::ContactState::Begin);
	}

	void JoltCharacterContactListener::OnContactSolve(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2, const JPH::SubShapeID &inSubShapeID2, JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal, JPH::Vec3Arg inContactVelocity, const JPH::PhysicsMaterial *inContactMaterial, JPH::Vec3Arg inCharacterVelocity, JPH::Vec3 &ioNewCharacterVelocity)
	{
		/* Default do nothing */
	}
} // namespace RN
