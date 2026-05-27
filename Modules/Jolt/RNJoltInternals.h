//
//  RNJoltInternals.h
//  Rayne-Jolt
//
//  Copyright 2023 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_JOLTINTERNALS_H_
#define __RAYNE_JOLTINTERNALS_H_

#include "RNJolt.h"
#include "RNJoltKinematicController.h"
#include "RNJoltCollisionObject.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

namespace RN
{
	class JoltObjectLayerMapper : public JPH::ObjectLayerPairFilter, public JPH::BroadPhaseLayerInterface, public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		JPH::ObjectLayer GetObjectLayer(uint32 collisionGroup, uint32 collisionMask, uint8 broadPhaseLayer)
		{
			uint64 upperBits = (uint64)collisionMask << 32U;
			uint64 lowerBits = (uint64)collisionGroup;
			uint64 collision = upperBits | lowerBits;

			size_t counter = 0;
			for(uint64 value : _objectLayerMapping)
			{
				if(value == collision)
				{
					uint16 objectLayerBits = static_cast<uint16>(counter);
					uint16 broadPhaseBits = (static_cast<uint16>(broadPhaseLayer) << 14U);
					return static_cast<JPH::ObjectLayer>(broadPhaseBits | objectLayerBits);
				}
				counter += 1;
			}

			_objectLayerMapping.push_back(collision);

			uint16 objectLayerBits = static_cast<uint16>(counter);
			uint16 broadPhaseBits = (static_cast<uint16>(broadPhaseLayer) << 14U);
			return static_cast<JPH::ObjectLayer>(broadPhaseBits | objectLayerBits);
		}

		uint32 GetCollisionGroup(JPH::ObjectLayer objectLayer) const
		{
			uint16 objectLayerIndex = static_cast<uint16>(objectLayer & 0b0011111111111111U);
			uint64 collision = _objectLayerMapping[static_cast<size_t>(objectLayerIndex)];
			return static_cast<uint32>(collision & 0xFFFFFFFFU);
		}

		uint32 GetCollisionMask(JPH::ObjectLayer objectLayer) const
		{
			uint16 objectLayerIndex = static_cast<uint16>(objectLayer & 0b0011111111111111U);
			uint64 collision = _objectLayerMapping[static_cast<size_t>(objectLayerIndex)];
			return static_cast<uint32>(collision >> 32U);
		}

		//From JPH::ObjectLayerPairFilter
		bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const final
		{
			uint32 collisionGroup1 = GetCollisionGroup(inObject1);
			uint32 collisionMask1 = GetCollisionMask(inObject1);

			uint32 collisionGroup2 = GetCollisionGroup(inObject2);
			uint32 collisionMask2 = GetCollisionMask(inObject2);

			bool filterMask = (collisionGroup1 & collisionMask2) && (collisionGroup2 & collisionMask1);
			//bool filterID = (filterData0.word3 == 0 && filterData1.word3 == 0) || (filterData0.word2 != filterData1.word3 && filterData0.word3 != filterData1.word2);
			return (filterMask); // && filterID)
		}

		//From JPH::ObjectVsBroadPhaseLayerFilter
		bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const final
		{
			/*uint16 broadPhaseLayer = (inLayer1 >> 30) & 0xf; //Encoded in the last 2 bits
			
			if(broadPhaseLayer == 0) //Is static object layer
			{
				return inLayer2 != JoltBroadPhaseLayers::LAYER_STATIC; //Only collide with none-static
			}*/

			return true; //Everything else collides with everything
		}

		//From JPH::BroadPhaseLayerInterface
		JPH::uint GetNumBroadPhaseLayers() const final
		{
			return 4;
		}

		//From JPH::BroadPhaseLayerInterface
		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const final
		{
			uint16 broadPhaseLayer = (inLayer >> 14U) & 0xf; //Encoded in the last 2 bits
			return JPH::BroadPhaseLayer(broadPhaseLayer);
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		//From JPH::BroadPhaseLayerInterface
		virtual const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
		{
			switch((JPH::BroadPhaseLayer::Type)inLayer)
			{
				case 0:
					return "LAYER_0";

				case 1:
					return "LAYER_1";

				case 2:
					return "LAYER_2";

				case 3:
					return "LAYER_3";

				default:
					JPH_ASSERT(false);
					return "INVALID";
			}
		}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

	private:
		std::vector<uint64> _objectLayerMapping;
	};

	// An example contact listener
	class JoltContactListener : public JPH::ContactListener
	{
	public:
		void SetBodyPairCollisionEnabled(const JPH::BodyID &body1, const JPH::BodyID &body2, bool enabled)
		{
			uint32 bodyID1;
			uint32 bodyID2;
			if(!GetBodyPair(body1, body2, bodyID1, bodyID2)) return;

			SetBodyPairTracked(_ignoredBodyPairs, bodyID1, bodyID2, !enabled);
		}

