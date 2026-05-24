//
//  RNJoltWheelCylinderShape.h
//  Rayne-Jolt
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_JOLTWHEELCYLINDERSHAPE_H_
#define __RAYNE_JOLTWHEELCYLINDERSHAPE_H_

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/PhysicsSettings.h>

JPH_NAMESPACE_BEGIN

class RNWheelCylinderShape final : public ConvexShape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	RNWheelCylinderShape();
	RNWheelCylinderShape(float halfHeight, float radius, float convexRadius = cDefaultConvexRadius, const PhysicsMaterial *material = nullptr);

	float GetHalfHeight() const { return _halfHeight; }
	float GetRadius() const { return _radius; }
	float GetConvexRadius() const { return _convexRadius; }

	AABox GetLocalBounds() const override;
	float GetInnerRadius() const override { return min(_halfHeight, _radius); }
	MassProperties GetMassProperties() const override;
	Vec3 GetSurfaceNormal(const SubShapeID &subShapeID, Vec3Arg localSurfacePosition) const override;
	void GetSupportingFace(const SubShapeID &subShapeID, Vec3Arg direction, Vec3Arg scale, Mat44Arg centerOfMassTransform, SupportingFace &outVertices) const override;
	const Support *GetSupportFunction(ESupportMode mode, SupportBuffer &buffer, Vec3Arg scale) const override;

#ifdef JPH_DEBUG_RENDERER
	void Draw(DebugRenderer *renderer, RMat44Arg centerOfMassTransform, Vec3Arg scale, ColorArg color, bool useMaterialColors, bool drawWireframe) const override;
#endif

	bool CastRay(const RayCast &ray, const SubShapeIDCreator &subShapeIDCreator, RayCastResult &hit) const override;
	void CollidePoint(Vec3Arg point, const SubShapeIDCreator &subShapeIDCreator, CollidePointCollector &collector, const ShapeFilter &shapeFilter = { }) const override;
	void CollideSoftBodyVertices(Mat44Arg centerOfMassTransform, Vec3Arg scale, const CollideSoftBodyVertexIterator &vertices, uint numVertices, int collidingShapeIndex) const override;

	void SaveBinaryState(StreamOut &stream) const override;
	Stats GetStats() const override { return Stats(sizeof(*this), 0); }
	float GetVolume() const override { return 2.0f * JPH_PI * _halfHeight * Square(_radius); }
	bool IsValidScale(Vec3Arg scale) const override;
	Vec3 MakeScaleValid(Vec3Arg scale) const override;

	static void sRegister();

protected:
	void RestoreBinaryState(StreamIn &stream) override;

private:
	class WheelCylinder;

	float _halfHeight = 0.0f;
	float _radius = 0.0f;
	float _convexRadius = 0.0f;
};

JPH_NAMESPACE_END

#endif /* defined(__RAYNE_JOLTWHEELCYLINDERSHAPE_H_) */
