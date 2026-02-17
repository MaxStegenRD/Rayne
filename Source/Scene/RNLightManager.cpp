//
//  RNLightManager.cpp
//  Rayne
//
//  Copyright 2025 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNLightManager.h"
#include "RNScene.h"
#include "../Rendering/RNRenderer.h"

namespace RN
{
	RNDefineMeta(LightManager, Object)

	LightManager::LightManager(uint32 x, uint32 y, uint32 z, float zLogFactor) :
		_pointLightBuffer(nullptr),
		_spotLightBuffer(nullptr),
		_clusterIndexBuffer(nullptr),
		_clusterRecordsBuffer(nullptr),
		_lastViewportWidth(0.0f),
		_lastViewportHeight(0.0f),
		_lastClipNear(0.0f),
		_lastClipFar(0.0f),
		_maxLightsPerCluster(255)
	{
		SetClusterGridInfo(x, y, z, zLogFactor);
		_grid.zFirstSliceDepth = 3.0f;
	}

	LightManager::~LightManager()
	{
		SafeRelease(_pointLightBuffer);
		SafeRelease(_spotLightBuffer);
		SafeRelease(_clusterIndexBuffer);
		SafeRelease(_clusterRecordsBuffer);
	}

	void LightManager::SetClusterGridInfo(uint32 x, uint32 y, uint32 z, float zLogFactor)
	{
		_grid.clustersX = std::max<uint32>(1, x);
		_grid.clustersY = std::max<uint32>(1, y);
		_grid.clustersZ = std::max<uint32>(1, z);
		_grid.zLogFactor = std::clamp(zLogFactor, 0.0f, 1.0f);

		// Ensure buffers for new cluster count are reasonably pre-sized
		PreallocateBuffers(256, 128, 16, 8);
	}

	void LightManager::PreallocateBuffers(uint32 pointEstimate, uint32 spotEstimate, uint16 pointPerClusterEstimate, uint16 spotPerClusterEstimate)
	{
		uint32 clusterCount = ComputeClusterCount();
		pointPerClusterEstimate = std::max<uint16_t>(1, std::min<uint16_t>(pointPerClusterEstimate, _maxLightsPerCluster));
		spotPerClusterEstimate = std::max<uint16_t>(1, std::min<uint16_t>(spotPerClusterEstimate, _maxLightsPerCluster));

		// Metal requires uniform buffers to be at least as large as the declared array sizes in HLSL
		const uint32 kLightsPointMax = 256; // must match Base.hlsl
		const uint32 kLightsSpotMax  = 256; // must match Base.hlsl

		size_t pointBytes = std::max<size_t>(pointEstimate, kLightsPointMax) * sizeof(PointLightPacked);
		size_t spotBytes  = std::max<size_t>(spotEstimate,  kLightsSpotMax)  * sizeof(SpotLightPacked);
		size_t indexBytes = clusterCount * (pointPerClusterEstimate + spotPerClusterEstimate) * sizeof(uint16);
		size_t headerBytes = sizeof(ClusterGridInfo);

		// Records buffer must match shader-declared maximum array size
		const uint32 kLightClusterRecordsMax = 4000;
		size_t recordsBytes = headerBytes + static_cast<size_t>(kLightClusterRecordsMax) * sizeof(ClusterRecord);

		if(!_pointLightBuffer || _pointLightBuffer->GetLength() < pointBytes)
		{
			SafeRelease(_pointLightBuffer);
			_pointLightBuffer = Renderer::GetActiveRenderer()->CreateBufferWithLength(pointBytes, GPUResource::UsageOptions::Uniform, GPUResource::AccessOptions::WriteOnly, true);
		}
		if(!_spotLightBuffer || _spotLightBuffer->GetLength() < spotBytes)
		{
			SafeRelease(_spotLightBuffer);
			_spotLightBuffer = Renderer::GetActiveRenderer()->CreateBufferWithLength(spotBytes, GPUResource::UsageOptions::Uniform, GPUResource::AccessOptions::WriteOnly, true);
		}
		if(!_clusterIndexBuffer || _clusterIndexBuffer->GetLength() < indexBytes)
		{
			SafeRelease(_clusterIndexBuffer);
			_clusterIndexBuffer = Renderer::GetActiveRenderer()->CreateBufferWithLength(indexBytes, GPUResource::UsageOptions::Uniform, GPUResource::AccessOptions::WriteOnly, true);
		}
		if(!_clusterRecordsBuffer || _clusterRecordsBuffer->GetLength() < recordsBytes)
		{
			SafeRelease(_clusterRecordsBuffer);
			_clusterRecordsBuffer = Renderer::GetActiveRenderer()->CreateBufferWithLength(recordsBytes, GPUResource::UsageOptions::Uniform, GPUResource::AccessOptions::WriteOnly, true);
		}
	}

