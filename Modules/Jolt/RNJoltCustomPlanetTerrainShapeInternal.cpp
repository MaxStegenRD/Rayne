//
//  RNJoltCustomPlanetTerrainShapeInternal.cpp
//  Rayne-Jolt
//
//  Copyright 2026 by Überpixel. All rights reserved.
//

#include "RNJoltCustomPlanetTerrainShapeInternal.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollideConvexVsTriangles.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif

JPH_NAMESPACE_BEGIN

class RNCustomPlanetTerrainShape final : public Shape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	struct Triangle
	{
		Vec3 vertices[3];
		uint8 activeEdges;
	};

	RNCustomPlanetTerrainShape() :
		Shape(EShapeType::User1, EShapeSubType::User1),
		_provider(nullptr)
	{}

	RNCustomPlanetTerrainShape(RN::JoltCustomPlanetTerrainInternalProvider *provider) :
		Shape(EShapeType::User1, EShapeSubType::User1),
		_provider(provider)
	{
		if(_provider) _provider->RetainProvider();
	}

	~RNCustomPlanetTerrainShape() override
	{
		if(_provider) _provider->ReleaseProvider();
	}

	bool MustBeStatic() const override
	{
		return true;
	}

	AABox GetLocalBounds() const override
	{
		const float radius = GetMaximumRadius();
		return AABox(Vec3::sReplicate(-radius), Vec3::sReplicate(radius));
	}

	uint GetSubShapeIDBitsRecursive() const override
	{
		return 16;
	}

	float GetInnerRadius() const override
	{
		return _provider ? _provider->GetMinimumPlanetTerrainRadius() : 0.0f;
	}

	MassProperties GetMassProperties() const override
	{
		return MassProperties();
	}

	const PhysicsMaterial *GetMaterial([[maybe_unused]] const SubShapeID &subShapeID) const override
	{
		return PhysicsMaterial::sDefault;
	}

	Vec3 GetSurfaceNormal([[maybe_unused]] const SubShapeID &subShapeID, Vec3Arg localSurfacePosition) const override
	{
		Vec3 direction = localSurfacePosition.NormalizedOr(Vec3::sAxisY());
		Vec3 position;
		Vec3 normal;
		float radius = 0.0f;
		if(Sample(direction, position, normal, radius)) return normal;
		return direction;
	}

	void GetSubmergedVolume([[maybe_unused]] Mat44Arg centerOfMassTransform, [[maybe_unused]] Vec3Arg scale, [[maybe_unused]] const Plane &surface, float &totalVolume, float &submergedVolume, Vec3 &centerOfBuoyancy
#ifdef JPH_DEBUG_RENDERER
		, [[maybe_unused]] RVec3Arg baseOffset
#endif
		) const override
	{
		totalVolume = 0.0f;
		submergedVolume = 0.0f;
		centerOfBuoyancy = Vec3::sZero();
	}

#ifdef JPH_DEBUG_RENDERER
	void Draw([[maybe_unused]] DebugRenderer *renderer, [[maybe_unused]] RMat44Arg centerOfMassTransform, [[maybe_unused]] Vec3Arg scale, [[maybe_unused]] ColorArg color, [[maybe_unused]] bool useMaterialColors, [[maybe_unused]] bool drawWireframe) const override
	{}
