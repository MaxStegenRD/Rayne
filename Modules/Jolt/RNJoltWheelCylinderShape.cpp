//
//  RNJoltWheelCylinderShape.cpp
//  Rayne-Jolt
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNJoltWheelCylinderShape.h"

#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/CollideSoftBodyVertexIterator.h>
#include <Jolt/Geometry/RayCylinder.h>
#include <Jolt/Math/Trigonometry.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif

JPH_NAMESPACE_BEGIN

static const Vec3 cWheelCylinderTopFace[] =
{
	Vec3(0.0f,			1.0f,	1.0f),
	Vec3(0.707106769f,	1.0f,	0.707106769f),
	Vec3(1.0f,			1.0f,	0.0f),
	Vec3(0.707106769f,	1.0f,	-0.707106769f),
	Vec3(-0.0f,			1.0f,	-1.0f),
	Vec3(-0.707106769f,	1.0f,	-0.707106769f),
	Vec3(-1.0f,			1.0f,	0.0f),
	Vec3(-0.707106769f,	1.0f,	0.707106769f)
};

class RNWheelCylinderShape::WheelCylinder final : public Support
{
public:
	WheelCylinder(float halfHeight, float radius, float convexRadius) :
		_halfHeight(halfHeight),
		_radius(radius),
		_convexRadius(convexRadius)
	{
		static_assert(sizeof(WheelCylinder) <= sizeof(SupportBuffer), "Buffer size too small");
		JPH_ASSERT(IsAligned(this, alignof(WheelCylinder)));
	}

	Vec3 GetSupport(Vec3Arg direction) const override
	{
		float x = direction.GetX();
		float y = direction.GetY();
		float z = direction.GetZ();
		float o = Sqrt(Square(x) + Square(z));
		if(o > 0.0f) return Vec3((_radius * x) / o, Sign(y) * _halfHeight, (_radius * z) / o);
		return Vec3(0.0f, Sign(y) * _halfHeight, 0.0f);
	}

	float GetConvexRadius() const override
	{
		return _convexRadius;
	}

private:
	float _halfHeight;
	float _radius;
	float _convexRadius;
};

RNWheelCylinderShape::RNWheelCylinderShape() :
	ConvexShape(EShapeSubType::UserConvex1)
{}

RNWheelCylinderShape::RNWheelCylinderShape(float halfHeight, float radius, float convexRadius, const PhysicsMaterial *material) :
	ConvexShape(EShapeSubType::UserConvex1, material),
	_halfHeight(halfHeight),
	_radius(radius),
	_convexRadius(min(convexRadius, min(halfHeight, radius)))
{
	JPH_ASSERT(halfHeight >= 0.0f);
	JPH_ASSERT(radius >= 0.0f);
	JPH_ASSERT(convexRadius >= 0.0f);
}

const ConvexShape::Support *RNWheelCylinderShape::GetSupportFunction(ESupportMode mode, SupportBuffer &buffer, Vec3Arg scale) const
{
	JPH_ASSERT(IsValidScale(scale));

	Vec3 absScale = scale.Abs();
	float scaleXZ = absScale.GetX();
	float scaleY = absScale.GetY();
	float scaledHalfHeight = scaleY * _halfHeight;
	float scaledRadius = scaleXZ * _radius;
	float scaledConvexRadius = ScaleHelpers::ScaleConvexRadius(_convexRadius, scale);

	switch(mode)
	{
		case ESupportMode::IncludeConvexRadius:
		case ESupportMode::Default:
			return new(&buffer) WheelCylinder(scaledHalfHeight, scaledRadius, 0.0f);

		case ESupportMode::ExcludeConvexRadius:
			return new(&buffer) WheelCylinder(scaledHalfHeight - scaledConvexRadius, scaledRadius - scaledConvexRadius, scaledConvexRadius);
	}

	JPH_ASSERT(false);
	return nullptr;
}

