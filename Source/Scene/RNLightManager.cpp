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
	namespace
	{
		constexpr uint16_t kLightManagerShaderMaxPointLights = 512;
		constexpr uint16_t kLightManagerShaderMaxSpotLights = 512;
	}

	RNDefineMeta(LightManager, Object)

	LightManager::LightManager(uint32 x, uint32 y, uint32 z, float zLogFactor, uint16_t maxPackedPointLights, uint16_t maxPackedSpotLights) :
		_pointLightBuffer(nullptr),
		_spotLightBuffer(nullptr),
		_clusterIndexBuffer(nullptr),
		_clusterRecordsBuffer(nullptr),
		_lastViewportWidth(0.0f),
		_lastViewportHeight(0.0f),
		_lastClipNear(0.0f),
		_lastClipFar(0.0f),
		_hasSpotClusterBoundsCache(false),
		_maxLightsPerCluster(255),
		_maxPackedPointLights(std::max<uint16_t>(1, std::min<uint16_t>(maxPackedPointLights, kLightManagerShaderMaxPointLights))),
		_maxPackedSpotLights(std::max<uint16_t>(1, std::min<uint16_t>(maxPackedSpotLights, kLightManagerShaderMaxSpotLights)))
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
		_hasSpotClusterBoundsCache = false;

		// Ensure buffers for new cluster count are reasonably pre-sized
		PreallocateBuffers(256, 128, 16, 8);
	}

	void LightManager::PreallocateBuffers(uint32 pointEstimate, uint32 spotEstimate, uint16 pointPerClusterEstimate, uint16 spotPerClusterEstimate)
	{
		uint32 clusterCount = ComputeClusterCount();
		pointPerClusterEstimate = std::max<uint16_t>(1, std::min<uint16_t>(pointPerClusterEstimate, _maxLightsPerCluster));
		spotPerClusterEstimate = std::max<uint16_t>(1, std::min<uint16_t>(spotPerClusterEstimate, _maxLightsPerCluster));

		// Metal requires uniform buffers to be at least as large as the declared array sizes in HLSL.
		size_t pointBytes = std::max<size_t>(pointEstimate, kLightManagerShaderMaxPointLights) * sizeof(PointLightPacked);
		size_t spotBytes  = std::max<size_t>(spotEstimate,  kLightManagerShaderMaxSpotLights)  * sizeof(SpotLightPacked);
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
		_hasSpotClusterBoundsCache = false;
	}

	void LightManager::SetZFirstSliceDepth(float meters)
	{
		_grid.zFirstSliceDepth = std::max(0.0f, meters);
		_hasSpotClusterBoundsCache = false;
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

	void LightManager::SetMaxPackedLights(uint16_t maxPointLights, uint16_t maxSpotLights)
	{
		const uint16_t clampedPoint = std::max<uint16_t>(1, std::min<uint16_t>(maxPointLights, kLightManagerShaderMaxPointLights));
		const uint16_t clampedSpot = std::max<uint16_t>(1, std::min<uint16_t>(maxSpotLights, kLightManagerShaderMaxSpotLights));
		_maxPackedPointLights = clampedPoint;
		_maxPackedSpotLights = clampedSpot;
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
		_spotLightCullData.clear();
		_clusterLightIndices.clear();
		_clusterRecords.clear();
	}

	void LightManager::PackLights(const std::vector<Light *> &lights)
	{
		_packedPointLights.reserve(lights.size());
		_packedSpotLights.reserve(lights.size());
		_spotLightCullData.reserve(lights.size());
		for(const Light *light : lights)
		{
			Light::Type type = light->GetType();
			if(type == Light::Type::DirectionalLight) continue; // not clustered

			if(type == Light::Type::SpotLight)
			{
				if(_packedSpotLights.size() >= _maxPackedSpotLights) continue;
				Vector3 pos = light->GetWorldPosition();
				Vector3 dir = light->GetForward();
				SpotLightPacked out;
				out.positionRange = Vector4(pos.x, pos.y, pos.z, light->GetRange());
				out.color = light->GetFinalColor();
				out.dirCos = Vector4(dir.x, dir.y, dir.z, light->GetAngleCos());
				_packedSpotLights.push_back(out);

				const Sphere cullSphere = light->GetBoundingSphere();
				SpotLightCullData cullData;
				cullData.center = cullSphere.position + cullSphere.offset;
				cullData.radius = cullSphere.radius;
				cullData.forward = dir;
				cullData.tanHalfAngle = light->GetTanHalfAngle();
				_spotLightCullData.push_back(cullData);
			}
			else // Point
			{
				if(_packedPointLights.size() >= _maxPackedPointLights) continue;
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
		std::vector<Vector3> viewPositions;
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
				viewPositions.push_back(eye->GetWorldPosition());
			}
		}
		else
		{
			views.push_back(parentView);
			projs.push_back(parentProj);
			viewProjs.push_back(parentProj * parentView);
			viewPositions.push_back(camera->GetWorldPosition());
		}
		const size_t viewCount = views.size();
		std::vector<float> projAbsX(viewCount);
		std::vector<float> projAbsY(viewCount);
		for(size_t vi = 0; vi < viewCount; ++vi)
		{
			projAbsX[vi] = std::abs(projs[vi].m[0]);
			projAbsY[vi] = std::abs(projs[vi].m[5]);
		}

		const float zNear = camera->GetClipNear();
		const float zFar = camera->GetClipFar();
		const bool clipPlanesMatch = Math::Compare(_lastClipNear, zNear) && Math::Compare(_lastClipFar, zFar);

		Vector2 framebufferSize = camera->GetRenderPass()->GetFrame().GetSize();
		_lastViewportWidth = framebufferSize.x;
		_lastViewportHeight = framebufferSize.y;
		uint32 tilesX = _grid.clustersX;
		uint32 tilesY = _grid.clustersY;
		uint32 tilesZ = _grid.clustersZ;
		// Inflate projected bounds by half a tile in NDC to reduce precision/quantization under-coverage.
		const float ndcPadX = 1.0f / float(std::max(1u, tilesX));
		const float ndcPadY = 1.0f / float(std::max(1u, tilesY));
		const float invTilesX = 1.0f / float(std::max(1u, tilesX));
		const float invTilesY = 1.0f / float(std::max(1u, tilesY));
		const std::vector<SpotClusterBound> *clusterConeBoundsByEye = nullptr;
		if(!_packedSpotLights.empty())
		{
			bool projectionsMatch = (_cachedSpotBoundsProjections.size() == viewCount);
			if(projectionsMatch)
			{
				for(size_t vi = 0; vi < viewCount; ++vi)
				{
					for(size_t mi = 0; mi < 16; ++mi)
					{
						if(!Math::Compare(_cachedSpotBoundsProjections[vi].m[mi], projs[vi].m[mi]))
						{
							projectionsMatch = false;
							break;
						}
					}
					if(!projectionsMatch) break;
				}
			}
			const bool canUseSpotBoundsCache =
				_hasSpotClusterBoundsCache &&
				clipPlanesMatch &&
				projectionsMatch;

			auto solveDepthFromU = [&](float nearDepth, float farDepth, float u) {
				u = std::clamp(u, 0.0f, 1.0f);
				const float logDenominator = std::max(log2f(std::max(farDepth / std::max(nearDepth, 1e-6f), 1.0f)), 1e-6f);
				float low = nearDepth;
				float high = farDepth;
				for(uint32 iteration = 0; iteration < 18; ++iteration)
				{
					float mid = 0.5f * (low + high);
					float uLinear = (mid - nearDepth) / std::max(1e-6f, (farDepth - nearDepth));
					float uLog = log2f(std::max(mid / std::max(nearDepth, 1e-6f), 1.0f)) / logDenominator;
					float mixedU = std::lerp(uLinear, uLog, std::clamp(_grid.zLogFactor, 0.0f, 1.0f));
					if(mixedU < u) low = mid;
					else high = mid;
				}
				return 0.5f * (low + high);
			};

			if(!canUseSpotBoundsCache)
			{
				std::vector<float> zSliceNearDepth(tilesZ);
				std::vector<float> zSliceFarDepth(tilesZ);
				if(_grid.zFirstSliceDepth > 0.0f)
				{
					const float firstEnd = std::min(zNear + _grid.zFirstSliceDepth, zFar);
					zSliceNearDepth[0] = zNear;
					zSliceFarDepth[0] = firstEnd;

					const float remainingNear = std::max(firstEnd, zNear + 1e-6f);
					if(tilesZ > 1)
					{
						const float remainingSlices = float(std::max(1u, tilesZ - 1u));
						for(uint32 z = 1; z < tilesZ; ++z)
						{
							const float u0 = float(z - 1u) / remainingSlices;
							const float u1 = float(z) / remainingSlices;
							zSliceNearDepth[z] = solveDepthFromU(remainingNear, zFar, u0);
							zSliceFarDepth[z] = solveDepthFromU(remainingNear, zFar, u1);
						}
					}
				}
				else
				{
					const float slicesF = float(std::max(1u, tilesZ));
					for(uint32 z = 0; z < tilesZ; ++z)
					{
						const float u0 = float(z) / slicesF;
						const float u1 = float(z + 1u) / slicesF;
						zSliceNearDepth[z] = solveDepthFromU(zNear, zFar, u0);
						zSliceFarDepth[z] = solveDepthFromU(zNear, zFar, u1);
					}
				}

				const uint32 cornerCols = tilesX + 1u;
				const uint32 cornerRows = tilesY + 1u;
				const size_t cornersPerEye = static_cast<size_t>(cornerCols) * static_cast<size_t>(cornerRows);
				std::vector<Vector3> tileCornerRays(static_cast<size_t>(viewCount) * cornersPerEye);
				auto cornerIndex = [&](size_t eye, uint32 cx, uint32 cy) -> size_t {
					return eye * cornersPerEye + static_cast<size_t>(cy) * static_cast<size_t>(cornerCols) + static_cast<size_t>(cx);
				};

				for(size_t vi = 0; vi < viewCount; ++vi)
				{
					const Matrix invProj = projs[vi].GetInverse();
					for(uint32 cy = 0; cy <= tilesY; ++cy)
					{
						const float ndcY = std::clamp(2.0f * (float(cy) * invTilesY) - 1.0f, -1.0f, 1.0f);
						for(uint32 cx = 0; cx <= tilesX; ++cx)
						{
							const float ndcX = std::clamp(2.0f * (float(cx) * invTilesX) - 1.0f, -1.0f, 1.0f);
							const Vector4 viewPoint4 = invProj * Vector4(ndcX, ndcY, 1.0f, 1.0f);
							const float invW = 1.0f / std::max(std::abs(viewPoint4.w), 1e-6f);
							Vector3 rayVS(viewPoint4.x * invW, viewPoint4.y * invW, viewPoint4.z * invW);
							const float invDepth = 1.0f / std::max(std::abs(rayVS.z), 1e-6f);
							rayVS *= invDepth;
							if(rayVS.z > 0.0f) rayVS *= -1.0f;
							tileCornerRays[cornerIndex(vi, cx, cy)] = rayVS;
						}
					}
				}

				_cachedSpotClusterBoundsByEye.resize(static_cast<size_t>(viewCount) * static_cast<size_t>(clusterCount));
				for(size_t vi = 0; vi < viewCount; ++vi)
				{
					const size_t eyeRayBase = vi * cornersPerEye;
					const size_t eyeClusterBase = vi * static_cast<size_t>(clusterCount);
					for(uint32 z = 0; z < tilesZ; ++z)
					{
						const float clusterDepthNear = zSliceNearDepth[z];
						const float clusterDepthFar = zSliceFarDepth[z];
						const uint32 zBase = z * tilesY * tilesX;
						for(uint32 y = 0; y < tilesY; ++y)
						{
							const uint32 yBase = zBase + y * tilesX;
							for(uint32 x = 0; x < tilesX; ++x)
							{
								const size_t corner00 = eyeRayBase + static_cast<size_t>(y) * static_cast<size_t>(cornerCols) + static_cast<size_t>(x);
								const size_t corner10 = eyeRayBase + static_cast<size_t>(y) * static_cast<size_t>(cornerCols) + static_cast<size_t>(x + 1u);
								const size_t corner01 = eyeRayBase + static_cast<size_t>(y + 1u) * static_cast<size_t>(cornerCols) + static_cast<size_t>(x);
								const size_t corner11 = eyeRayBase + static_cast<size_t>(y + 1u) * static_cast<size_t>(cornerCols) + static_cast<size_t>(x + 1u);

								const Vector3 &ray00 = tileCornerRays[corner00];
								const Vector3 &ray10 = tileCornerRays[corner10];
								const Vector3 &ray01 = tileCornerRays[corner01];
								const Vector3 &ray11 = tileCornerRays[corner11];

								const Vector3 near00 = ray00 * clusterDepthNear;
								const Vector3 near10 = ray10 * clusterDepthNear;
								const Vector3 near01 = ray01 * clusterDepthNear;
								const Vector3 near11 = ray11 * clusterDepthNear;
								const Vector3 far00 = ray00 * clusterDepthFar;
								const Vector3 far10 = ray10 * clusterDepthFar;
								const Vector3 far01 = ray01 * clusterDepthFar;
								const Vector3 far11 = ray11 * clusterDepthFar;

								float clusterMinX = std::min(std::min(near00.x, near10.x), std::min(near01.x, near11.x));
								clusterMinX = std::min(clusterMinX, std::min(std::min(far00.x, far10.x), std::min(far01.x, far11.x)));
								float clusterMaxX = std::max(std::max(near00.x, near10.x), std::max(near01.x, near11.x));
								clusterMaxX = std::max(clusterMaxX, std::max(std::max(far00.x, far10.x), std::max(far01.x, far11.x)));
								float clusterMinYEye = std::min(std::min(near00.y, near10.y), std::min(near01.y, near11.y));
								clusterMinYEye = std::min(clusterMinYEye, std::min(std::min(far00.y, far10.y), std::min(far01.y, far11.y)));
								float clusterMaxYEye = std::max(std::max(near00.y, near10.y), std::max(near01.y, near11.y));
								clusterMaxYEye = std::max(clusterMaxYEye, std::max(std::max(far00.y, far10.y), std::max(far01.y, far11.y)));
								float clusterMinZEye = std::min(std::min(near00.z, near10.z), std::min(near01.z, near11.z));
								clusterMinZEye = std::min(clusterMinZEye, std::min(std::min(far00.z, far10.z), std::min(far01.z, far11.z)));
								float clusterMaxZEye = std::max(std::max(near00.z, near10.z), std::max(near01.z, near11.z));
								clusterMaxZEye = std::max(clusterMaxZEye, std::max(std::max(far00.z, far10.z), std::max(far01.z, far11.z)));

								const Vector3 center((clusterMinX + clusterMaxX) * 0.5f, (clusterMinYEye + clusterMaxYEye) * 0.5f, (clusterMinZEye + clusterMaxZEye) * 0.5f);
								const Vector3 halfExtents((clusterMaxX - clusterMinX) * 0.5f, (clusterMaxYEye - clusterMinYEye) * 0.5f, (clusterMaxZEye - clusterMinZEye) * 0.5f);
								const uint32 clusterIndex = yBase + x;
								_cachedSpotClusterBoundsByEye[eyeClusterBase + static_cast<size_t>(clusterIndex)] = {center, halfExtents.GetLength()};
							}
						}
					}
				}

				_cachedSpotBoundsProjections = projs;
				_hasSpotClusterBoundsCache = true;
			}

			clusterConeBoundsByEye = &_cachedSpotClusterBoundsByEye;
		}
		_lastClipNear = zNear;
		_lastClipFar = zFar;

		auto screenToCluster = [&](float ndcX, float ndcY, uint32 &cx, uint32 &cy)
		{
			// Map NDC [-1,1] to pixel coords with bottom-left origin to match shader formula
			float sx = (ndcX * 0.5f + 0.5f) * framebufferSize.x; // [0..W]
			float syBottomLeft = (ndcY * 0.5f + 0.5f) * framebufferSize.y; // [0..H], y up from bottom
			cx = std::clamp(uint32(sx * tilesX / std::max(1.0f, framebufferSize.x)), 0u, tilesX - 1u);
			cy = std::clamp(uint32(syBottomLeft * tilesY / std::max(1.0f, framebufferSize.y)), 0u, tilesY - 1u);
		};
		struct ClusterSpan
		{
			uint32 zMin, zMax;
			uint32 x0, y0, x1, y1;
		};
		auto computeClusterSpan = [&](const Vector3 &center, float depthRadius, auto &&computeProjectedRadius) -> ClusterSpan {
			float minNdcX = 1.0f;
			float minNdcY = 1.0f;
			float maxNdcX = -1.0f;
			float maxNdcY = -1.0f;
			float minZDepth = zFar;
			float maxZDepth = zNear;
			bool hadProjected = false;
			for(size_t vi = 0; vi < viewCount; ++vi)
			{
				const Vector4 centerVSv = views[vi] * Vector4(center, 1.0f);
				const float depthv = -centerVSv.z;
				const float minZv = std::clamp(depthv - depthRadius, zNear, zFar);
				const float maxZv = std::clamp(depthv + depthRadius, zNear, zFar);
				minZDepth = std::min(minZDepth, minZv);
				maxZDepth = std::max(maxZDepth, maxZv);

				const Vector4 centerCSv = viewProjs[vi] * Vector4(center, 1.0f);
				if(centerCSv.w > 0.0f && depthv > 1e-6f)
				{
					const float centerNdcXv = centerCSv.x / centerCSv.w;
					const float centerNdcYv = centerCSv.y / centerCSv.w;
					float rNdcXv, rNdcYv;
					computeProjectedRadius(vi, depthv, rNdcXv, rNdcYv);
					const float ndcMinXv = std::max(-1.0f, centerNdcXv - rNdcXv);
					const float ndcMaxXv = std::min( 1.0f, centerNdcXv + rNdcXv);
					const float ndcMinYv = std::max(-1.0f, centerNdcYv - rNdcYv);
					const float ndcMaxYv = std::min( 1.0f, centerNdcYv + rNdcYv);
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

			ClusterSpan span{};
			span.zMin = uint32(ComputeZSlice(camera, minZDepth));
			span.zMax = uint32(ComputeZSlice(camera, maxZDepth));
			if(span.zMax < span.zMin) std::swap(span.zMin, span.zMax);

			screenToCluster(minNdcX, minNdcY, span.x0, span.y0);
			screenToCluster(maxNdcX, maxNdcY, span.x1, span.y1);
			if(span.x1 < span.x0) std::swap(span.x0, span.x1);
			if(span.y1 < span.y0) std::swap(span.y0, span.y1);
			return span;
		};
		auto computePointClusterSpan = [&](const Vector3 &position, float range) -> ClusterSpan {
			const float lightRadiusSq = range * range;
			bool cameraInsideLight = false;
			float maxInfluenceDepth = 0.0f;
			for(const Vector3 &viewPosition : viewPositions)
			{
				const float distanceToCamera = position.GetDistance(viewPosition);
				maxInfluenceDepth = std::max(maxInfluenceDepth, distanceToCamera + range);
				if(position.GetSquaredDistance(viewPosition) <= lightRadiusSq)
				{
					cameraInsideLight = true;
				}
			}

			if(cameraInsideLight)
			{
				// The tangent-silhouette formula below assumes the eye is outside the sphere.
				// If any eye is inside, the sphere subtends all screen directions.
				ClusterSpan span {};
				span.x0 = 0;
				span.y0 = 0;
				span.x1 = tilesX - 1;
				span.y1 = tilesY - 1;
				span.zMin = 0;
				const float clampedDepth = std::clamp(maxInfluenceDepth, zNear, zFar);
				span.zMax = uint32(ComputeZSlice(camera, clampedDepth));
				return span;
			}

			return computeClusterSpan(position, range, [&](size_t vi, float depthv, float &rNdcXv, float &rNdcYv) {
				const float denom = std::max(depthv * depthv - range * range, 1e-6f);
				const float rSil = range / std::sqrt(denom);
				rNdcXv = projAbsX[vi] * rSil;
				rNdcYv = projAbsY[vi] * rSil;
			});
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
		const uint32 sliceStride = tilesX * tilesY;
		const uint32 rowStride = tilesX;
		for(uint32 li = 0; li < _packedPointLights.size(); ++li)
		{
			const PointLightPacked &pl = _packedPointLights[li];
			const uint16_t lightIndex = static_cast<uint16_t>(std::min<uint32>(li, 0xffffu));

			const Vector3 position(pl.positionRange.x, pl.positionRange.y, pl.positionRange.z);
			const float range = pl.positionRange.w;
			const ClusterSpan span = computePointClusterSpan(position, range);

			for(uint32 z = span.zMin; z <= span.zMax; ++z)
			{
				const uint32 zBase = z * sliceStride;
				for(uint32 y = span.y0; y <= span.y1; ++y)
				{
					uint32 idx = zBase + y * rowStride + span.x0;
					for(uint32 x = span.x0; x <= span.x1; ++x, ++idx)
					{
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

		std::vector<Vector3> spotDirectionVSByEye(viewCount);
		std::vector<uint8_t> spotDirectionValidByEye(viewCount, 0);
		std::vector<Vector3> spotPositionVSByEye(viewCount);
		for(uint32 li = 0; li < _packedSpotLights.size(); ++li)
		{
			const SpotLightPacked &pl = _packedSpotLights[li];
			const uint16_t lightIndex = static_cast<uint16_t>(std::min<uint32>(li, 0xffffu));
			const Vector3 position(pl.positionRange.x, pl.positionRange.y, pl.positionRange.z);
			const float range = pl.positionRange.w;
			const SpotLightCullData &cullData = _spotLightCullData[li];
			const Vector3 directionWS = cullData.forward;
			const float spotTanHalfAngle = cullData.tanHalfAngle;
			const float spotSideExpandFactor = std::sqrt(1.0f + spotTanHalfAngle * spotTanHalfAngle);
			const Vector3 cullCenter = cullData.center;
			const float cullRadius = cullData.radius;
			const ClusterSpan span = computeClusterSpan(cullCenter, cullRadius, [&](size_t vi, float depthv, float &rNdcXv, float &rNdcYv) {
				const float denom = std::max(depthv * depthv - cullRadius * cullRadius, 1e-6f);
				const float rSil = cullRadius / std::sqrt(denom);
				rNdcXv = projAbsX[vi] * rSil;
				rNdcYv = projAbsY[vi] * rSil;
			});
			std::fill(spotDirectionValidByEye.begin(), spotDirectionValidByEye.end(), 0);
			for(size_t vi = 0; vi < viewCount; ++vi)
			{
				const Vector4 directionVS4 = views[vi] * Vector4(directionWS, 0.0f);
				Vector3 directionVS(directionVS4.x, directionVS4.y, directionVS4.z);
				const float dirLenSq = directionVS.GetSquaredLength();
				if(std::isfinite(dirLenSq) && dirLenSq > 1e-12f)
				{
					directionVS *= 1.0f / std::sqrt(dirLenSq);
					spotDirectionVSByEye[vi] = directionVS;
					spotDirectionValidByEye[vi] = 1;
				}
				const Vector4 positionVS4 = views[vi] * Vector4(position, 1.0f);
				spotPositionVSByEye[vi] = Vector3(positionVS4.x, positionVS4.y, positionVS4.z);
			}

			for(uint32 z = span.zMin; z <= span.zMax; ++z)
			{
				const uint32 zBase = z * sliceStride;
				for(uint32 y = span.y0; y <= span.y1; ++y)
				{
					uint32 idx = zBase + y * rowStride + span.x0;
					for(uint32 x = span.x0; x <= span.x1; ++x, ++idx)
					{
						bool passesAnyEye = false;
						for(size_t vi = 0; vi < viewCount; ++vi)
						{
							// Fail-open: if spotlight axis is invalid in this eye, keep this cluster.
							if(spotDirectionValidByEye[vi] == 0) { passesAnyEye = true; break; }

							const SpotClusterBound &clusterBounds = (*clusterConeBoundsByEye)[vi * static_cast<size_t>(clusterCount) + static_cast<size_t>(idx)];
							const Vector3 toCluster = clusterBounds.center - spotPositionVSByEye[vi];
							const float clusterRadius = clusterBounds.radius;

							const float axial = spotDirectionVSByEye[vi].GetDotProduct(toCluster);
							if(!std::isfinite(axial)) { passesAnyEye = true; break; }
							if(axial < -clusterRadius) continue;
							if(axial > range + clusterRadius) continue;

							const Vector3 radialVector = toCluster - (spotDirectionVSByEye[vi] * axial);
							const float radialSq = radialVector.GetSquaredLength();
							if(!std::isfinite(radialSq)) { passesAnyEye = true; break; }

							const float sideExpand = clusterRadius * spotSideExpandFactor;
							if(axial > range)
							{
								// End-cap reject: outside cone base radius (expanded by cluster sphere support term).
								const float capLimit = range * spotTanHalfAngle + sideExpand;
								const float capLimitSq = capLimit * capLimit;
								if(!std::isfinite(capLimitSq)) { passesAnyEye = true; break; }
								if(radialSq > capLimitSq) continue;
							}
							else
							{
								const float sideLimit = std::max(axial, 0.0f) * spotTanHalfAngle + sideExpand;
								const float sideLimitSq = sideLimit * sideLimit;
								if(!std::isfinite(sideLimitSq)) { passesAnyEye = true; break; }
								if(radialSq > sideLimitSq) continue;
							}

							passesAnyEye = true;
							break;
						}
						if(!passesAnyEye) continue;

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
		_clusterLightIndices.resize(totalCount);
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
		size_t pointBytes = std::max<size_t>(_packedPointLights.size(), kLightManagerShaderMaxPointLights) * sizeof(PointLightPacked);
		size_t spotBytes = std::max<size_t>(_packedSpotLights.size(),  kLightManagerShaderMaxSpotLights)  * sizeof(SpotLightPacked);
		size_t indexBytes = _clusterLightIndices.size() * sizeof(uint16);
		size_t headerBytes = sizeof(ClusterGridInfo);
		size_t recordsBytes = _clusterRecords.size() * sizeof(ClusterRecord);

		// Keep cluster index capacity stable across visibility changes to avoid transient realloc/release churn.
		PreallocateBuffers(static_cast<uint32>(_packedPointLights.size()), static_cast<uint32>(_packedSpotLights.size()), _maxLightsPerCluster, _maxLightsPerCluster);

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
			memcpy(static_cast<uint8 *>(dst) + headerBytes, _clusterRecords.data(), recordsBytes);
			_clusterRecordsBuffer->FlushRange(Range(0, headerBytes + recordsBytes));
		}
	}
}