#endif

	bool CastRay(const RayCast &ray, const SubShapeIDCreator &subShapeIDCreator, RayCastResult &hit) const override
	{
		float fraction = 0.0f;
		if(!FindRayHit(ray, hit.mFraction, fraction)) return false;

		hit.mFraction = fraction;
		hit.mSubShapeID2 = subShapeIDCreator.GetID();
		return true;
	}

	void CastRay(const RayCast &ray, [[maybe_unused]] const RayCastSettings &rayCastSettings, const SubShapeIDCreator &subShapeIDCreator, CastRayCollector &collector, const ShapeFilter &shapeFilter = { }) const override
	{
		if(!shapeFilter.ShouldCollide(this, subShapeIDCreator.GetID())) return;

		RayCastResult hit;
		hit.mFraction = collector.GetEarlyOutFraction();
		if(CastRay(ray, subShapeIDCreator, hit))
		{
			collector.AddHit(hit);
		}
	}

	void CollidePoint(Vec3Arg point, const SubShapeIDCreator &subShapeIDCreator, CollidePointCollector &collector, const ShapeFilter &shapeFilter = { }) const override
	{
		if(!shapeFilter.ShouldCollide(this, subShapeIDCreator.GetID())) return;
		if(GetSignedDistance(point) > 0.0f) return;
		collector.AddHit({ TransformedShape::sGetBodyID(collector.GetContext()), subShapeIDCreator.GetID() });
	}

	void CollideSoftBodyVertices([[maybe_unused]] Mat44Arg centerOfMassTransform, [[maybe_unused]] Vec3Arg scale, [[maybe_unused]] const CollideSoftBodyVertexIterator &vertices, [[maybe_unused]] uint numVertices, [[maybe_unused]] int collidingShapeIndex) const override
	{}

	void GetTrianglesStart(GetTrianglesContext &context, const AABox &box, [[maybe_unused]] Vec3Arg positionCOM, [[maybe_unused]] QuatArg rotation, [[maybe_unused]] Vec3Arg scale) const override
	{
		TriangleContext *triangleContext = reinterpret_cast<TriangleContext *>(&context);
		triangleContext->count = 0;
		triangleContext->index = 0;

		Array<Triangle> triangles;
		CollectTriangles(box, 0.0f, triangles, MaxContextTriangles);
		triangleContext->count = triangles.size();
		for(uint i = 0; i < triangleContext->count; i += 1)
		{
			triangleContext->triangles[i] = triangles[i];
		}
	}

	int GetTrianglesNext(GetTrianglesContext &context, int maxTrianglesRequested, Float3 *triangleVertices, [[maybe_unused]] const PhysicsMaterial **materials = nullptr) const override
	{
		TriangleContext *triangleContext = reinterpret_cast<TriangleContext *>(&context);
		int writtenCount = 0;
		while(writtenCount < maxTrianglesRequested && triangleContext->index < triangleContext->count)
		{
			const Triangle &triangle = triangleContext->triangles[triangleContext->index];
			for(uint vertex = 0; vertex < 3; vertex += 1)
			{
				const Vec3 &position = triangle.vertices[vertex];
				triangleVertices[writtenCount * 3 + vertex] = Float3(position.GetX(), position.GetY(), position.GetZ());
			}
			if(materials) materials[writtenCount] = PhysicsMaterial::sDefault;
			triangleContext->index += 1;
			writtenCount += 1;
		}
		return writtenCount;
	}

	Stats GetStats() const override
	{
		return Stats(sizeof(*this), 0);
	}

	float GetVolume() const override
	{
		return 0.0f;
	}

	bool IsValidScale(Vec3Arg scale) const override
	{
		return scale.GetX() != 0.0f && scale.GetY() != 0.0f && scale.GetZ() != 0.0f;
	}

	Vec3 MakeScaleValid(Vec3Arg scale) const override
	{
		return ScaleHelpers::MakeNonZeroScale(scale);
	}

	template<class Visitor>
	void CollideTriangles(const AABox &box, float maxSeparationDistance, const SubShapeIDCreator &subShapeIDCreator, Visitor &visitor, Vec3Arg localBase) const
	{
		Array<Triangle> triangles;
		CollectTriangles(box, maxSeparationDistance, localBase, triangles, MaxCollisionTriangles);
		for(uint i = 0; i < triangles.size() && !visitor.ShouldAbort(); i += 1)
		{
			const Triangle &triangle = triangles[i];
			visitor.Collide(triangle.vertices[0], triangle.vertices[1], triangle.vertices[2], triangle.activeEdges, subShapeIDCreator.PushID(i & 0xffffu, 16).GetID());
		}
	}

	void CollideSolidVolume(const ConvexShape *shape, Vec3Arg scale, Mat44Arg shapeTransform, Mat44Arg planetTransform, Vec3Arg localBase, const SubShapeIDCreator &shapeSubShapeIDCreator, const SubShapeIDCreator &planetSubShapeIDCreator, const CollideShapeSettings &settings, CollideShapeCollector &collector) const
	{
		if(!_provider) return;

		Vec3 contactPointOnTerrain;
		Vec3 surfaceNormal;
		Vec3 supportInPlanet;
		float penetration = 0.0f;
		if(!SampleConvexSupport(shape, scale, planetTransform.InversedRotationTranslation() * shapeTransform, localBase, contactPointOnTerrain, surfaceNormal, supportInPlanet, penetration)) return;
		if(penetration <= -settings.mMaxSeparationDistance) return;

		const Vec3 supportWorld = planetTransform * supportInPlanet;
		const Vec3 terrainWorld = planetTransform * contactPointOnTerrain;
		const Vec3 contactNormalWorld = planetTransform.Multiply3x3(surfaceNormal).NormalizedOr(Vec3::sAxisY());
		CollideShapeResult result(supportWorld,
								  terrainWorld,
								  -contactNormalWorld,
								  penetration,
								  shapeSubShapeIDCreator.GetID(),
								  planetSubShapeIDCreator.GetID(),
								  TransformedShape::sGetBodyID(collector.GetContext()));
		collector.AddHit(result);
	}

	static void sRegister()
	{
		ShapeFunctions &functions = ShapeFunctions::sGet(EShapeSubType::User1);
		functions.mConstruct = []() -> Shape * { return new RNCustomPlanetTerrainShape; };
		functions.mColor = Color::sDarkGreen;

		for(EShapeSubType subtype : sConvexSubShapeTypes)
		{
			CollisionDispatch::sRegisterCollideShape(subtype, EShapeSubType::User1, sCollideConvexVsPlanetTerrain);
			CollisionDispatch::sRegisterCollideShape(EShapeSubType::User1, subtype, CollisionDispatch::sReversedCollideShape);
			CollisionDispatch::sRegisterCastShape(subtype, EShapeSubType::User1, sCastConvexVsPlanetTerrain);
			CollisionDispatch::sRegisterCastShape(EShapeSubType::User1, subtype, CollisionDispatch::sReversedCastShape);
		}
	}