	void LightManager::SetZLogFactor(float zLogFactor)
	{
		_grid.zLogFactor = std::clamp(zLogFactor, 0.0f, 1.0f);
	}

	void LightManager::SetZFirstSliceDepth(float meters)
	{
		_grid.zFirstSliceDepth = std::max(0.0f, meters);
	}

	void LightManager::SetMaxLightsPerCluster(uint16_t max)
	{
		uint16_t clamped = std::max<uint16_t>(1, std::min<uint16_t>(max, static_cast<uint16_t>(255)));
		if(_maxLightsPerCluster == clamped) return;
		_maxLightsPerCluster = clamped;

		// Re-preallocate with the new per-cluster limits; keep conservative light estimates if none packed yet
		uint32 pointEstimate = _packedPointLights.empty() ? 256u : static_cast<uint32>(_packedPointLights.size());
		uint32 spotEstimate  = _packedSpotLights.empty()  ? 128u : static_cast<uint32>(_packedSpotLights.size());
		PreallocateBuffers(pointEstimate, spotEstimate, clamped, clamped);
	}

	void LightManager::BuildForCamera(Camera *camera, const std::vector<Light *> &lights)
	{
		ClearData();
		PackLights(lights);
		BuildClusters(camera);
		UploadBuffers();
	}

	void LightManager::ClearData()
	{
		_packedPointLights.clear();
		_packedSpotLights.clear();
		_clusterLightIndices.clear();
		_clusterRecords.clear();
	}

	void LightManager::PackLights(const std::vector<Light *> &lights)
	{
		_packedPointLights.reserve(lights.size());
		_packedSpotLights.reserve(lights.size());
		for(const Light *light : lights)
		{
			Light::Type type = light->GetType();
			if(type == Light::Type::DirectionalLight) continue; // not clustered

			if(type == Light::Type::SpotLight)
			{
				Vector3 pos = light->GetWorldPosition();
				Vector3 dir = light->GetWorldRotation().GetRotatedVector(Vector3(0.0f, 0.0f, -1.0f));
				SpotLightPacked out;
				out.positionRange = Vector4(pos.x, pos.y, pos.z, light->GetRange());
				out.color = light->GetFinalColor();
				out.dirCos = Vector4(dir.x, dir.y, dir.z, light->GetAngleCos());
				_packedSpotLights.push_back(out);
			}
			else // Point
			{
				Vector3 pos = light->GetWorldPosition();
				PointLightPacked out;
				out.positionRange = Vector4(pos.x, pos.y, pos.z, light->GetRange());
				out.color = light->GetFinalColor();
				_packedPointLights.push_back(out);
			}
		}
	}

