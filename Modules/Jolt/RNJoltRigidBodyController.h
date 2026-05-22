//
//  RNJoltRigidBodyController.h
//  Rayne-Jolt
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_JOLTRIGIDBODYCONTROLLER_H_
#define __RAYNE_JOLTRIGIDBODYCONTROLLER_H_

#include "RNJolt.h"
#include "RNJoltCollisionObject.h"
#include "RNJoltShape.h"

namespace JPH
{
	class Character;
}

namespace RN
{
	class JoltRigidBodyController : public JoltCollisionObject
	{
	public:
		JTAPI JoltRigidBodyController(float radius, float height, float groundTolerance = 0.05f, float mass = 80.0f);
		JTAPI ~JoltRigidBodyController() override;

		JTAPI void UpdatePosition() override;

		JTAPI void Move(const Vector3 &velocity, float delta);
		JTAPI bool Resize(float height, bool checkIfBlocked = true);

		JTAPI void SetCollisionFilter(uint32 group, uint32 mask) override;
		JTAPI Vector3 GetFeetOffset() const;
		JTAPI Vector3 GetGroundVelocity() const { return _groundVelocity; }

		JoltShape *GetShape() const { return _shape; }
		SceneNode *GetObjectBelow() const { return _objectBelow; }
		bool GetIsFalling() const { return _isFalling; }

	protected:
		void DidUpdate(SceneNode::ChangeSet changeSet) override;

		JoltShape *_shape;
		JPH::Character *_controller;

		float _radius;
		float _height;
		float _groundTolerance;
		SceneNode *_objectBelow;
		Vector3 _groundVelocity;
		bool _isFalling;

		void UpdateControllerTransform();
		void UpdateGroundState();

		RNDeclareMetaAPI(JoltRigidBodyController, JTAPI)
	};
} // namespace RN

#endif /* defined(__RAYNE_JOLTRIGIDBODYCONTROLLER_H_) */
