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
	void CollideTriangles(const AABox &box, float maxSeparationDistance, const SubShapeIDCreator &subShapeIDCreator, Visitor &visitor) const
	{
		Array<Triangle> triangles;
		CollectTriangles(box, maxSeparationDistance, triangles, MaxCollisionTriangles);
		for(uint i = 0; i < triangles.size() && !visitor.ShouldAbort(); i += 1)
		{
			const Triangle &triangle = triangles[i];
			visitor.Collide(triangle.vertices[0], triangle.vertices[1], triangle.vertices[2], triangle.activeEdges, subShapeIDCreator.PushID(i & 0xffffu, 16).GetID());
		}
	}

	void CollideSolidVolume(const ConvexShape *shape, Vec3Arg scale, Mat44Arg shapeTransform, Mat44Arg planetTransform, const SubShapeIDCreator &shapeSubShapeIDCreator, const SubShapeIDCreator &planetSubShapeIDCreator, const CollideShapeSettings &settings, CollideShapeCollector &collector) const
	{
		if(!_provider) return;

		Vec3 surfacePosition;
		Vec3 surfaceNormal;
		Vec3 supportInPlanet;
		float penetration = 0.0f;
		if(!SampleConvexSupport(shape, scale, planetTransform.InversedRotationTranslation() * shapeTransform, surfacePosition, surfaceNormal, supportInPlanet, penetration)) return;
		if(penetration < -settings.mMaxSeparationDistance) return;

		const Vec3 supportWorld = planetTransform * supportInPlanet;
		const Vec3 contactPointOnPlanet = planetTransform * surfacePosition;
		const Vec3 contactNormalWorld = planetTransform.Multiply3x3(surfaceNormal).NormalizedOr(Vec3::sAxisY());
		CollideShapeResult result(supportWorld,
								  contactPointOnPlanet,
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

		Visitor visitor(convex, scale1, scale2, transform1, transform2, subShapeIDCreator1.GetID(), settings, collector);
		planet->CollideTriangles(visitor.GetQueryBounds(), settings.mMaxSeparationDistance, subShapeIDCreator2, visitor);
		planet->CollideSolidVolume(convex, scale1, transform1, transform2, subShapeIDCreator1, subShapeIDCreator2, settings, collector);
	}

	static void sCastConvexVsPlanetTerrain(const ShapeCast &shapeCast, [[maybe_unused]] const ShapeCastSettings &settings, const Shape *shape, [[maybe_unused]] Vec3Arg scale, [[maybe_unused]] const ShapeFilter &shapeFilter, Mat44Arg planetTransform, const SubShapeIDCreator &shapeSubShapeIDCreator, const SubShapeIDCreator &planetSubShapeIDCreator, CastShapeCollector &collector)
	{
		JPH_ASSERT(shapeCast.mShape->GetType() == EShapeType::Convex);
		JPH_ASSERT(shape->GetSubType() == EShapeSubType::User1);
		const ConvexShape *convex = static_cast<const ConvexShape *>(shapeCast.mShape);
		const RNCustomPlanetTerrainShape *planet = static_cast<const RNCustomPlanetTerrainShape *>(shape);

		Vec3 surfacePosition;
		Vec3 surfaceNormal;
		Vec3 supportInPlanet;
		float penetration = 0.0f;
		if(planet->SampleConvexSupport(convex, shapeCast.mScale, shapeCast.mCenterOfMassStart, surfacePosition, surfaceNormal, supportInPlanet, penetration) && penetration > 0.0f)
		{
			planet->AddShapeCastHit(0.0f, planetTransform, surfacePosition, surfaceNormal, supportInPlanet, shapeSubShapeIDCreator, planetSubShapeIDCreator, collector);
			return;
		}

		float previousFraction = 0.0f;
		for(uint i = 1; i <= 32; i += 1)
		{
			const float currentFraction = static_cast<float>(i) / 32.0f;
			const Mat44 shapeTransform = shapeCast.mCenterOfMassStart.PostTranslated(shapeCast.mDirection * currentFraction);
			if(planet->SampleConvexSupport(convex, shapeCast.mScale, shapeTransform, surfacePosition, surfaceNormal, supportInPlanet, penetration) && penetration > 0.0f)
			{
				float minFraction = previousFraction;
				float maxFraction = currentFraction;
				for(uint step = 0; step < 10; step += 1)
				{
					const float midFraction = (minFraction + maxFraction) * 0.5f;
					const Mat44 midTransform = shapeCast.mCenterOfMassStart.PostTranslated(shapeCast.mDirection * midFraction);
					if(planet->SampleConvexSupport(convex, shapeCast.mScale, midTransform, surfacePosition, surfaceNormal, supportInPlanet, penetration) && penetration > 0.0f)
					{
						maxFraction = midFraction;
					}
					else
					{
						minFraction = midFraction;
					}
				}

				const Mat44 hitTransform = shapeCast.mCenterOfMassStart.PostTranslated(shapeCast.mDirection * maxFraction);
				if(planet->SampleConvexSupport(convex, shapeCast.mScale, hitTransform, surfacePosition, surfaceNormal, supportInPlanet, penetration))
				{
					planet->AddShapeCastHit(maxFraction, planetTransform, surfacePosition, surfaceNormal, supportInPlanet, shapeSubShapeIDCreator, planetSubShapeIDCreator, collector);
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
		if(!_provider) return false;
		const Vec3 normalizedDirection = direction.NormalizedOr(Vec3::sAxisY());
		RN::JoltCustomPlanetTerrainSample sample;
		if(!_provider->SamplePlanetTerrain(normalizedDirection.GetX(), normalizedDirection.GetY(), normalizedDirection.GetZ(), sample)) return false;

		position = Vec3(static_cast<float>(sample.positionX), static_cast<float>(sample.positionY), static_cast<float>(sample.positionZ));
		normal = Vec3(sample.normalX, sample.normalY, sample.normalZ).NormalizedOr(normalizedDirection);
		radius = sample.radius > 0.0f ? sample.radius : position.Length();
		return true;
	}

	bool SampleConvexSupport(const ConvexShape *shape, Vec3Arg scale, Mat44Arg shapeTransformInPlanet, Vec3 &surfacePosition, Vec3 &surfaceNormal, Vec3 &supportInPlanet, float &penetration) const
	{
		const Vec3 shapeCenter = shapeTransformInPlanet.GetTranslation();
		const Vec3 direction = shapeCenter.NormalizedOr(Vec3::sAxisY());

		float surfaceRadius = 0.0f;
		if(!Sample(direction, surfacePosition, surfaceNormal, surfaceRadius)) return false;

		ConvexShape::SupportBuffer supportBuffer;
		const ConvexShape::Support *support = shape->GetSupportFunction(ConvexShape::ESupportMode::Default, supportBuffer, scale);
		if(!support) return false;

		const Vec3 supportDirection = shapeTransformInPlanet.Multiply3x3Transposed(-surfaceNormal);
		supportInPlanet = shapeTransformInPlanet * support->GetSupport(supportDirection);
		const Vec3 supportDirectionInPlanet = supportInPlanet.NormalizedOr(direction);
		if(!Sample(supportDirectionInPlanet, surfacePosition, surfaceNormal, surfaceRadius)) return false;

		penetration = surfaceRadius - supportInPlanet.Length();
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

	void CollectProviderTriangles(const AABox &box, Array<Triangle> &triangles, uint maximumTriangleCount) const
	{
		if(!_provider) return;

		class TriangleCollector final : public RN::JoltCustomPlanetTerrainInternalTriangleCollector
		{
		public:
			TriangleCollector(Array<Triangle> &triangles, uint maximumTriangleCount) :
				_triangles(triangles),
				_maximumTriangleCount(maximumTriangleCount)
			{}

			bool AddPlanetTerrainTriangle(const RN::JoltCustomPlanetTerrainTriangle &source) override
			{
				if(_triangles.size() >= _maximumTriangleCount) return false;

				Triangle triangle;
				for(uint i = 0; i < 3; i += 1)
				{
					triangle.vertices[i] = Vec3(static_cast<float>(source.vertices[i][0]), static_cast<float>(source.vertices[i][1]), static_cast<float>(source.vertices[i][2]));
				}
				triangle.activeEdges = source.activeEdges;
				_triangles.push_back(triangle);
				return _triangles.size() < _maximumTriangleCount;
			}

		private:
			Array<Triangle> &_triangles;
			uint _maximumTriangleCount;
		};

		TriangleCollector collector(triangles, maximumTriangleCount);
		_provider->CollectPlanetTerrainTriangles(box.mMin.GetX(), box.mMin.GetY(), box.mMin.GetZ(), box.mMax.GetX(), box.mMax.GetY(), box.mMax.GetZ(), collector, maximumTriangleCount);
	}

	void CollectTriangles(const AABox &box, float maxSeparationDistance, Array<Triangle> &triangles, uint maximumTriangleCount) const
	{
		if(!ShouldCollectTriangles(box, maxSeparationDistance)) return;
		CollectProviderTriangles(box, triangles, maximumTriangleCount);
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