		void SetConnectedBodyCollisionFilteringEnabled(const JPH::BodyID &body1, const JPH::BodyID &body2, bool enabled, std::vector<JPH::BodyID> &affectedBodies)
		{
			uint32 bodyID1;
			uint32 bodyID2;
			if(!GetBodyPair(body1, body2, bodyID1, bodyID2)) return;

			CollectConnectedFilteredBodies(bodyID1, affectedBodies);
			CollectConnectedFilteredBodies(bodyID2, affectedBodies);

			SetBodyPairTracked(_connectedFilteredBodyPairs, bodyID1, bodyID2, enabled);

			CollectConnectedFilteredBodies(bodyID1, affectedBodies);
			CollectConnectedFilteredBodies(bodyID2, affectedBodies);
			affectedBodies.push_back(body1);
			affectedBodies.push_back(body2);
		}

		// See: ContactListener
		virtual JPH::ValidateResult OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult) override
		{
			AutoreleasePool pool;
//			RNDebug("Contact validate callback");

			if(ShouldIgnoreBodyPair(inBody1.GetID(), inBody2.GetID()))
				return JPH::ValidateResult::RejectContact;

			// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
			return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
		}

		virtual void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override
		{
			AutoreleasePool pool;
			// Forward to RN callback if present
			JoltCollisionObject *co1 = reinterpret_cast<JoltCollisionObject *>(inBody1.GetUserData());
			JoltCollisionObject *co2 = reinterpret_cast<JoltCollisionObject *>(inBody2.GetUserData());
			ApplyContactResponseMassScale(co1, co2, inBody1.GetID(), ioSettings, true);
			ApplyContactResponseMassScale(co2, co1, inBody2.GetID(), ioSettings, false);
			JPH::Vec3 joltVelocity1 = inBody1.GetLinearVelocity();
			JPH::Vec3 joltVelocity2 = inBody2.GetLinearVelocity();
			Vector3 velocity1(joltVelocity1.GetX(), joltVelocity1.GetY(), joltVelocity1.GetZ());
			Vector3 velocity2(joltVelocity2.GetX(), joltVelocity2.GetY(), joltVelocity2.GetZ());
			JoltContactInfo info1{};
			JoltContactInfo info2{};
			info1.node = co2 ? co2->GetParent() : nullptr;
			info1.collisionObject = co2;
			info1.linearVelocity = velocity1;
			info1.otherLinearVelocity = velocity2;
			info1.distance = 0.0f;
			info1.position = Vector3();
			info1.normal = Vector3();
			info2.node = co1 ? co1->GetParent() : nullptr;
			info2.collisionObject = co1;
			info2.linearVelocity = velocity2;
			info2.otherLinearVelocity = velocity1;
			info2.distance = 0.0f;
			info2.position = Vector3();
			info2.normal = Vector3();
			if(co1) co1->NotifyContact(info1, JoltCollisionObject::ContactState::Begin);
			if(co2) co2->NotifyContact(info2, JoltCollisionObject::ContactState::Begin);
		}

		virtual void OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override
		{
			AutoreleasePool pool;
			// Forward to RN callback if present
			JoltCollisionObject *co1 = reinterpret_cast<JoltCollisionObject *>(inBody1.GetUserData());
			JoltCollisionObject *co2 = reinterpret_cast<JoltCollisionObject *>(inBody2.GetUserData());
			ApplyContactResponseMassScale(co1, co2, inBody1.GetID(), ioSettings, true);
			ApplyContactResponseMassScale(co2, co1, inBody2.GetID(), ioSettings, false);
			JPH::Vec3 joltVelocity1 = inBody1.GetLinearVelocity();
			JPH::Vec3 joltVelocity2 = inBody2.GetLinearVelocity();
			Vector3 velocity1(joltVelocity1.GetX(), joltVelocity1.GetY(), joltVelocity1.GetZ());
			Vector3 velocity2(joltVelocity2.GetX(), joltVelocity2.GetY(), joltVelocity2.GetZ());
			JoltContactInfo info1{};
			JoltContactInfo info2{};
			info1.node = co2 ? co2->GetParent() : nullptr;
			info1.collisionObject = co2;
			info1.linearVelocity = velocity1;
			info1.otherLinearVelocity = velocity2;
			info1.distance = 0.0f;
			info1.position = Vector3();
			info1.normal = Vector3();
			info2.node = co1 ? co1->GetParent() : nullptr;
			info2.collisionObject = co1;
			info2.linearVelocity = velocity2;
			info2.otherLinearVelocity = velocity1;
			info2.distance = 0.0f;
			info2.position = Vector3();
			info2.normal = Vector3();
			if(co1) co1->NotifyContact(info1, JoltCollisionObject::ContactState::Continue);
			if(co2) co2->NotifyContact(info2, JoltCollisionObject::ContactState::Continue);
		}

