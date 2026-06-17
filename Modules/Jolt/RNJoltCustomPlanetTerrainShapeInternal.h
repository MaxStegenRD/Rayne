//
//  RNJoltCustomPlanetTerrainShapeInternal.h
//  Rayne-Jolt
//
//  Copyright 2026 by Überpixel. All rights reserved.
//

#ifndef __RAYNE_JOLTCUSTOMPLANETTERRAINSHAPEINTERNAL_H_
#define __RAYNE_JOLTCUSTOMPLANETTERRAINSHAPEINTERNAL_H_

namespace JPH
{
	class Shape;
}

namespace RN
{
	struct JoltCustomPlanetTerrainSample
	{
		double positionX;
		double positionY;
		double positionZ;
		float normalX;
		float normalY;
		float normalZ;
		float radius;
	};

	struct JoltCustomPlanetTerrainTriangle
	{
		double vertices[3][3];
		unsigned int id;
		unsigned char activeEdges;
	};

	class JoltCustomPlanetTerrainInternalTriangleCollector
	{
	public:
		virtual bool AddPlanetTerrainTriangle(const JoltCustomPlanetTerrainTriangle &triangle) = 0;

	protected:
		virtual ~JoltCustomPlanetTerrainInternalTriangleCollector() = default;
	};

	class JoltCustomPlanetTerrainInternalProvider
	{
	public:
		virtual void RetainProvider() = 0;
		virtual void ReleaseProvider() = 0;
		virtual float GetMinimumPlanetTerrainRadius() const = 0;
		virtual float GetMaximumPlanetTerrainRadius() const = 0;
		virtual float GetPlanetTerrainCollisionSampleSpacing() const = 0;
		virtual bool SamplePlanetTerrain(float directionX, float directionY, float directionZ, JoltCustomPlanetTerrainSample &sample) const = 0;
		virtual bool CollectPlanetTerrainTriangles(float minimumX, float minimumY, float minimumZ, float maximumX, float maximumY, float maximumZ, JoltCustomPlanetTerrainInternalTriangleCollector &collector, unsigned int maximumTriangleCount) const = 0;

	protected:
		virtual ~JoltCustomPlanetTerrainInternalProvider() = default;
	};

	namespace JoltCustomPlanetTerrainInternal
	{
		JPH::Shape *CreateShape(JoltCustomPlanetTerrainInternalProvider *provider);
		void RegisterShape();
	}
}

#endif /* defined(__RAYNE_JOLTCUSTOMPLANETTERRAINSHAPEINTERNAL_H_) */
