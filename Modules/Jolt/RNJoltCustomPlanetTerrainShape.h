//
//  RNJoltCustomPlanetTerrainShape.h
//  Rayne-Jolt
//
//  Copyright 2026 by Überpixel. All rights reserved.
//

#ifndef __RAYNE_JOLTCUSTOMPLANETTERRAINSHAPE_H_
#define __RAYNE_JOLTCUSTOMPLANETTERRAINSHAPE_H_

#include "RNJoltShape.h"

namespace RN
{
	class JoltCustomPlanetTerrainProvider : public Object
	{
	public:
		struct SurfaceTriangle
		{
			JoltPosition vertices[3];
			uint32 id;
			uint8 activeEdges;
		};

		class SurfaceTriangleCollector
		{
		public:
			virtual bool AddTriangle(const SurfaceTriangle &triangle) = 0;

		protected:
			virtual ~SurfaceTriangleCollector() = default;
		};

		struct SurfaceSample
		{
			JoltPosition position;
			Vector3 normal;
			float radius;
		};

		JTAPI virtual float GetMinimumPlanetTerrainRadius() const = 0;
		JTAPI virtual float GetMaximumPlanetTerrainRadius() const = 0;
		JTAPI virtual float GetPlanetTerrainCollisionSampleSpacing() const;
		JTAPI virtual bool SamplePlanetTerrain(const Vector3 &direction, SurfaceSample &sample) const = 0;
		JTAPI virtual bool CollectPlanetTerrainTriangles(const JoltPosition &minimum, const JoltPosition &maximum, SurfaceTriangleCollector &collector, uint32 maximumTriangleCount) const;

		RNDeclareMetaAPI(JoltCustomPlanetTerrainProvider, JTAPI)
	};

	class JoltCustomPlanetTerrainShape : public JoltShape
	{
	public:
		JTAPI JoltCustomPlanetTerrainShape(JoltCustomPlanetTerrainProvider *provider);

		JTAPI static JoltCustomPlanetTerrainShape *WithProvider(JoltCustomPlanetTerrainProvider *provider);
		JTAPI static void RegisterJoltShape();

		RNDeclareMetaAPI(JoltCustomPlanetTerrainShape, JTAPI)
	};
}

#endif /* defined(__RAYNE_JOLTCUSTOMPLANETTERRAINSHAPE_H_) */