private:
	static constexpr uint MaxCollisionTriangles = 2048;
	static constexpr uint MaxContextTriangles = 64;

	static Mat44 GetShiftedTransform(Mat44Arg transform, Vec3Arg worldBase)
	{
		Mat44 shiftedTransform = transform;
		shiftedTransform.SetTranslation(transform.GetTranslation() - worldBase);
		return shiftedTransform;
	}

	static Mat44 GetShiftedReferenceTransform(Mat44Arg transform, Vec3Arg localBase, Vec3Arg worldBase)
	{
		Mat44 shiftedTransform = transform;
		shiftedTransform.SetTranslation(transform.GetTranslation() - worldBase + transform.Multiply3x3(localBase));
		return shiftedTransform;
	}

	static Vec3 GetOffsetPosition(double x, double y, double z, Vec3Arg localBase)
	{
		return Vec3(static_cast<float>(x - static_cast<double>(localBase.GetX())),
					static_cast<float>(y - static_cast<double>(localBase.GetY())),
					static_cast<float>(z - static_cast<double>(localBase.GetZ())));
	}

	static void TranslateFace(CollideShapeResult::Face &face, Vec3Arg offset)
	{
		for(CollideShapeResult::Face::size_type i = 0; i < face.size(); i += 1)
		{
			face[i] += offset;
		}
	}

	static void TranslateResult(CollideShapeResult &result, Vec3Arg offset)
	{
		result.mContactPointOn1 += offset;
		result.mContactPointOn2 += offset;
		TranslateFace(result.mShape1Face, offset);
		TranslateFace(result.mShape2Face, offset);
	}

	class OffsetCollideShapeCollector final : public CollideShapeCollector
	{
	public:
		OffsetCollideShapeCollector(CollideShapeCollector &collector, Vec3Arg offset) :
			CollideShapeCollector(collector),
			_collector(collector),
			_offset(offset),
			_hitCount(0)
		{}

		void AddHit(const CollideShapeResult &result) override
		{
			_hitCount += 1;
			CollideShapeResult offsetResult = result;
			RNCustomPlanetTerrainShape::TranslateResult(offsetResult, _offset);
			_collector.AddHit(offsetResult);
			UpdateEarlyOutFraction(_collector.GetEarlyOutFraction());
		}

		uint GetHitCount() const
		{
			return _hitCount;
		}

	private:
		CollideShapeCollector &_collector;
		Vec3 _offset;
		uint _hitCount;
	};

	class OffsetCastShapeCollector final : public CastShapeCollector
	{
	public:
		OffsetCastShapeCollector(CastShapeCollector &collector, Vec3Arg offset) :
			CastShapeCollector(collector),
			_collector(collector),
			_offset(offset)
		{}

		void AddHit(const ShapeCastResult &result) override
		{
			ShapeCastResult offsetResult = result;
			RNCustomPlanetTerrainShape::TranslateResult(offsetResult, _offset);
			_collector.AddHit(offsetResult);
			UpdateEarlyOutFraction(_collector.GetEarlyOutFraction());
		}

	private:
		CastShapeCollector &_collector;
		Vec3 _offset;
	};

	struct TriangleContext
	{
		Triangle triangles[MaxContextTriangles];
		uint count;
		uint index;
	};

	static_assert(sizeof(TriangleContext) <= sizeof(GetTrianglesContext), "Triangle context is too large.");

	static void sCollideConvexVsPlanetTerrain(const Shape *shape1, const Shape *shape2, Vec3Arg scale1, Vec3Arg scale2, Mat44Arg transform1, Mat44Arg transform2, const SubShapeIDCreator &subShapeIDCreator1, const SubShapeIDCreator &subShapeIDCreator2, const CollideShapeSettings &settings, CollideShapeCollector &collector, [[maybe_unused]] const ShapeFilter &shapeFilter)
	{
		JPH_ASSERT(shape1->GetType() == EShapeType::Convex);
		JPH_ASSERT(shape2->GetSubType() == EShapeSubType::User1);
		const ConvexShape *convex = static_cast<const ConvexShape *>(shape1);
		const RNCustomPlanetTerrainShape *planet = static_cast<const RNCustomPlanetTerrainShape *>(shape2);

		struct Visitor : public CollideConvexVsTriangles
		{
			using CollideConvexVsTriangles::CollideConvexVsTriangles;

			bool ShouldAbort() const
			{
				return mCollector.ShouldEarlyOut();
			}

			AABox GetQueryBounds() const
			{
				return mBoundsOf1InSpaceOf2;
			}
		};

		const Vec3 worldBase = transform1.GetTranslation();
		const Vec3 localBase = transform2.InversedRotationTranslation() * worldBase;
		const Mat44 shiftedTransform1 = GetShiftedTransform(transform1, worldBase);
		const Mat44 shiftedTransform2 = GetShiftedReferenceTransform(transform2, localBase, worldBase);

		CollideShapeSettings triangleSettings = settings;
		triangleSettings.mActiveEdgeMode = EActiveEdgeMode::CollideOnlyWithActive;
		triangleSettings.mCollectFacesMode = ECollectFacesMode::CollectFaces;

		OffsetCollideShapeCollector triangleCollector(collector, worldBase);
		Visitor visitor(convex, scale1, scale2, shiftedTransform1, shiftedTransform2, subShapeIDCreator1.GetID(), triangleSettings, triangleCollector);
		planet->CollideTriangles(visitor.GetQueryBounds(), triangleSettings.mMaxSeparationDistance, subShapeIDCreator2, visitor, localBase);
		if(triangleCollector.GetHitCount() != 0) return;

		OffsetCollideShapeCollector solidCollector(collector, worldBase);
		planet->CollideSolidVolume(convex, scale1, shiftedTransform1, shiftedTransform2, localBase, subShapeIDCreator1, subShapeIDCreator2, settings, solidCollector);
	}

	static void sCastConvexVsPlanetTerrain(const ShapeCast &shapeCast, [[maybe_unused]] const ShapeCastSettings &settings, const Shape *shape, [[maybe_unused]] Vec3Arg scale, [[maybe_unused]] const ShapeFilter &shapeFilter, Mat44Arg planetTransform, const SubShapeIDCreator &shapeSubShapeIDCreator, const SubShapeIDCreator &planetSubShapeIDCreator, CastShapeCollector &collector)
	{
		JPH_ASSERT(shapeCast.mShape->GetType() == EShapeType::Convex);
		JPH_ASSERT(shape->GetSubType() == EShapeSubType::User1);
		const ConvexShape *convex = static_cast<const ConvexShape *>(shapeCast.mShape);
		const RNCustomPlanetTerrainShape *planet = static_cast<const RNCustomPlanetTerrainShape *>(shape);
		const Vec3 localBase = shapeCast.mCenterOfMassStart.GetTranslation();
		const Vec3 worldBase = planetTransform * localBase;
		const Mat44 shiftedPlanetTransform = GetShiftedReferenceTransform(planetTransform, localBase, worldBase);
		const ShapeCast shiftedShapeCast = shapeCast.PostTranslated(-localBase);
		OffsetCastShapeCollector offsetCollector(collector, worldBase);

		Vec3 surfacePosition;
		Vec3 surfaceNormal;
		Vec3 supportInPlanet;
		float penetration = 0.0f;
		if(planet->SampleConvexSupport(convex, shiftedShapeCast.mScale, shiftedShapeCast.mCenterOfMassStart, localBase, surfacePosition, surfaceNormal, supportInPlanet, penetration) && penetration > 0.0f)
		{
			planet->AddShapeCastHit(0.0f, shiftedPlanetTransform, surfacePosition, surfaceNormal, supportInPlanet, shapeSubShapeIDCreator, planetSubShapeIDCreator, offsetCollector);
			return;
		}

		float previousFraction = 0.0f;
		for(uint i = 1; i <= 32; i += 1)
		{
			const float currentFraction = static_cast<float>(i) / 32.0f;
			const Mat44 shapeTransform = shiftedShapeCast.mCenterOfMassStart.PostTranslated(shiftedShapeCast.mDirection * currentFraction);
			if(planet->SampleConvexSupport(convex, shiftedShapeCast.mScale, shapeTransform, localBase, surfacePosition, surfaceNormal, supportInPlanet, penetration) && penetration > 0.0f)
			{
				float minFraction = previousFraction;
				float maxFraction = currentFraction;
				for(uint step = 0; step < 10; step += 1)
				{
					const float midFraction = (minFraction + maxFraction) * 0.5f;
					const Mat44 midTransform = shiftedShapeCast.mCenterOfMassStart.PostTranslated(shiftedShapeCast.mDirection * midFraction);
					if(planet->SampleConvexSupport(convex, shiftedShapeCast.mScale, midTransform, localBase, surfacePosition, surfaceNormal, supportInPlanet, penetration) && penetration > 0.0f)
					{
						maxFraction = midFraction;
					}
					else
					{
						minFraction = midFraction;
					}
				}

				const Mat44 hitTransform = shiftedShapeCast.mCenterOfMassStart.PostTranslated(shiftedShapeCast.mDirection * maxFraction);
				if(planet->SampleConvexSupport(convex, shiftedShapeCast.mScale, hitTransform, localBase, surfacePosition, surfaceNormal, supportInPlanet, penetration))
				{
					planet->AddShapeCastHit(maxFraction, shiftedPlanetTransform, surfacePosition, surfaceNormal, supportInPlanet, shapeSubShapeIDCreator, planetSubShapeIDCreator, offsetCollector);
				}
				return;
			}

			previousFraction = currentFraction;
		}
	}

	float GetMaximumRadius() const
	{
		if(!_provider) return 0.0f;
		const float radius = _provider->GetMaximumPlanetTerrainRadius();
		return radius > 0.0f ? radius : 0.0f;
	}

	float GetSampleSpacing() const
	{
		if(!_provider) return 4.0f;
		const float spacing = _provider->GetPlanetTerrainCollisionSampleSpacing();
		return spacing > 0.25f ? spacing : 0.25f;
	}

	bool Sample(Vec3Arg direction, Vec3 &position, Vec3 &normal, float &radius) const
	{
		return Sample(direction, Vec3::sZero(), position, normal, radius);
	}

	bool Sample(Vec3Arg direction, Vec3Arg localBase, Vec3 &position, Vec3 &normal, float &radius) const
	{
		if(!_provider) return false;
		const Vec3 normalizedDirection = direction.NormalizedOr(Vec3::sAxisY());
		RN::JoltCustomPlanetTerrainSample sample;
		if(!_provider->SamplePlanetTerrain(normalizedDirection.GetX(), normalizedDirection.GetY(), normalizedDirection.GetZ(), sample)) return false;

		position = GetOffsetPosition(sample.positionX, sample.positionY, sample.positionZ, localBase);
		normal = Vec3(sample.normalX, sample.normalY, sample.normalZ).NormalizedOr(normalizedDirection);
		radius = sample.radius > 0.0f ? sample.radius : (position + localBase).Length();
		return true;
	}

	bool SampleConvexSupport(const ConvexShape *shape, Vec3Arg scale, Mat44Arg shapeTransformInPlanet, Vec3Arg localBase, Vec3 &surfacePosition, Vec3 &surfaceNormal, Vec3 &supportInPlanet, float &penetration) const
	{
		const Vec3 shapeCenter = shapeTransformInPlanet.GetTranslation() + localBase;
		const Vec3 direction = shapeCenter.NormalizedOr(Vec3::sAxisY());

		float surfaceRadius = 0.0f;
		if(!Sample(direction, localBase, surfacePosition, surfaceNormal, surfaceRadius)) return false;

		ConvexShape::SupportBuffer supportBuffer;
		const ConvexShape::Support *support = shape->GetSupportFunction(ConvexShape::ESupportMode::Default, supportBuffer, scale);
		if(!support) return false;

		const Vec3 supportDirection = shapeTransformInPlanet.Multiply3x3Transposed(-surfaceNormal);
		const Vec3 supportPositionInPlanet = shapeTransformInPlanet * support->GetSupport(supportDirection);
		const Vec3 supportDirectionInPlanet = (supportPositionInPlanet + localBase).NormalizedOr(direction);
		if(!Sample(supportDirectionInPlanet, localBase, surfacePosition, surfaceNormal, surfaceRadius)) return false;

		const float signedDistance = (supportPositionInPlanet - surfacePosition).Dot(surfaceNormal);
		const float convexRadius = support->GetConvexRadius();
		surfacePosition = supportPositionInPlanet - surfaceNormal * signedDistance;
		supportInPlanet = supportPositionInPlanet - surfaceNormal * convexRadius;
		penetration = -signedDistance + convexRadius;
		return true;
	}

	void AddShapeCastHit(float fraction, Mat44Arg planetTransform, Vec3Arg surfacePosition, Vec3Arg surfaceNormal, Vec3Arg supportInPlanet, const SubShapeIDCreator &shapeSubShapeIDCreator, const SubShapeIDCreator &planetSubShapeIDCreator, CastShapeCollector &collector) const
	{
		if(fraction >= collector.GetEarlyOutFraction()) return;

		const Vec3 supportWorld = planetTransform * supportInPlanet;
		const Vec3 surfaceWorld = planetTransform * surfacePosition;
		const Vec3 normalWorld = planetTransform.Multiply3x3(surfaceNormal).NormalizedOr(Vec3::sAxisY());
		ShapeCastResult result(fraction,
							   supportWorld,
							   surfaceWorld,
							   -normalWorld,
							   false,
							   shapeSubShapeIDCreator.GetID(),
							   planetSubShapeIDCreator.GetID(),
							   TransformedShape::sGetBodyID(collector.GetContext()));
		collector.AddHit(result);
	}

	float GetSignedDistance(Vec3Arg point) const
	{
		const Vec3 direction = point.NormalizedOr(Vec3::sAxisY());
		Vec3 position;
		Vec3 normal;
		float radius = 0.0f;
		if(!Sample(direction, position, normal, radius)) return FLT_MAX;
		return point.Length() - radius;
	}

	bool FindRayHit(const RayCast &ray, float maximumFraction, float &fraction) const
	{
		if(ray.mDirection.LengthSq() <= 0.0f) return false;

		float previousFraction = 0.0f;
		if(GetSignedDistance(ray.mOrigin) <= 0.0f)
		{
			fraction = 0.0f;
			return true;
		}

		for(uint i = 1; i <= 64; i += 1)
		{
			const float currentFraction = maximumFraction * static_cast<float>(i) / 64.0f;
			const Vec3 currentPosition = ray.mOrigin + ray.mDirection * currentFraction;
			const float currentDistance = GetSignedDistance(currentPosition);
			if(currentDistance <= 0.0f)
			{
				float minFraction = previousFraction;
				float maxFraction = currentFraction;
				for(uint step = 0; step < 10; step += 1)
				{
					const float midFraction = (minFraction + maxFraction) * 0.5f;
					if(GetSignedDistance(ray.mOrigin + ray.mDirection * midFraction) <= 0.0f)
					{
						maxFraction = midFraction;
					}
					else
					{
						minFraction = midFraction;
					}
				}
				fraction = maxFraction;
				return true;
			}

			previousFraction = currentFraction;
		}

		return false;
	}

	bool ShouldCollectTriangles(const AABox &box, float maxSeparationDistance) const
	{
		if(!_provider) return false;

		const float spacing = GetSampleSpacing();
		const Vec3 boxCenter = box.GetCenter();
		const Vec3 boxExtent = box.GetExtent();
		const float boxRadius = boxExtent.Length();
		const float centerDistance = boxCenter.Length();
		const float maximumRadius = _provider->GetMaximumPlanetTerrainRadius();
		const float minimumRadius = _provider->GetMinimumPlanetTerrainRadius();
		if(centerDistance - boxRadius > maximumRadius + spacing * 4.0f) return false;

		const Vec3 up = boxCenter.NormalizedOr(Vec3::sAxisY());
		if(centerDistance + boxRadius < minimumRadius - spacing * 4.0f) return false;

		Vec3 centerPosition;
		Vec3 centerNormal;
		float centerRadius = 0.0f;
		if(!Sample(up, centerPosition, centerNormal, centerRadius)) return false;
		(void)centerRadius;

		const float projectedRadius = abs(centerNormal.GetX()) * boxExtent.GetX() + abs(centerNormal.GetY()) * boxExtent.GetY() + abs(centerNormal.GetZ()) * boxExtent.GetZ();
		const float surfaceDistance = (boxCenter - centerPosition).Dot(centerNormal);
		const float tolerance = maxSeparationDistance + spacing * 0.5f;
		return surfaceDistance <= projectedRadius + tolerance && surfaceDistance >= -projectedRadius - tolerance;
	}

	void CollectProviderTriangles(const AABox &box, Vec3Arg localBase, Array<Triangle> &triangles, uint maximumTriangleCount) const
	{
		if(!_provider) return;

		class TriangleCollector final : public RN::JoltCustomPlanetTerrainInternalTriangleCollector
		{
		public:
			TriangleCollector(Array<Triangle> &triangles, uint maximumTriangleCount, Vec3Arg localBase) :
				_triangles(triangles),
				_maximumTriangleCount(maximumTriangleCount),
				_localBase(localBase)
			{}

			bool AddPlanetTerrainTriangle(const RN::JoltCustomPlanetTerrainTriangle &source) override
			{
				if(_triangles.size() >= _maximumTriangleCount) return false;

				Triangle triangle;
				for(uint i = 0; i < 3; i += 1)
				{
					triangle.vertices[i] = RNCustomPlanetTerrainShape::GetOffsetPosition(source.vertices[i][0], source.vertices[i][1], source.vertices[i][2], _localBase);
				}
				triangle.activeEdges = source.activeEdges;
				_triangles.push_back(triangle);
				return _triangles.size() < _maximumTriangleCount;
			}

		private:
			Array<Triangle> &_triangles;
			uint _maximumTriangleCount;
			Vec3 _localBase;
		};

		TriangleCollector collector(triangles, maximumTriangleCount, localBase);
		_provider->CollectPlanetTerrainTriangles(box.mMin.GetX(), box.mMin.GetY(), box.mMin.GetZ(), box.mMax.GetX(), box.mMax.GetY(), box.mMax.GetZ(), collector, maximumTriangleCount);
	}

	void CollectTriangles(const AABox &box, float maxSeparationDistance, Array<Triangle> &triangles, uint maximumTriangleCount) const
	{
		CollectTriangles(box, maxSeparationDistance, Vec3::sZero(), triangles, maximumTriangleCount);
	}

	void CollectTriangles(const AABox &box, float maxSeparationDistance, Vec3Arg localBase, Array<Triangle> &triangles, uint maximumTriangleCount) const
	{
		AABox queryBox = box;
		queryBox.Translate(localBase);
		if(!ShouldCollectTriangles(queryBox, maxSeparationDistance)) return;
		CollectProviderTriangles(queryBox, localBase, triangles, maximumTriangleCount);
	}

	RN::JoltCustomPlanetTerrainInternalProvider *_provider;
};

JPH_NAMESPACE_END

namespace RN
{
	namespace JoltCustomPlanetTerrainInternal
	{
		JPH::Shape *CreateShape(JoltCustomPlanetTerrainInternalProvider *provider)
		{
			return new JPH::RNCustomPlanetTerrainShape(provider);
		}

		void RegisterShape()
		{
			JPH::RNCustomPlanetTerrainShape::sRegister();
		}
	}
}
