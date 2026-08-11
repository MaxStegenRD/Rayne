//
//  RNOcclusionCuller.cpp
//  Rayne
//
//  Copyright 2025 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNOcclusionCuller.h"

#define OCCLUSION_DEPTH_BIAS 0.000001f

namespace RN
{
	static RN_INLINE float edgeFunction(const Vector2 a, const Vector2 b, const Vector2 c)
	{
		return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
	}

	OcclusionCuller::OcclusionCuller(uint16 width, uint16 height) : _width(width), _height(height), _depthBuffer(nullptr)
	{
		_depthBuffer = new float[_width * _height];
		Clear();
	}

	OcclusionCuller::~OcclusionCuller()
	{
		delete[] _depthBuffer;
	}

	void OcclusionCuller::Clear()
	{
		std::fill(_depthBuffer, _depthBuffer + _width * _height, 0.0f);
	}

	void OcclusionCuller::CopyDepthBufferForVisualization(float *target, float depthScaleFactor) const
	{
		if(!target) return;

		const size_t pixelCount = static_cast<size_t>(_width) * static_cast<size_t>(_height);
		for(size_t i = 0; i < pixelCount; i++)
		{
			target[i] = _depthBuffer[i] * depthScaleFactor;
		}
	}

	bool OcclusionCuller::TestBoundingBox(const Matrix &matViewProj, const AABB &aabb, const Vector2 &screenPixelSize) const
	{
		Vector4 boxCorners[8];

		const Vector4 position(Vector3(aabb.position), 1.0f);
		boxCorners[0] = position + Vector4(aabb.maxExtend.x, aabb.maxExtend.y, aabb.maxExtend.z, 0.0f);
		boxCorners[1] = position + Vector4(aabb.maxExtend.x, aabb.maxExtend.y, aabb.minExtend.z, 0.0f);
		boxCorners[2] = position + Vector4(aabb.maxExtend.x, aabb.minExtend.y, aabb.minExtend.z, 0.0f);
		boxCorners[3] = position + Vector4(aabb.maxExtend.x, aabb.minExtend.y, aabb.maxExtend.z, 0.0f);
		boxCorners[4] = position + Vector4(aabb.minExtend.x, aabb.maxExtend.y, aabb.maxExtend.z, 0.0f);
		boxCorners[5] = position + Vector4(aabb.minExtend.x, aabb.maxExtend.y, aabb.minExtend.z, 0.0f);
		boxCorners[6] = position + Vector4(aabb.minExtend.x, aabb.minExtend.y, aabb.minExtend.z, 0.0f);
		boxCorners[7] = position + Vector4(aabb.minExtend.x, aabb.minExtend.y, aabb.maxExtend.z, 0.0f);

		Vector3 maxCorners;
		Vector3 minCorners;

		for(int i = 0; i < 8; i++)
		{
			boxCorners[i] = matViewProj * boxCorners[i];

			if(boxCorners[i].z > boxCorners[i].w)
			{
				return true; // Intersects near plane, assume visible
			}

			boxCorners[i] /= boxCorners[i].w;
			boxCorners[i].x = boxCorners[i].x * 0.5f + 0.5f;
			boxCorners[i].y = boxCorners[i].y * 0.5f + 0.5f;

			if(i == 0)
			{
				maxCorners = minCorners = Vector3(boxCorners[i]);
			}
			else
			{
				maxCorners.x = std::max(maxCorners.x, boxCorners[i].x);
				maxCorners.y = std::max(maxCorners.y, boxCorners[i].y);
				maxCorners.z = std::max(maxCorners.z, boxCorners[i].z);

				minCorners.x = std::min(minCorners.x, boxCorners[i].x);
				minCorners.y = std::min(minCorners.y, boxCorners[i].y);
			}
		}

		if((maxCorners.x - minCorners.x) < screenPixelSize.x && (maxCorners.y - minCorners.y) < screenPixelSize.y)
		{
			return false;
		}

		minCorners.x *= _width;
		minCorners.y *= _height;

		maxCorners.x *= _width;
		maxCorners.y *= _height;

		uint16 minX = std::max(std::min(std::floor(minCorners.x) - 1.0f, static_cast<float>(_width - 1)), 0.0f);
		uint16 maxX = std::max(std::min(std::ceil(maxCorners.x) + 1.0f, static_cast<float>(_width - 1)), 0.0f);

		uint16 minY = std::max(std::min(std::floor(minCorners.y) - 1.0f, static_cast<float>(_height - 1)), 0.0f);
		uint16 maxY = std::max(std::min(std::ceil(maxCorners.y) + 1.0f, static_cast<float>(_height - 1)), 0.0f);

		for(uint16 y = minY; y <= maxY; y++)
		{
			for(uint16 x = minX; x <= maxX; x++)
			{
				if(maxCorners.z > _depthBuffer[_width * (_height - y - 1) + x])
				{
					return true;
				}
			}
		}

		return false;
	}