		virtual void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override
		{
			AutoreleasePool pool;
//			RNDebug("A contact was removed");
		}

	private:
		struct CountedBodyPair
		{
			uint32 body1;
			uint32 body2;
			uint32 count;
		};

		void SetBodyPairTracked(std::vector<CountedBodyPair> &bodyPairs, uint32 bodyID1, uint32 bodyID2, bool tracked)
		{
			for(CountedBodyPair &pair : bodyPairs)
			{
				if(pair.body1 == bodyID1 && pair.body2 == bodyID2)
				{
					if(tracked)
					{
						pair.count += 1;
						return;
					}

					if(pair.count > 0) pair.count -= 1;
					if(pair.count == 0)
					{
						pair = bodyPairs.back();
						bodyPairs.pop_back();
					}
					return;
				}
			}

			if(tracked)
			{
				bodyPairs.push_back({bodyID1, bodyID2, 1});
			}
		}

		bool GetBodyPair(const JPH::BodyID &body1, const JPH::BodyID &body2, uint32 &id1, uint32 &id2) const
		{
			if(body1.IsInvalid() || body2.IsInvalid()) return false;

			id1 = body1.GetIndexAndSequenceNumber();
			id2 = body2.GetIndexAndSequenceNumber();
			if(id1 == id2) return false;
			if(id1 > id2)
			{
				uint32 temp = id1;
				id1 = id2;
				id2 = temp;
			}

			return true;
		}

		void CollectConnectedFilteredBodies(uint32 bodyID, std::vector<JPH::BodyID> &bodies) const
		{
			std::vector<uint32> bodyIDs;
			CollectConnectedFilteredBodyIDs(bodyID, bodyIDs);
			for(uint32 connectedBodyID : bodyIDs)
			{
				bodies.push_back(JPH::BodyID(connectedBodyID));
			}
		}

		void CollectConnectedFilteredBodyIDs(uint32 bodyID, std::vector<uint32> &bodyIDs) const
		{
			for(uint32 currentBodyID : bodyIDs)
			{
				if(currentBodyID == bodyID) return;
			}

			bodyIDs.push_back(bodyID);
			if(_connectedFilteredBodyPairs.empty()) return;

			for(size_t i = 0; i < bodyIDs.size(); i += 1)
			{
				uint32 currentBodyID = bodyIDs[i];
				for(const CountedBodyPair &pair : _connectedFilteredBodyPairs)
				{
					uint32 connectedBodyID = 0;
					if(pair.body1 == currentBodyID)
					{
						connectedBodyID = pair.body2;
					}
					else if(pair.body2 == currentBodyID)
					{
						connectedBodyID = pair.body1;
					}
					else
					{
						continue;
					}

					bool hasBodyID = false;
					for(uint32 collectedBodyID : bodyIDs)
					{
						if(collectedBodyID == connectedBodyID)
						{
							hasBodyID = true;
							break;
						}
					}
					if(!hasBodyID)
					{
						bodyIDs.push_back(connectedBodyID);
					}
				}
			}
		}

		bool ShouldIgnoreBodyPair(const JPH::BodyID &body1, const JPH::BodyID &body2) const
		{
			if(_ignoredBodyPairs.empty() && _connectedFilteredBodyPairs.empty()) return false;

			uint32 bodyID1;
			uint32 bodyID2;
			if(!GetBodyPair(body1, body2, bodyID1, bodyID2)) return false;

			for(const CountedBodyPair &pair : _ignoredBodyPairs)
			{
				if(pair.body1 == bodyID1 && pair.body2 == bodyID2) return true;
			}

			if(_connectedFilteredBodyPairs.empty()) return false;

			std::vector<uint32> connectedBodyIDs;
			CollectConnectedFilteredBodyIDs(bodyID1, connectedBodyIDs);
			for(uint32 connectedBodyID : connectedBodyIDs)
			{
				if(connectedBodyID == bodyID2) return true;
			}

			return false;
		}

		bool HasConnectedFilteredBody(uint32 bodyID, uint32 connectedBodyID) const
		{
			if(bodyID == connectedBodyID) return true;
			if(_connectedFilteredBodyPairs.empty()) return false;

			std::vector<uint32> connectedBodyIDs;
			CollectConnectedFilteredBodyIDs(bodyID, connectedBodyIDs);
			for(uint32 currentConnectedBodyID : connectedBodyIDs)
			{
				if(currentConnectedBodyID == connectedBodyID) return true;
			}

			return false;
		}