void RNWheelCylinderShape::GetSupportingFace(const SubShapeID &subShapeID, Vec3Arg direction, Vec3Arg scale, Mat44Arg centerOfMassTransform, SupportingFace &outVertices) const
{
	JPH_ASSERT(subShapeID.IsEmpty(), "Invalid subshape ID");
	JPH_ASSERT(IsValidScale(scale));

	Vec3 absScale = scale.Abs();
	float scaleXZ = absScale.GetX();
	float scaleY = absScale.GetY();
	float scaledHalfHeight = scaleY * _halfHeight;
	float scaledRadius = scaleXZ * _radius;

	float x = direction.GetX();
	float y = direction.GetY();
	float z = direction.GetZ();
	float xzSq = Square(x) + Square(z);
	float ySq = Square(y);

	if(xzSq > ySq)
	{
		float invXZLength = 1.0f / Sqrt(xzSq);
		Vec3 radial = Vec3(-x * invXZLength, 0.0f, -z * invXZLength);
		Vec3 tangent = Vec3(-radial.GetZ(), 0.0f, radial.GetX());
		float scaledConvexRadius = ScaleHelpers::ScaleConvexRadius(_convexRadius, scale);
		float patchHalfChord = min(0.25f * scaledConvexRadius, 0.25f * scaledRadius);
		if(patchHalfChord <= 0.0f)
		{
			Vec3 edge = radial * scaledRadius;
			outVertices.push_back(centerOfMassTransform * Vec3(edge.GetX(), scaledHalfHeight, edge.GetZ()));
			outVertices.push_back(centerOfMassTransform * Vec3(edge.GetX(), -scaledHalfHeight, edge.GetZ()));
			return;
		}

		float patchHalfAngle = ASin(patchHalfChord / max(scaledRadius, 1.0e-6f));
		float sinAngle = Sin(patchHalfAngle);
		float cosAngle = Cos(patchHalfAngle);
		Vec3 edge1 = (radial * cosAngle + tangent * sinAngle) * scaledRadius;
		Vec3 edge2 = (radial * cosAngle - tangent * sinAngle) * scaledRadius;

		outVertices.push_back(centerOfMassTransform * Vec3(edge1.GetX(), scaledHalfHeight, edge1.GetZ()));
		outVertices.push_back(centerOfMassTransform * Vec3(edge1.GetX(), -scaledHalfHeight, edge1.GetZ()));
		outVertices.push_back(centerOfMassTransform * Vec3(edge2.GetX(), -scaledHalfHeight, edge2.GetZ()));
		outVertices.push_back(centerOfMassTransform * Vec3(edge2.GetX(), scaledHalfHeight, edge2.GetZ()));
	}
	else
	{
		Mat44 transform = centerOfMassTransform;
		if(xzSq > 0.00765427f * ySq)
		{
			float o = scaledRadius / Sqrt(xzSq);
			Vec3 xAxis = Vec3(x * o, 0.0f, z * o);
			Vec3 yAxis = Sign(y) * Vec3(0.0f, scaledHalfHeight, 0.0f);
			Vec3 zAxis = yAxis.Cross(xAxis);
			transform = transform * Mat44(Vec4(xAxis, 0.0f), Vec4(yAxis, 0.0f), Vec4(zAxis, 0.0f), Vec4(0.0f, 0.0f, 0.0f, 1.0f));
		}
		else
		{
			Vec3 multiplier = y < 0.0f ? Vec3(scaledRadius, scaledHalfHeight, scaledRadius) : Vec3(-scaledRadius, -scaledHalfHeight, scaledRadius);
			transform = transform.PreScaled(multiplier);
		}

		for(const Vec3 &vertex : cWheelCylinderTopFace)
		{
			outVertices.push_back(transform * vertex);
		}
	}
}

MassProperties RNWheelCylinderShape::GetMassProperties() const
{
	MassProperties properties;
	float radiusSq = Square(_radius);
	float height = 2.0f * _halfHeight;
	properties.mMass = JPH_PI * radiusSq * height * GetDensity();

	float inertiaY = radiusSq * properties.mMass * 0.5f;
	float inertiaX = inertiaY * 0.5f + properties.mMass * height * height / 12.0f;
	properties.mInertia = Mat44::sScale(Vec3(inertiaX, inertiaY, inertiaX));

	return properties;
}

Vec3 RNWheelCylinderShape::GetSurfaceNormal(const SubShapeID &subShapeID, Vec3Arg localSurfacePosition) const
{
	JPH_ASSERT(subShapeID.IsEmpty(), "Invalid subshape ID");

	Vec3 localSurfacePositionXZ(localSurfacePosition.GetX(), 0.0f, localSurfacePosition.GetZ());
	float localSurfacePositionXZLength = localSurfacePositionXZ.Length();
	float distanceToCurvedSurface = abs(localSurfacePositionXZLength - _radius);
	float distanceToTopOrBottom = abs(abs(localSurfacePosition.GetY()) - _halfHeight);

	if(distanceToCurvedSurface < distanceToTopOrBottom)
	{
		return localSurfacePositionXZLength > 0.0f ? localSurfacePositionXZ / localSurfacePositionXZLength : Vec3::sAxisX();
	}

	return localSurfacePosition.GetY() > 0.0f ? Vec3::sAxisY() : -Vec3::sAxisY();
}

AABox RNWheelCylinderShape::GetLocalBounds() const
{
	Vec3 extent(_radius, _halfHeight, _radius);
	return AABox(-extent, extent);
}