	void OcclusionCuller::RasterizeClipSpaceTriangle(Vector4 A, Vector4 B, Vector4 C)
	{
		A /= A.w;
		B /= B.w;
		C /= C.w;
		if(edgeFunction(Vector2(A), Vector2(B), Vector2(C)) < 0) return;

		A.x = A.x * 0.5f + 0.5f; A.x *= _width;
		A.y = A.y * 0.5f + 0.5f; A.y *= _height;
		B.x = B.x * 0.5f + 0.5f; B.x *= _width;
		B.y = B.y * 0.5f + 0.5f; B.y *= _height;
		C.x = C.x * 0.5f + 0.5f; C.x *= _width;
		C.y = C.y * 0.5f + 0.5f; C.y *= _height;

		uint16 minX = std::min(std::max(std::min(A.x, std::min(B.x, C.x)), 0.0f), static_cast<float>(_width - 1));
		uint16 minY = std::min(std::max(std::min(A.y, std::min(B.y, C.y)), 0.0f), static_cast<float>(_height - 1));
		uint16 maxX = std::min(std::max(std::max(A.x, std::max(B.x, C.x)), 0.0f), static_cast<float>(_width - 1));
		uint16 maxY = std::min(std::max(std::max(A.y, std::max(B.y, C.y)), 0.0f), static_cast<float>(_height - 1));

		float area = edgeFunction(Vector2(A), Vector2(B), Vector2(C));
		float inverseArea = 1.0f / area;

		Vector2 point(minX, minY);
		float rowW0 = edgeFunction(Vector2(B), Vector2(C), point);
		float rowW1 = edgeFunction(Vector2(C), Vector2(A), point);
		float rowW2 = edgeFunction(Vector2(A), Vector2(B), point);

		float w0StepX = C.y - B.y;
		float w1StepX = A.y - C.y;
		float w2StepX = B.y - A.y;

		float w0StepY = B.x - C.x;
		float w1StepY = C.x - A.x;
		float w2StepY = A.x - B.x;

		float rowDepthNumerator = rowW0 * A.z + rowW1 * B.z + rowW2 * C.z;
		float depthNumeratorStepX = w0StepX * A.z + w1StepX * B.z + w2StepX * C.z;
		float depthNumeratorStepY = w0StepY * A.z + w1StepY * B.z + w2StepY * C.z;

		for(uint16 y = minY; y <= maxY; y++)
		{
			float *depthBufferRow = &_depthBuffer[_width * (_height - y - 1)];
			float w0 = rowW0;
			float w1 = rowW1;
			float w2 = rowW2;
			float depthNumerator = rowDepthNumerator;
			for(uint16 x = minX; x <= maxX; x++)
			{
				if(w0 > 0 && w1 >= 0 && w2 >= 0)
				{
					float depth = depthNumerator * inverseArea - OCCLUSION_DEPTH_BIAS;
					if(depth <= 1.0f)
					{
						if(depth > depthBufferRow[x]) depthBufferRow[x] = depth;
					}
				}

				w0 += w0StepX;
				w1 += w1StepX;
				w2 += w2StepX;
				depthNumerator += depthNumeratorStepX;
			}

			rowW0 += w0StepY;
			rowW1 += w1StepY;
			rowW2 += w2StepY;
			rowDepthNumerator += depthNumeratorStepY;
		}
	}

