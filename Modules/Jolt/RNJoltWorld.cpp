//
//  RNJoltWorld.cpp
//  Rayne-Jolt
//
//  Copyright 2023 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNJoltWorld.h"
#include "RNJoltConversions.h"
#include "RNJoltCustomPlanetTerrainShape.h"
#include "RNJoltInternals.h"
#include "RNJoltWheelCylinderShape.h"

#if RN_PLATFORM_LINUX || RN_PLATFORM_ANDROID
	#include <sys/resource.h>
#endif

#if RN_PLATFORM_MAC_OS || RN_PLATFORM_IOS || RN_PLATFORM_VISIONOS
	#include <pthread/qos.h>
#endif

namespace RN
{
	RNDefineMeta(JoltWorld, SceneAttachment)

	JoltWorld *JoltWorld::_sharedInstance = nullptr;

	void JoltWorld::InitializeWorkerThread(int)
	{
#if RN_PLATFORM_LINUX || RN_PLATFORM_ANDROID
		setpriority(PRIO_PROCESS, 0, -2);
#elif RN_PLATFORM_MAC_OS || RN_PLATFORM_IOS || RN_PLATFORM_VISIONOS
		pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
#elif RN_PLATFORM_WINDOWS
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#endif
	}

	JoltWorld::JoltWorld(const Vector3 &gravity, uint32 maxBodies, uint32 maxBodyPairs, uint32 maxContactConstraints) :
		_defaultDynamicBodyLinearDamping(0.05f), _defaultDynamicBodyAngularDamping(0.05f), _defaultDynamicBodyMaxLinearVelocity(500.0f), _defaultDynamicBodyMaxAngularVelocity(0.25f * k::Pi * 60.0f), _universePosition(0.0), _universeRotation(), _inverseUniverseRotation(), _universePositionRotation(), _inverseUniversePositionRotation(), _substeps(1), _paused(false), _isSimulating(false), _isLoadingLevel(false)
	{
		RN_ASSERT(!_sharedInstance, "There can only be one Jolt instance at a time!");
		_sharedInstance = this;

		// Register allocation hook
		JPH::RegisterDefaultAllocator();

		// Create a factory
		JPH::Factory::sInstance = new JPH::Factory();

		// Register all Jolt physics types
		JPH::RegisterTypes();
		JoltCustomPlanetTerrainShape::RegisterJoltShape();
		JPH::RNWheelCylinderShape::sRegister();

		_internals->tempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024); //Preallocate 10mb for temp allocations during physics update

		// We need a job system that will execute physics jobs on multiple threads. Typically
		// you would implement the JobSystem interface yourself and let Jolt Physics run on top
		// of your own job scheduler. JobSystemThreadPool is an example implementation.
		_internals->jobSystem = new JPH::JobSystemThreadPool();
		_internals->jobSystem->SetThreadInitFunction(InitializeWorkerThread);
		_internals->jobSystem->Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

		_physicsSystem = new JPH::PhysicsSystem();
		_physicsSystem->Init(maxBodies, 0, maxBodyPairs, maxContactConstraints, _internals->objectLayerMapper, _internals->objectLayerMapper, _internals->objectLayerMapper);

		_physicsSystem->SetContactListener(&_internals->contactListener);