	float LightManager::ComputeZSlice(const Camera *camera, float viewZ) const
	{
		const uint32 slices = _grid.clustersZ;
		const float n = camera->GetClipNear();
		const float f = camera->GetClipFar();
		const float z = std::clamp(viewZ, n, f);

		if(_grid.zFirstSliceDepth > 0.0f)
		{
			const float firstEnd = std::min(n + _grid.zFirstSliceDepth, f);
			if(z <= firstEnd) return 0.0f;

			const float uLinear = (z - firstEnd) / std::max(1e-6f, (f - firstEnd));
			const float uLog = (log2f(std::max(z / std::max(firstEnd, 1e-6f), 1.0f)) / std::max(log2f(std::max(f / std::max(firstEnd, 1e-6f), 1.0f)), 1e-6f));
			const float u = std::lerp(uLinear, uLog, std::clamp(_grid.zLogFactor, 0.0f, 1.0f));
			// Shader: 1 + u * (slices-1)
			const float sliceF = 1.0f + u * float(std::max<int>(1, int(slices) - 1));
			return std::clamp(floorf(sliceF), 0.0f, float(slices - 1));
		}

		const float uLinear = (z - n) / std::max(1e-6f, (f - n));
		const float uLog    = (log2f(std::max(z / std::max(n, 1e-6f), 1.0f)) /
							std::max(log2f(std::max(f / std::max(n, 1e-6f), 1.0f)), 1e-6f));
		const float u = std::lerp(uLinear, uLog, std::clamp(_grid.zLogFactor, 0.0f, 1.0f));
		// Shader: u * slices
		const float sliceF = u * float(slices);
		return std::clamp(floorf(sliceF), 0.0f, float(slices - 1));
	}