		void ApplyContactResponseMassScale(const JoltCollisionObject *collisionObject, const JoltCollisionObject *otherCollisionObject, const JPH::BodyID &bodyID, JPH::ContactSettings &settings, bool isFirstBody) const
		{
			if(!collisionObject || !otherCollisionObject) return;

			float inverseMassScale = collisionObject->GetContactResponseInverseMassScaleFor(otherCollisionObject);
			float inverseInertiaScale = collisionObject->GetContactResponseInverseInertiaScaleFor(otherCollisionObject);
			uint32 supportBodyID = otherCollisionObject->GetContactResponseSupportBodyID();
			if(supportBodyID != 0xffffffff && !bodyID.IsInvalid() && HasConnectedFilteredBody(bodyID.GetIndexAndSequenceNumber(), supportBodyID))
			{
				inverseMassScale = 0.0f;
				inverseInertiaScale = 0.0f;
			}

			if(isFirstBody)
			{
				settings.mInvMassScale1 *= inverseMassScale;
				settings.mInvInertiaScale1 *= inverseInertiaScale;
			}
			else
			{
				settings.mInvMassScale2 *= inverseMassScale;
				settings.mInvInertiaScale2 *= inverseInertiaScale;
			}
		}

		std::vector<CountedBodyPair> _ignoredBodyPairs;
		std::vector<CountedBodyPair> _connectedFilteredBodyPairs;
	};

	// An example activation listener
	class JoltBodyActivationListener : public JPH::BodyActivationListener
	{
	public:
		virtual void OnBodyActivated(const JPH::BodyID &inBodyID, uint64 inBodyUserData) override
		{
			AutoreleasePool pool;
//			RNDebug("A body got activated");
		}

		virtual void OnBodyDeactivated(const JPH::BodyID &inBodyID, uint64 inBodyUserData) override
		{
			AutoreleasePool pool;
//			RNDebug("A body went to sleep");
		}
	};

	class JoltCharacterContactListener : public JPH::CharacterContactListener
	{
	public:
		/// Callback to adjust the velocity of a body as seen by the character. Can be adjusted to e.g. implement a conveyor belt or an inertial dampener system of a sci-fi space ship.
		/// Note that inBody2 is locked during the callback so you can read its properties freely.
		void OnAdjustBodyVelocity(const JPH::CharacterVirtual *inCharacter, const JPH::Body &inBody2, JPH::Vec3 &ioLinearVelocity, JPH::Vec3 &ioAngularVelocity) override;

		/// Checks if a character can collide with specified body. Return true if the contact is valid.
		bool OnContactValidate(const JPH::CharacterVirtual *inCharacter, const JPH::CharacterContact &inContact) override;

		/// Called whenever the character collides with a body. Returns true if the contact can push the character.
		/// @param inCharacter Character that is being solved
		/// @param inContact Contact that is being hit
		/// @param ioSettings Settings returned by the contact callback to indicate how the character should behave
		void OnContactAdded(const JPH::CharacterVirtual *inCharacter, const JPH::CharacterContact &inContact, JPH::CharacterContactSettings &ioSettings) override;

		/// Called whenever a contact is being used by the solver. Allows the listener to override the resulting character velocity (e.g. by preventing sliding along certain surfaces).
		/// @param inCharacter Character that is being solved
		/// @param inBodyID2 Body ID of body that is being hit
		/// @param inSubShapeID2 Sub shape ID of shape that is being hit
		/// @param inContactPosition World space contact position
		/// @param inContactNormal World space contact normal
		/// @param inContactVelocity World space velocity of contact point (e.g. for a moving platform)
		/// @param inContactMaterial Material of contact point
		/// @param inCharacterVelocity World space velocity of the character prior to hitting this contact
		/// @param ioNewCharacterVelocity Contains the calculated world space velocity of the character after hitting this contact, this velocity slides along the surface of the contact. Can be modified by the listener to provide an alternative velocity.
		void OnContactSolve(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2, const JPH::SubShapeID &inSubShapeID2, JPH::RVec3Arg inContactPosition, JPH::Vec3Arg inContactNormal, JPH::Vec3Arg inContactVelocity, const JPH::PhysicsMaterial *inContactMaterial, JPH::Vec3Arg inCharacterVelocity, JPH::Vec3 &ioNewCharacterVelocity) override;

		JoltKinematicController *controller;
	};

	struct JoltInternals
	{
		JPH::TempAllocatorImpl *tempAllocator;
		JPH::JobSystemThreadPool *jobSystem;

		JoltObjectLayerMapper objectLayerMapper;

		JoltContactListener contactListener;

		std::vector<JPH::BodyID> bodiesToAddLoadingLevel;
	};

	struct JoltCharacterInternals
	{
		JoltCharacterContactListener contactListener;
	};
} // namespace RN

#endif /* defined(__RAYNE_JOLTINTERNALS_H_) */