	void OcclusionCuller::RasterizeMesh(const Matrix &matModelViewProj, Mesh *mesh)
	{
		RN::Mesh::Chunk chunk = mesh->GetTrianglesChunk();
		Mesh::ElementIterator<Vector3> iterator = chunk.GetIterator<Vector3>(Mesh::VertexAttribute::Feature::Vertices);
		size_t triangleCount = mesh->GetIndicesCount() / 3;
		for(size_t i = 0; i < triangleCount; i++)
		{
			const Vector3 &posA = *iterator++;
			const Vector3 &posB = *iterator++;
			const Vector3 &posC = *iterator; if(i < triangleCount - 1) { iterator++; }

			Vector4 A = matModelViewProj * Vector4(posB, 1.0f);
			Vector4 B = matModelViewProj * Vector4(posA, 1.0f);
			Vector4 C = matModelViewProj * Vector4(posC, 1.0f);

			if(A.z < -A.w && B.z < -B.w && C.z < -C.w) continue;
			if(A.z > A.w && B.z > B.w && C.z > C.w) continue;

			if(A.z > A.w && B.z > B.w)
			{
				float dA = (A.z - (A.w)); float dB = (B.z - (B.w)); float dC = (C.z - (C.w));
				float tA = dA / (dA - dC); float tB = dB / (dB - dC);
				A = A + (C - A) * tA; B = B + (C - B) * tB;
			}
			else if(A.z > A.w && C.z > C.w)
			{
				float dA = (A.z - (A.w)); float dB = (B.z - (B.w)); float dC = (C.z - (C.w));
				float tA = dA / (dA - dB); float tC = dC / (dC - dB);
				A = A + (B - A) * tA; C = C + (B - C) * tC;
			}
			else if(B.z > B.w && C.z > C.w)
			{
				float dA = (A.z - (A.w)); float dB = (B.z - (B.w)); float dC = (C.z - (C.w));
				float tB = dB / (dB - dA); float tC = dC / (dC - dA);
				B = B + (A - B) * tB; C = C + (A - C) * tC;
			}
			else if(A.z > A.w)
			{
				float dA = (A.z - (A.w)); float dB = (B.z - (B.w)); float dC = (C.z - (C.w));
				float tB = dA / (dA - dB); float tC = dA / (dA - dC);
				Vector4 AB = A + (B - A) * tB; Vector4 AC = A + (C - A) * tC; A = AB;
				RasterizeClipSpaceTriangle(C, AC, AB);
			}
			else if(B.z > B.w)
			{
				float dA = (A.z - (A.w)); float dB = (B.z - (B.w)); float dC = (C.z - (C.w));
				float tA = dB / (dB - dA); float tC = dB / (dB - dC); Vector4 BA = B + (A - B) * tA; Vector4 BC = B + (C - B) * tC; B = BA;
				RasterizeClipSpaceTriangle(C, BA, BC);
			}
			else if(C.z > C.w)
			{
				float dA = (A.z - (A.w)); float dB = (B.z - (B.w)); float dC = (C.z - (C.w));
				float tA = dC / (dC - dA); float tB = dC / (dC - dB); Vector4 CA = C + (A - C) * tA; Vector4 CB = C + (B - C) * tB; C = CA;
				RasterizeClipSpaceTriangle(B, CB, CA);
			}

			RasterizeClipSpaceTriangle(A, B, C);
		}
	}
}