	void LightManager::BuildClusters(Camera *camera)
	{
		uint32 clusterCount = ComputeClusterCount();

		// Collect view/projection for parent camera
		const Array *mv = camera->GetMultiviewCameras();

		// Parent/head view & proj; collect per-eye matrices when multiview is present
		Matrix parentView = camera->GetViewMatrix();
		Matrix parentProj = camera->GetProjectionMatrix();
		std::vector<Matrix> views;
		std::vector<Matrix> projs;
		std::vector<Matrix> viewProjs;
		if(mv && mv->GetCount() > 0)
		{
			for(size_t i = 0; i < mv->GetCount(); ++i)
			{
				Camera *eye = mv->GetObjectAtIndex<Camera>(i);
				if(!eye) continue;
				Matrix v = eye->GetViewMatrix();
				Matrix p = eye->GetProjectionMatrix();
				views.push_back(v);
				projs.push_back(p);
				viewProjs.push_back(p * v);
			}
		}
		else
		{
			views.push_back(parentView);
			projs.push_back(parentProj);
			viewProjs.push_back(parentProj * parentView);
		}
		const size_t viewCount = views.size();
		std::vector<float> projAbsX(viewCount);
		std::vector<float> projAbsY(viewCount);
		for(size_t vi = 0; vi < viewCount; ++vi)
		{
			projAbsX[vi] = std::abs(projs[vi].m[0]);
			projAbsY[vi] = std::abs(projs[vi].m[5]);
		}

		_lastClipNear = camera->GetClipNear();
		_lastClipFar = camera->GetClipFar();
		const float zNear = _lastClipNear;
		const float zFar = _lastClipFar;

		Vector2 framebufferSize = camera->GetRenderPass()->GetFrame().GetSize();
		_lastViewportWidth = framebufferSize.x;
		_lastViewportHeight = framebufferSize.y;
		uint32 tilesX = _grid.clustersX;
		uint32 tilesY = _grid.clustersY;
		// Inflate projected bounds by half a tile in NDC to reduce precision/quantization under-coverage.
		const float ndcPadX = 1.0f / float(std::max(1u, tilesX));
		const float ndcPadY = 1.0f / float(std::max(1u, tilesY));

		auto screenToCluster = [&](float ndcX, float ndcY, uint32 &cx, uint32 &cy)
		{
			// Map NDC [-1,1] to pixel coords with bottom-left origin to match shader formula
			float sx = (ndcX * 0.5f + 0.5f) * framebufferSize.x; // [0..W]
			float syBottomLeft = (ndcY * 0.5f + 0.5f) * framebufferSize.y; // [0..H], y up from bottom
			cx = std::clamp(uint32(sx * tilesX / std::max(1.0f, framebufferSize.x)), 0u, tilesX - 1u);
			cy = std::clamp(uint32(syBottomLeft * tilesY / std::max(1.0f, framebufferSize.y)), 0u, tilesY - 1u);
		};

		// Reuse scratch buffers to avoid per-frame per-cluster allocations.
		const size_t perClusterCapacity = static_cast<size_t>(_maxLightsPerCluster);
		const size_t totalScratchCapacity = static_cast<size_t>(clusterCount) * perClusterCapacity;
		_clusterPointScratch.resize(totalScratchCapacity);
		_clusterSpotScratch.resize(totalScratchCapacity);
		_clusterPointCountsScratch.assign(clusterCount, 0);
		_clusterSpotCountsScratch.assign(clusterCount, 0);
		_clusterOffsetsScratch.resize(clusterCount);
		_clusterLightIndices.clear();

		// Second pass: actually fill using separate write cursors
		for(uint32 li = 0; li < _packedPointLights.size(); ++li)
		{
			const PointLightPacked &pl = _packedPointLights[li];
			const uint16_t lightIndex = static_cast<uint16_t>(std::min<uint32>(li, 0xffffu));

			Vector3 position(pl.positionRange.x, pl.positionRange.y, pl.positionRange.z);
			float range = pl.positionRange.w;

			uint32 zMin, zMax;
			float minNdcX, minNdcY, maxNdcX, maxNdcY;
			float minZDepth = zFar;
			float maxZDepth = zNear;
			bool hadProjected = false;
			minNdcX =  1.0f; minNdcY =  1.0f; maxNdcX = -1.0f; maxNdcY = -1.0f;
			for(size_t vi = 0; vi < viewCount; ++vi)
			{
				Vector4 centerVSv = views[vi] * Vector4(position, 1.0f);
				float depthv = -centerVSv.z;
				float minZv = std::clamp(depthv - range, zNear, zFar);
				float maxZv = std::clamp(depthv + range, zNear, zFar);
				minZDepth = std::min(minZDepth, minZv);
				maxZDepth = std::max(maxZDepth, maxZv);

				Vector4 centerCSv = viewProjs[vi] * Vector4(position, 1.0f);
				if(centerCSv.w > 0.0f && depthv > 1e-6f)
				{
					float centerNdcXv = centerCSv.x / centerCSv.w;
					float centerNdcYv = centerCSv.y / centerCSv.w;
					float denom = std::max(depthv*depthv - range*range, 1e-6f);
					float rSil = range / std::sqrt(denom);
					float rNdcXv = projAbsX[vi] * rSil;
					float rNdcYv = projAbsY[vi] * rSil;
					float ndcMinXv = std::max(-1.0f, centerNdcXv - rNdcXv);
					float ndcMaxXv = std::min( 1.0f, centerNdcXv + rNdcXv);
					float ndcMinYv = std::max(-1.0f, centerNdcYv - rNdcYv);
					float ndcMaxYv = std::min( 1.0f, centerNdcYv + rNdcYv);
					minNdcX = std::min(minNdcX, ndcMinXv);
					maxNdcX = std::max(maxNdcX, ndcMaxXv);
					minNdcY = std::min(minNdcY, ndcMinYv);
					maxNdcY = std::max(maxNdcY, ndcMaxYv);
					hadProjected = true;
				}
			}
			if(!hadProjected)
			{
				minNdcX = -1.0f; maxNdcX = 1.0f; minNdcY = -1.0f; maxNdcY = 1.0f;
			}
			minNdcX = std::max(-1.0f, minNdcX - ndcPadX);
			maxNdcX = std::min( 1.0f, maxNdcX + ndcPadX);
			minNdcY = std::max(-1.0f, minNdcY - ndcPadY);
			maxNdcY = std::min( 1.0f, maxNdcY + ndcPadY);
			zMin = uint32(ComputeZSlice(camera, minZDepth));
			zMax = uint32(ComputeZSlice(camera, maxZDepth));
			if(zMax < zMin) std::swap(zMin, zMax);

			uint32 x0, y0, x1, y1;
			screenToCluster(minNdcX, minNdcY, x0, y0);
			screenToCluster(maxNdcX, maxNdcY, x1, y1);
			if(x1 < x0) std::swap(x0, x1);
			if(y1 < y0) std::swap(y0, y1);

			for(uint32 z = zMin; z <= zMax; ++z)
			{
				for(uint32 y = y0; y <= y1; ++y)
				{
					for(uint32 x = x0; x <= x1; ++x)
					{
						uint32 idx = EncodeClusterIndex(x, y, z);
						uint8_t &count = _clusterPointCountsScratch[idx];
						if(count < _maxLightsPerCluster)
						{
							const size_t writeIndex = static_cast<size_t>(idx) * perClusterCapacity + count;
							_clusterPointScratch[writeIndex] = lightIndex;
							++count;
						}
					}
				}
			}
		}

		for(uint32 li = 0; li < _packedSpotLights.size(); ++li)
		{
			const SpotLightPacked &pl = _packedSpotLights[li];
			const uint16_t lightIndex = static_cast<uint16_t>(std::min<uint32>(li, 0xffffu));
			Vector3 position(pl.positionRange.x, pl.positionRange.y, pl.positionRange.z);
			float range = pl.positionRange.w;

			uint32 zMin, zMax;
			float minNdcX, minNdcY, maxNdcX, maxNdcY;
			float minZDepth = zFar;
			float maxZDepth = zNear;
			bool hadProjected = false;
			minNdcX =  1.0f; minNdcY =  1.0f; maxNdcX = -1.0f; maxNdcY = -1.0f;
			for(size_t vi = 0; vi < viewCount; ++vi)
			{
				Vector4 centerVSv = views[vi] * Vector4(position, 1.0f);
				float depthv = -centerVSv.z;
				float minZv = std::clamp(depthv - range, zNear, zFar);
				float maxZv = std::clamp(depthv + range, zNear, zFar);
				minZDepth = std::min(minZDepth, minZv);
				maxZDepth = std::max(maxZDepth, maxZv);

				Vector4 centerCSv = viewProjs[vi] * Vector4(position, 1.0f);
				if(centerCSv.w > 0.0f && depthv > 1e-6f)
				{
					float centerNdcXv = centerCSv.x / centerCSv.w;
					float centerNdcYv = centerCSv.y / centerCSv.w;
					float dFront = std::max(zNear, depthv - range);
					float rcpFront = 1.0f / std::max(1e-6f, dFront);
					float rNdcXv = projAbsX[vi] * range * rcpFront;
					float rNdcYv = projAbsY[vi] * range * rcpFront;
					float ndcMinXv = std::max(-1.0f, centerNdcXv - rNdcXv);
					float ndcMaxXv = std::min( 1.0f, centerNdcXv + rNdcXv);
					float ndcMinYv = std::max(-1.0f, centerNdcYv - rNdcYv);
					float ndcMaxYv = std::min( 1.0f, centerNdcYv + rNdcYv);
					minNdcX = std::min(minNdcX, ndcMinXv);
					maxNdcX = std::max(maxNdcX, ndcMaxXv);
					minNdcY = std::min(minNdcY, ndcMinYv);
					maxNdcY = std::max(maxNdcY, ndcMaxYv);
					hadProjected = true;
				}
			}
			if(!hadProjected)
			{
				minNdcX = -1.0f; maxNdcX = 1.0f; minNdcY = -1.0f; maxNdcY = 1.0f;
			}
			minNdcX = std::max(-1.0f, minNdcX - ndcPadX);
			maxNdcX = std::min( 1.0f, maxNdcX + ndcPadX);
			minNdcY = std::max(-1.0f, minNdcY - ndcPadY);
			maxNdcY = std::min( 1.0f, maxNdcY + ndcPadY);
			zMin = uint32(ComputeZSlice(camera, minZDepth));
			zMax = uint32(ComputeZSlice(camera, maxZDepth));
			if(zMax < zMin) std::swap(zMin, zMax);

			uint32 x0, y0, x1, y1;
			screenToCluster(minNdcX, minNdcY, x0, y0);
			screenToCluster(maxNdcX, maxNdcY, x1, y1);
			if(x1 < x0) std::swap(x0, x1);
			if(y1 < y0) std::swap(y0, y1);

			for(uint32 z = zMin; z <= zMax; ++z)
			{
				for(uint32 y = y0; y <= y1; ++y)
				{
					for(uint32 x = x0; x <= x1; ++x)
					{
						uint32 idx = EncodeClusterIndex(x, y, z);
						uint8_t &count = _clusterSpotCountsScratch[idx];
						if(count < _maxLightsPerCluster)
						{
							const size_t writeIndex = static_cast<size_t>(idx) * perClusterCapacity + count;
							_clusterSpotScratch[writeIndex] = lightIndex;
							++count;
						}
					}
				}
			}
		}

		// Build per-cluster counts and offsets
		uint32 totalCount = 0;
		for(uint32 i = 0; i < clusterCount; ++i)
		{
			const uint8_t pcount = _clusterPointCountsScratch[i];
			const uint8_t scount = _clusterSpotCountsScratch[i];
			_clusterOffsetsScratch[i] = totalCount;
			totalCount += static_cast<uint32>(pcount) + static_cast<uint32>(scount);
		}

		// Flatten indices in cluster order
		_clusterLightIndices.assign(totalCount, 0);
		for(uint32 i = 0, offset = 0; i < clusterCount; ++i)
		{
			const uint8_t pcount = _clusterPointCountsScratch[i];
			const uint8_t scount = _clusterSpotCountsScratch[i];
			if(pcount)
			{
				const size_t src = static_cast<size_t>(i) * perClusterCapacity;
				memcpy(_clusterLightIndices.data() + offset, _clusterPointScratch.data() + src, static_cast<size_t>(pcount) * sizeof(uint16_t));
				offset += pcount;
			}
			if(scount)
			{
				const size_t src = static_cast<size_t>(i) * perClusterCapacity;
				memcpy(_clusterLightIndices.data() + offset, _clusterSpotScratch.data() + src, static_cast<size_t>(scount) * sizeof(uint16_t));
				offset += scount;
			}
		}

		// Pack records for groups of 6 clusters: 1 base offset + 6 pairs of counts
		uint32 groupCount = (clusterCount + 5u) / 6u;
		_clusterRecords.resize(groupCount);
		for(uint32 g = 0; g < groupCount; ++g)
		{
			uint32 base = g * 6u;
			ClusterRecord rec{};
			rec.offset = (base < clusterCount) ? _clusterOffsetsScratch[base] : 0u;
			auto packPair = [&](uint32 idxInGroup) -> uint32 {
				uint32 ci = base + idxInGroup;
				uint32 p = (ci < clusterCount) ? _clusterPointCountsScratch[ci] : 0u;
				uint32 s = (ci < clusterCount) ? _clusterSpotCountsScratch[ci] : 0u;
				return (p & 0xffu) | ((s & 0xffu) << 8);
			};
			rec.counts01 = packPair(0) | (packPair(1) << 16);
			rec.counts23 = packPair(2) | (packPair(3) << 16);
			rec.counts45 = packPair(4) | (packPair(5) << 16);
			_clusterRecords[g] = rec;
		}
	}

