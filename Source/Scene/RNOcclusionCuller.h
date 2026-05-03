//
//  RNOcclusionCuller.h
//  Rayne
//
//  Copyright 2025 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_OCCLUSIONCULLER_H__
#define __RAYNE_OCCLUSIONCULLER_H__

#include "../Math/RNMatrix.h"
#include "../Math/RNAABB.h"
#include "../Math/RNVector.h"
#include "../Rendering/RNMesh.h"

namespace RN
{
	class OcclusionCuller
	{
	public:
		RNAPI OcclusionCuller(uint16 width, uint16 height);
		RNAPI ~OcclusionCuller();

		RNAPI void Clear();
		RNAPI bool TestBoundingBox(const Matrix &matViewProj, const AABB &aabb, const Vector2 &screenPixelSize) const;
		RNAPI void RasterizeMesh(const Matrix &matModelViewProj, Mesh *mesh);
		RNAPI void CopyDepthBufferForVisualization(float *target, float depthScaleFactor) const;

		RNAPI uint16 GetWidth() const { return _width; }
		RNAPI uint16 GetHeight() const { return _height; }

	private:
		void RasterizeClipSpaceTriangle(Vector4 A, Vector4 B, Vector4 C);

		uint16 _width;
		uint16 _height;
		float *_depthBuffer;
	};
}

#endif /* __RAYNE_OCCLUSIONCULLER_H__ */
