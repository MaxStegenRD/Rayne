//
//  RNJoltCustomPlanetTerrainShape.cpp
//  Rayne-Jolt
//
//  Copyright 2026 by Überpixel. All rights reserved.
//

#include "RNJoltCustomPlanetTerrainShape.h"
#include "RNJoltCustomPlanetTerrainShapeInternal.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace RN
{
	RNDefineMeta(JoltCustomPlanetTerrainProvider, Object)
	RNDefineMeta(JoltCustomPlanetTerrainShape, JoltShape)

	class JoltCustomPlanetTerrainProviderAdapter final : public JoltCustomPlanetTerrainInternalProvider
	{
	public:
		JoltCustomPlanetTerrainProviderAdapter(JoltCustomPlanetTerrainProvider *provider) :
			_referenceCount(1),
			_provider(provider ? provider->Retain() : nullptr)
		{}

		void RetainProvider() override
		{
			_referenceCount += 1;
		}

		void ReleaseProvider() override
		{
			_referenceCount -= 1;
			if(_referenceCount == 0) delete this;
		}

		float GetMinimumPlanetTerrainRadius() const override
		{
			return _provider ? _provider->GetMinimumPlanetTerrainRadius() : 0.0f;
		}

		float GetMaximumPlanetTerrainRadius() const override
		{
			return _provider ? _provider->GetMaximumPlanetTerrainRadius() : 0.0f;
		}

		float GetPlanetTerrainCollisionSampleSpacing() const override
		{
			return _provider ? _provider->GetPlanetTerrainCollisionSampleSpacing() : 2.0f;
		}

		bool SamplePlanetTerrain(float directionX, float directionY, float directionZ, JoltCustomPlanetTerrainSample &sample) const override
		{
			if(!_provider) return false;

			JoltCustomPlanetTerrainProvider::SurfaceSample surfaceSample;
			if(!_provider->SamplePlanetTerrain(Vector3(directionX, directionY, directionZ), surfaceSample)) return false;

			sample.positionX = surfaceSample.position.x;
			sample.positionY = surfaceSample.position.y;
			sample.positionZ = surfaceSample.position.z;
			sample.normalX = surfaceSample.normal.x;
			sample.normalY = surfaceSample.normal.y;
			sample.normalZ = surfaceSample.normal.z;
			sample.radius = surfaceSample.radius;
			return true;
		}

		bool CollectPlanetTerrainTriangles(float minimumX, float minimumY, float minimumZ, float maximumX, float maximumY, float maximumZ, JoltCustomPlanetTerrainInternalTriangleCollector &collector, unsigned int maximumTriangleCount) const override
		{
			if(!_provider) return false;

			class CollectorAdapter final : public JoltCustomPlanetTerrainProvider::SurfaceTriangleCollector
			{
			public:
				CollectorAdapter(JoltCustomPlanetTerrainInternalTriangleCollector &collector) :
					_collector(collector)
				{}

				bool AddTriangle(const JoltCustomPlanetTerrainProvider::SurfaceTriangle &triangle) override
				{
					JoltCustomPlanetTerrainTriangle internalTriangle;
					for(uint8 i = 0; i < 3; i += 1)
					{
						internalTriangle.vertices[i][0] = triangle.vertices[i].x;
						internalTriangle.vertices[i][1] = triangle.vertices[i].y;
						internalTriangle.vertices[i][2] = triangle.vertices[i].z;
					}
					internalTriangle.activeEdges = triangle.activeEdges;
					return _collector.AddPlanetTerrainTriangle(internalTriangle);
				}

			private:
				JoltCustomPlanetTerrainInternalTriangleCollector &_collector;
			};

			CollectorAdapter adapter(collector);
			return _provider->CollectPlanetTerrainTriangles(JoltPosition(minimumX, minimumY, minimumZ), JoltPosition(maximumX, maximumY, maximumZ), adapter, maximumTriangleCount);
		}

	private:
		~JoltCustomPlanetTerrainProviderAdapter() override
		{
			if(_provider) _provider->Release();
		}

		uint32 _referenceCount;
		JoltCustomPlanetTerrainProvider *_provider;
	};

	float JoltCustomPlanetTerrainProvider::GetPlanetTerrainCollisionSampleSpacing() const
	{
		return 2.0f;
	}

	bool JoltCustomPlanetTerrainProvider::CollectPlanetTerrainTriangles([[maybe_unused]] const JoltPosition &minimum, [[maybe_unused]] const JoltPosition &maximum, [[maybe_unused]] SurfaceTriangleCollector &collector, [[maybe_unused]] uint32 maximumTriangleCount) const
	{
		return false;
	}

	JoltCustomPlanetTerrainShape::JoltCustomPlanetTerrainShape(JoltCustomPlanetTerrainProvider *provider)
	{
		JoltCustomPlanetTerrainProviderAdapter *adapter = new JoltCustomPlanetTerrainProviderAdapter(provider);
		_shape = JoltCustomPlanetTerrainInternal::CreateShape(adapter);
		adapter->ReleaseProvider();
		_shape->AddRef();
	}

	JoltCustomPlanetTerrainShape *JoltCustomPlanetTerrainShape::WithProvider(JoltCustomPlanetTerrainProvider *provider)
	{
		JoltCustomPlanetTerrainShape *shape = new JoltCustomPlanetTerrainShape(provider);
		return shape->Autorelease();
	}

	void JoltCustomPlanetTerrainShape::RegisterJoltShape()
	{
		JoltCustomPlanetTerrainInternal::RegisterShape();
	}
}