	void LightManager::UploadBuffers()
	{
		const uint32 kLightsPointMax = 256; // must match Base.hlsl
		const uint32 kLightsSpotMax  = 256; // must match Base.hlsl
		size_t pointBytes = std::max<size_t>(_packedPointLights.size(), kLightsPointMax) * sizeof(PointLightPacked);
		size_t spotBytes = std::max<size_t>(_packedSpotLights.size(),  kLightsSpotMax)  * sizeof(SpotLightPacked);
		size_t indexBytes = _clusterLightIndices.size() * sizeof(uint16);
		size_t headerBytes = sizeof(ClusterGridInfo);
		size_t recordsBytes = _clusterRecords.size() * sizeof(ClusterRecord);

		// Ensure buffers are large enough using unified preallocation path
		uint16_t maxPointPerCluster = 1;
		uint16_t maxSpotPerCluster = 1;
		for(const ClusterRecord &rec : _clusterRecords)
		{
			uint32 pairs[6] = {
				rec.counts01 & 0xffffu,
				rec.counts01 >> 16,
				rec.counts23 & 0xffffu,
				rec.counts23 >> 16,
				rec.counts45 & 0xffffu,
				rec.counts45 >> 16
			};
			for(int j = 0; j < 6; ++j)
			{
				uint16_t pair = static_cast<uint16_t>(pairs[j]);
				uint16_t p = static_cast<uint16_t>(pair & 0xffu);
				uint16_t s = static_cast<uint16_t>((pair >> 8) & 0xffu);
				if(p > maxPointPerCluster) maxPointPerCluster = p;
				if(s > maxSpotPerCluster)  maxSpotPerCluster = s;
			}
		}
		maxPointPerCluster = std::max<uint16_t>(1, std::min<uint16_t>(maxPointPerCluster, _maxLightsPerCluster));
		maxSpotPerCluster = std::max<uint16_t>(1, std::min<uint16_t>(maxSpotPerCluster, _maxLightsPerCluster));
		
		PreallocateBuffers(static_cast<uint32>(_packedPointLights.size()), static_cast<uint32>(_packedSpotLights.size()), maxPointPerCluster, maxSpotPerCluster);

		if(pointBytes > 0)
		{
			void *dst = _pointLightBuffer->GetBuffer();
			// Copy actual lights first; leave the rest undefined
			size_t used = _packedPointLights.size() * sizeof(PointLightPacked);
			if(used > 0) memcpy(dst, _packedPointLights.data(), used);
			_pointLightBuffer->FlushRange(Range(0, pointBytes));
		}

		if(spotBytes > 0)
		{
			void *dst = _spotLightBuffer->GetBuffer();
			size_t used = _packedSpotLights.size() * sizeof(SpotLightPacked);
			if(used > 0) memcpy(dst, _packedSpotLights.data(), used);
			_spotLightBuffer->FlushRange(Range(0, spotBytes));
		}

		if(indexBytes > 0)
		{
			void *dst = _clusterIndexBuffer->GetBuffer();
			memcpy(dst, _clusterLightIndices.data(), indexBytes);
			_clusterIndexBuffer->FlushRange(Range(0, indexBytes));
		}

		if(recordsBytes > 0)
		{
			void *dst = _clusterRecordsBuffer->GetBuffer();
			// Write header
			_grid.clipNear = _lastClipNear;
			_grid.clipFar = _lastClipFar;
			_grid.viewportWidth = _lastViewportWidth;
			_grid.viewportHeight = _lastViewportHeight;
			_grid.padFloat0 = 0.0f;
			_grid.padFloat1 = 0.0f;
			memcpy(dst, &_grid, headerBytes);
			memcpy(static_cast<uint8*>(dst) + headerBytes, _clusterRecords.data(), recordsBytes);
			_clusterRecordsBuffer->FlushRange(Range(0, headerBytes + recordsBytes));
		}
	}
}