		SetGravity(gravity);
	}

	JoltWorld::~JoltWorld()
	{
		delete _physicsSystem;

		delete _internals->tempAllocator;
		delete _internals->jobSystem;

		// Unregisters all types with the factory and cleans up the default material
		JPH::UnregisterTypes();

		// Destroy the factory
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;

		_sharedInstance = nullptr;
	}

	void JoltWorld::SetGravity(const Vector3 &gravity)
	{
		_physicsSystem->SetGravity(JoltConversions::ToJoltVector(gravity));
	}

	Vector3 JoltWorld::GetGravity()
	{
		JPH::Vec3 gravity = _physicsSystem->GetGravity();
		return JoltConversions::ToEngineVector(gravity);
	}

	void JoltWorld::SetUniverseTransform(const DVector3 &position, const Quaternion &rotation)
	{
		if(!position.IsValid() || !rotation.IsValid()) return;

		Quaternion normalizedRotation(rotation);
		normalizedRotation.Normalize();
		if(!normalizedRotation.IsValid()) return;

		if(_universePosition == position && _universeRotation == normalizedRotation && _universePositionRotation == rotation) return;

		_universePosition = position;
		_universeRotation = normalizedRotation;
		_inverseUniverseRotation = _universeRotation.GetConjugated();
		_universePositionRotation = rotation;
		_inverseUniversePositionRotation = _universePositionRotation.GetConjugated();

		UpdateDynamicBodyPositions();
	}

	void JoltWorld::SetUniversePosition(const DVector3 &position)
	{
		SetUniverseTransform(position, _universeRotation);
	}

	void JoltWorld::SetUniverseRotation(const Quaternion &rotation)
	{
		SetUniverseTransform(_universePosition, rotation);
	}

	void JoltWorld::UpdateDynamicBodyPositions()
	{
		SceneQuadtree *quadtree = nullptr;
		if(GetParent() && GetParent()->IsKindOfClass(SceneQuadtree::GetMetaClass()))
		{
			quadtree = static_cast<SceneQuadtree *>(GetParent());
		}

		JPH::BodyIDVector bodyIDs;
		_physicsSystem->GetBodies(bodyIDs);

		JPH::BodyInterface &bodyInterface = _physicsSystem->GetBodyInterface();
		for(JPH::BodyID bodyID : bodyIDs)
		{
			uint64 userData = bodyInterface.GetUserData(bodyID);
			JoltCollisionObject *collisionObject = reinterpret_cast<JoltCollisionObject *>(userData);
			JoltDynamicBody *body = collisionObject ? collisionObject->Downcast<JoltDynamicBody>() : nullptr;
			if(!body) continue;

			body->UpdatePosition();
			if(quadtree && body->GetParent())
			{
				quadtree->RelocateNodeIfNeeded(body->GetParent());
			}
		}
	}

	JoltPosition JoltWorld::ConvertPositionToPhysicsWorld(const JoltPosition &position) const
	{
#if RN_ENABLE_UNIVERSE_SCALE
		DVector3 offset = position - _universePosition;
#else
		DVector3 offset(position);
		offset -= _universePosition;
#endif
		DVector3 physicsPosition = _inverseUniversePositionRotation.GetRotatedVector(offset);
#if RN_ENABLE_UNIVERSE_SCALE
		return physicsPosition;
#else
		return physicsPosition.ToVector3();
#endif
	}

	JoltPosition JoltWorld::ConvertPositionFromPhysicsWorld(const JoltPosition &position) const
	{
		DVector3 sceneOffset = _universePositionRotation.GetRotatedVector(DVector3(position));
#if RN_ENABLE_UNIVERSE_SCALE
		return _universePosition + sceneOffset;
#else
		return (_universePosition + sceneOffset).ToVector3();
#endif
	}

	Vector3 JoltWorld::ConvertVectorToPhysicsWorld(const Vector3 &vector) const
	{
		return _inverseUniverseRotation.GetRotatedVector(vector);
	}

	Vector3 JoltWorld::ConvertVectorFromPhysicsWorld(const Vector3 &vector) const
	{
		return _universeRotation.GetRotatedVector(vector);
	}

	Quaternion JoltWorld::ConvertRotationToPhysicsWorld(const Quaternion &rotation) const
	{
		Quaternion result = _inverseUniverseRotation * rotation;
		result.Normalize();
		return result;
	}

	Quaternion JoltWorld::ConvertRotationFromPhysicsWorld(const Quaternion &rotation) const
	{
		Quaternion result = _universeRotation * rotation;
		result.Normalize();
		return result;
	}

	void JoltWorld::SetDefaultDynamicBodyDamping(float linear, float angular)
	{
		if(linear < 0.0f) linear = 0.0f;
		if(angular < 0.0f) angular = 0.0f;
		_defaultDynamicBodyLinearDamping = linear;
		_defaultDynamicBodyAngularDamping = angular;
	}

	void JoltWorld::SetDefaultDynamicBodyMaxVelocity(float linear, float angular)
	{
		if(linear < 0.0f) linear = 0.0f;
		if(angular < 0.0f) angular = 0.0f;
		_defaultDynamicBodyMaxLinearVelocity = linear;
		_defaultDynamicBodyMaxAngularVelocity = angular;
	}

	void JoltWorld::SetSubsteps(uint8 substeps)
	{
		_substeps = substeps;
	}

	void JoltWorld::SetSolverIterationCount(uint32 positionIterations, uint32 velocityIterations)
	{
		JPH::PhysicsSettings settings = _physicsSystem->GetPhysicsSettings();
		if(positionIterations > 0) settings.mNumPositionSteps = positionIterations;
		if(velocityIterations > 0) settings.mNumVelocitySteps = velocityIterations;
		_physicsSystem->SetPhysicsSettings(settings);
	}

	void JoltWorld::SetPenetrationSlop(float penetrationSlop)
	{
		JPH::PhysicsSettings settings = _physicsSystem->GetPhysicsSettings();
		if(penetrationSlop < 0.0f) penetrationSlop = 0.0f;
		settings.mPenetrationSlop = penetrationSlop;
		_physicsSystem->SetPhysicsSettings(settings);
	}

	void JoltWorld::SetContactCorrection(float baumgarte, float maxPenetrationDistance)
	{
		JPH::PhysicsSettings settings = _physicsSystem->GetPhysicsSettings();
		if(baumgarte < 0.0f) baumgarte = 0.0f;
		if(maxPenetrationDistance < 0.0f) maxPenetrationDistance = 0.0f;
		settings.mBaumgarte = baumgarte;
		settings.mMaxPenetrationDistance = maxPenetrationDistance;
		_physicsSystem->SetPhysicsSettings(settings);
	}

	void JoltWorld::SetPaused(bool paused)
	{
		_paused = paused;
	}

	void JoltWorld::Update(float delta)
	{
		SceneAttachment::Update(delta);

		if(_paused)
			return;
		
		if(delta > 0.1f || delta < k::EpsilonFloat)
			return;
		
		// Step physics
		_isSimulating = true;
		_physicsSystem->Update(delta, _substeps, _internals->tempAllocator, _internals->jobSystem); //This waits for all jobs to finish! Maybe it can be split up into simulate and finish like with physx (wait here for all jobs, but start them in WillUpdate)
		_isSimulating = false;

		JPH::BodyIDVector bodyIDs;
		_physicsSystem->GetActiveBodies(JPH::EBodyType::RigidBody, bodyIDs);
		if(GetParent() && GetParent()->IsKindOfClass(SceneQuadtree::GetMetaClass()))
		{
			SceneQuadtree *quadtree = static_cast<SceneQuadtree *>(GetParent());
			for(JPH::BodyID bodyID : bodyIDs)
			{
				uint64 userData = _physicsSystem->GetBodyInterface().GetUserData(bodyID);
				JoltCollisionObject *collisionObject = reinterpret_cast<JoltCollisionObject *>(userData);
				if(collisionObject)
				{
					collisionObject->UpdatePosition();
					quadtree->RelocateNodeIfNeeded(collisionObject->GetParent());
				}
			}
		}
		else
		{
			for(JPH::BodyID bodyID : bodyIDs)
			{
				uint64 userData = _physicsSystem->GetBodyInterface().GetUserData(bodyID);
				JoltCollisionObject *collisionObject = reinterpret_cast<JoltCollisionObject *>(userData);
				if(collisionObject) collisionObject->UpdatePosition();
			}
		}

		ProcessQueuedBodyRemovals();
	}

	void JoltWorld::EnumerateActiveCollisionObjects(const std::function<void(JoltCollisionObject *)> &callback)
	{
		JPH::BodyIDVector bodyIDs;
		_physicsSystem->GetActiveBodies(JPH::EBodyType::RigidBody, bodyIDs);
		for(JPH::BodyID bodyID : bodyIDs)
		{
			uint64 userData = _physicsSystem->GetBodyInterface().GetUserData(bodyID);
			JoltCollisionObject *collisionObject = reinterpret_cast<JoltCollisionObject *>(userData);
			if(collisionObject) callback(collisionObject);
		}
	}

	void JoltWorld::WillUpdate(float delta)
	{
		SceneAttachment::WillUpdate(delta);
	}

	void JoltWorld::OptimizeBroadPhase()
	{
		_physicsSystem->OptimizeBroadPhase();
	}


	void JoltWorld::PrepareLoadingLevel()
	{
		RN_DEBUG_ASSERT(!_isLoadingLevel, "Jolt is already setup for level loading! PrepareLoadingLevel should only be called once!");
		_isLoadingLevel = true;
	}

	void JoltWorld::FinalizeLoadingLevel()
	{
		RN_DEBUG_ASSERT(_isLoadingLevel, "PrepareLoadingLevel was not called or loading was already finalized!");
		_isLoadingLevel = false;

		if(_internals->bodiesToAddLoadingLevel.size() == 0) return;

		JPH::BodyInterface &bodyInterface = _physicsSystem->GetBodyInterface();
		JPH::BodyInterface::AddState state = bodyInterface.AddBodiesPrepare(_internals->bodiesToAddLoadingLevel.data(), _internals->bodiesToAddLoadingLevel.size());
		bodyInterface.AddBodiesFinalize(_internals->bodiesToAddLoadingLevel.data(), _internals->bodiesToAddLoadingLevel.size(), state, JPH::EActivation::DontActivate);

		_internals->bodiesToAddLoadingLevel.clear();
	}

	void JoltWorld::AddBodyForLoadingLevel(JPH::Body *body)
	{
		RN_DEBUG_ASSERT(_isLoadingLevel, "Not currently loading a level! This should only be called internally for bulk body creation!");
		_internals->bodiesToAddLoadingLevel.push_back(body->GetID());
	}


	uint16 JoltWorld::GetObjectLayer(uint32 collisionGroup, uint32 collisionMask, uint8 broadPhaseLayer)
	{
		return _internals->objectLayerMapper.GetObjectLayer(collisionGroup, collisionMask, broadPhaseLayer);
	}

	void JoltWorld::QueueBodyRemoval(const JPH::BodyID &bodyID)
	{
		if(bodyID.IsInvalid()) return;

		for(JoltInternals::PendingBodyRemoval &removal : _internals->pendingBodyRemovals)
		{
			if(removal.bodyID == bodyID) return;
		}

		JoltInternals::PendingBodyRemoval removal;
		removal.bodyID = bodyID;
		removal.delay = 1;
		_internals->pendingBodyRemovals.push_back(removal);
	}

	void JoltWorld::CancelQueuedBodyRemoval(const JPH::BodyID &bodyID)
	{
		for(auto iterator = _internals->pendingBodyRemovals.begin(); iterator != _internals->pendingBodyRemovals.end();)
		{
			if(iterator->bodyID == bodyID)
			{
				iterator = _internals->pendingBodyRemovals.erase(iterator);
			}
			else
			{
				++iterator;
			}
		}
	}

	void JoltWorld::ProcessQueuedBodyRemovals()
	{
		if(_internals->pendingBodyRemovals.empty()) return;

		JPH::BodyInterface &bodyInterface = _physicsSystem->GetBodyInterface();
		for(auto iterator = _internals->pendingBodyRemovals.begin(); iterator != _internals->pendingBodyRemovals.end();)
		{
			if(iterator->delay > 0)
			{
				iterator->delay -= 1;
				++iterator;
				continue;
			}

			if(bodyInterface.IsAdded(iterator->bodyID))
			{
				bodyInterface.DeactivateBody(iterator->bodyID);
				bodyInterface.RemoveBody(iterator->bodyID);
			}
			iterator = _internals->pendingBodyRemovals.erase(iterator);
		}
	}

	void JoltWorld::SetBodyPairCollisionEnabled(const JPH::BodyID &body1, const JPH::BodyID &body2, bool enabled)
	{
		if(body1.IsInvalid() || body2.IsInvalid() || body1 == body2) return;

		_internals->contactListener.SetBodyPairCollisionEnabled(body1, body2, enabled);

		JPH::BodyInterface &bodyInterface = _physicsSystem->GetBodyInterface();
		bodyInterface.InvalidateContactCache(body1);
		bodyInterface.InvalidateContactCache(body2);
	}

	void JoltWorld::SetConnectedBodyCollisionFilteringEnabled(const JPH::BodyID &body1, const JPH::BodyID &body2, bool enabled)
	{
		if(body1.IsInvalid() || body2.IsInvalid() || body1 == body2) return;

		std::vector<JPH::BodyID> affectedBodies;
		_internals->contactListener.SetConnectedBodyCollisionFilteringEnabled(body1, body2, enabled, affectedBodies);

		JPH::BodyInterface &bodyInterface = _physicsSystem->GetBodyInterface();
		for(const JPH::BodyID &bodyID : affectedBodies)
		{
			if(bodyID.IsInvalid()) continue;
			bodyInterface.InvalidateContactCache(bodyID);
		}
	}


	JoltContactInfo JoltWorld::CastRay(const JoltPosition &globalFrom, const JoltPosition &globalTo, uint32 filterGroup, uint32 filterMask)
	{
		JoltContactInfo hit;
		hit.distance = -1.0f;
		hit.node = nullptr;
		hit.collisionObject = nullptr;

		Vector3 diff = JoltConversions::ToVector3(globalTo - globalFrom);

		//TODO: Limit max distance of raycast or the result

		JPH::RRayCast rayInfo;
		rayInfo.mOrigin = JoltConversions::ToJoltPosition(globalFrom);
		rayInfo.mDirection = JoltConversions::ToJoltVector(diff);

		JPH::RayCastResult result;
		uint16 objectLayer = GetObjectLayer(filterGroup, filterMask, 1);
		if(!_physicsSystem->GetNarrowPhaseQuery().CastRay(rayInfo, result, _physicsSystem->GetDefaultBroadPhaseLayerFilter(objectLayer), _physicsSystem->GetDefaultLayerFilter(objectLayer)))
		{
			return hit;
		}

		JPH::RVec3 position = rayInfo.GetPointOnRay(result.mFraction);
		JPH::Vec3 normal;

		JPH::BodyInterface &bodyInterface = _physicsSystem->GetBodyInterface();
		{
			JPH::TransformedShape transformedShape = bodyInterface.GetTransformedShape(result.mBodyID);
			if(!transformedShape.mShape) return hit;

			normal = transformedShape.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, position);
			hit.collisionObject = reinterpret_cast<JoltCollisionObject *>(bodyInterface.GetUserData(result.mBodyID));
			JoltContactInfoShapeData::FillForTransformedShape(hit, transformedShape, result.mSubShapeID2);
		}

		hit.position = JoltConversions::ToPosition(position);
		hit.normal = JoltConversions::ToEngineVector(normal);

		hit.distance = static_cast<float>(globalFrom.GetDistance(hit.position));

		if(hit.collisionObject) hit.node = hit.collisionObject->GetParent();
		if(hit.node) hit.node->Retain()->Autorelease();

		return hit;
	}

	JoltContactInfo JoltWorld::CastSweep(JoltShape *shape, const Quaternion &rotation, const JoltPosition &globalFrom, const JoltPosition &globalTo, const Vector3 &scale, uint32 filterGroup, uint32 filterMask)
	{
		JoltContactInfo hit;
		hit.distance = -1.0f;
		hit.node = nullptr;
		hit.collisionObject = nullptr;

		Vector3 diff = JoltConversions::ToVector3(globalTo - globalFrom);

		JPH::RVec3 baseOffset = JoltConversions::ToJoltPosition(globalFrom);
		JPH::RMat44 worldTransform = JoltConversions::ToJoltRMat44(JoltConversions::ToJoltRotation(rotation), baseOffset);

		//TODO: Limit max distance of raycast or the result

		JPH::RShapeCast castInfo = JPH::RShapeCast::sFromWorldTransform(shape->GetJoltShape(), JoltConversions::ToJoltVec3(scale), worldTransform, JoltConversions::ToJoltVector(diff));

		JPH::ShapeCastSettings castSettings; //Defaults seem ok for now!?

		uint16 objectLayer = GetObjectLayer(filterGroup, filterMask, 1);
		JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> result;
		_physicsSystem->GetNarrowPhaseQuery().CastShape(castInfo, castSettings, baseOffset, result, _physicsSystem->GetDefaultBroadPhaseLayerFilter(objectLayer), _physicsSystem->GetDefaultLayerFilter(objectLayer));
		if(!result.HadHit())
		{
			return hit;
		}

		JPH::RVec3 position = baseOffset + result.mHit.mContactPointOn2; //castInfo.GetPointOnRay(result.mHit.mFraction);
		JPH::Vec3 normal;

		JPH::BodyInterface &bodyInterface = _physicsSystem->GetBodyInterface();
		{
			JPH::TransformedShape transformedShape = bodyInterface.GetTransformedShape(result.mHit.mBodyID2);
			if(!transformedShape.mShape) return hit;

			normal = transformedShape.GetWorldSpaceSurfaceNormal(result.mHit.mSubShapeID2, position);
			hit.collisionObject = reinterpret_cast<JoltCollisionObject *>(bodyInterface.GetUserData(result.mHit.mBodyID2));
			JoltContactInfoShapeData::FillForTransformedShape(hit, transformedShape, result.mHit.mSubShapeID2);
		}

		hit.position = JoltConversions::ToPosition(position);
		hit.normal = JoltConversions::ToEngineVector(normal);

		hit.distance = JoltConversions::ToVector3(result.mHit.mContactPointOn2).GetLength();

		if(hit.collisionObject) hit.node = hit.collisionObject->GetParent();
		if(hit.node) hit.node->Retain()->Autorelease();

		return hit;
	}

	std::vector<JoltContactInfo> JoltWorld::CheckOverlap(JoltShape *shape, const JoltPosition &globalPosition, const Quaternion &rotation, const Vector3 &scale, uint32 filterGroup, uint32 filterMask)
	{
		std::vector<JoltContactInfo> hits;

		JPH::RVec3 baseOffset = JoltConversions::ToJoltPosition(globalPosition);
		JPH::RMat44 worldTransform = JoltConversions::ToJoltRMat44(JoltConversions::ToJoltRotation(rotation), baseOffset);
		JPH::CollideShapeSettings collideSettings; //Defaults seem ok for now!?

		JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> results;
		uint16 objectLayer = GetObjectLayer(filterGroup, filterMask, 1);
		_physicsSystem->GetNarrowPhaseQuery().CollideShape(shape->GetJoltShape(), JoltConversions::ToJoltVec3(scale), worldTransform.PreTranslated(shape->GetJoltShape()->GetCenterOfMass()), collideSettings, baseOffset, results, _physicsSystem->GetDefaultBroadPhaseLayerFilter(objectLayer), _physicsSystem->GetDefaultLayerFilter(objectLayer));

		JPH::BodyInterface &bodyInterface = _physicsSystem->GetBodyInterface();
		for(auto result : results.mHits)
		{
			JoltContactInfo hit;
			hit.distance = 0.0f;
			hit.position = globalPosition;
			hit.node = nullptr;
			hit.collisionObject = nullptr;

			{
				JPH::TransformedShape transformedShape = bodyInterface.GetTransformedShape(result.mBodyID2);
				if(!transformedShape.mShape) continue;

				hit.collisionObject = reinterpret_cast<JoltCollisionObject *>(bodyInterface.GetUserData(result.mBodyID2));
				JoltContactInfoShapeData::FillForTransformedShape(hit, transformedShape, result.mSubShapeID2);
			}
			if(hit.collisionObject) hit.node = hit.collisionObject->GetParent();
			if(hit.node) hit.node->Retain()->Autorelease();

			hits.push_back(hit);
		}

		return hits;
	}
} // namespace RN