#ifdef JPH_DEBUG_RENDERER
void RNWheelCylinderShape::Draw(DebugRenderer *renderer, RMat44Arg centerOfMassTransform, Vec3Arg scale, ColorArg color, bool useMaterialColors, bool drawWireframe) const
{
	DebugRenderer::EDrawMode drawMode = drawWireframe ? DebugRenderer::EDrawMode::Wireframe : DebugRenderer::EDrawMode::Solid;
	renderer->DrawCylinder(centerOfMassTransform * Mat44::sScale(scale.Abs()), _halfHeight, _radius, useMaterialColors ? GetMaterial()->GetDebugColor() : color, DebugRenderer::ECastShadow::On, drawMode);
}
#endif

bool RNWheelCylinderShape::CastRay(const RayCast &ray, const SubShapeIDCreator &subShapeIDCreator, RayCastResult &hit) const
{
	float fraction = RayCylinder(ray.mOrigin, ray.mDirection, _halfHeight, _radius);
	if(fraction < hit.mFraction)
	{
		hit.mFraction = fraction;
		hit.mSubShapeID2 = subShapeIDCreator.GetID();
		return true;
	}

	return false;
}

void RNWheelCylinderShape::CollidePoint(Vec3Arg point, const SubShapeIDCreator &subShapeIDCreator, CollidePointCollector &collector, const ShapeFilter &shapeFilter) const
{
	if(!shapeFilter.ShouldCollide(this, subShapeIDCreator.GetID())) return;

	if(abs(point.GetY()) <= _halfHeight && Square(point.GetX()) + Square(point.GetZ()) <= Square(_radius))
	{
		collector.AddHit({ TransformedShape::sGetBodyID(collector.GetContext()), subShapeIDCreator.GetID() });
	}
}

void RNWheelCylinderShape::CollideSoftBodyVertices(Mat44Arg centerOfMassTransform, Vec3Arg scale, const CollideSoftBodyVertexIterator &vertices, uint numVertices, int collidingShapeIndex) const
{
	JPH_ASSERT(IsValidScale(scale));

	Mat44 inverseTransform = centerOfMassTransform.InversedRotationTranslation();
	Vec3 absScale = scale.Abs();
	float halfHeight = absScale.GetY() * _halfHeight;
	float radius = absScale.GetX() * _radius;

	for(CollideSoftBodyVertexIterator vertex = vertices, end = vertices + numVertices; vertex != end; ++vertex)
	{
		if(vertex.GetInvMass() <= 0.0f) continue;

		Vec3 localPosition = inverseTransform * vertex.GetPosition();
		Vec3 sideNormal = localPosition;
		sideNormal.SetY(0.0f);
		float sideNormalLength = sideNormal.Length();
		float sidePenetration = radius - sideNormalLength;
		float topPenetration = halfHeight - abs(localPosition.GetY());

		Vec3 point;
		Vec3 normal;
		if(sidePenetration < 0.0f && topPenetration < 0.0f)
		{
			point = sideNormal * (radius / sideNormalLength) + Vec3(0.0f, halfHeight * Sign(localPosition.GetY()), 0.0f);
			normal = (localPosition - point).NormalizedOr(Vec3::sAxisY());
		}
		else if(sidePenetration < topPenetration)
		{
			normal = sideNormalLength > 0.0f ? sideNormal / sideNormalLength : Vec3::sAxisX();
			point = radius * normal;
		}
		else
		{
			normal = Vec3(0.0f, Sign(localPosition.GetY()), 0.0f);
			point = halfHeight * normal;
		}

		Plane plane = Plane::sFromPointAndNormal(point, normal);
		float penetration = -plane.SignedDistance(localPosition);
		if(vertex.UpdatePenetration(penetration))
		{
			vertex.SetCollision(plane.GetTransformed(centerOfMassTransform), collidingShapeIndex);
		}
	}
}

void RNWheelCylinderShape::SaveBinaryState(StreamOut &stream) const
{
	ConvexShape::SaveBinaryState(stream);
	stream.Write(_halfHeight);
	stream.Write(_radius);
	stream.Write(_convexRadius);
}

void RNWheelCylinderShape::RestoreBinaryState(StreamIn &stream)
{
	ConvexShape::RestoreBinaryState(stream);
	stream.Read(_halfHeight);
	stream.Read(_radius);
	stream.Read(_convexRadius);
}

bool RNWheelCylinderShape::IsValidScale(Vec3Arg scale) const
{
	return ConvexShape::IsValidScale(scale) && ScaleHelpers::IsUniformScaleXZ(scale.Abs());
}

Vec3 RNWheelCylinderShape::MakeScaleValid(Vec3Arg scale) const
{
	Vec3 validScale = ScaleHelpers::MakeNonZeroScale(scale);
	return validScale.GetSign() * ScaleHelpers::MakeUniformScaleXZ(validScale.Abs());
}

void RNWheelCylinderShape::sRegister()
{
	ShapeFunctions &functions = ShapeFunctions::sGet(EShapeSubType::UserConvex1);
	functions.mConstruct = []() -> Shape * { return new RNWheelCylinderShape; };
	functions.mColor = Color::sGreen;
}

JPH_NAMESPACE_END
