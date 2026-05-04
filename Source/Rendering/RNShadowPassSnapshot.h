//
//  RNShadowPassSnapshot.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_SHADOW_PASS_SNAPSHOT_H__
#define __RAYNE_SHADOW_PASS_SNAPSHOT_H__

#include "../Math/RNMatrix.h"
#include "../Math/RNVector.h"
#include "../Objects/RNObject.h"
#include "RNTexture.h"

namespace RN
{
	class ShadowPassSnapshot : public Object
	{
	public:
		RNAPI ShadowPassSnapshot(Texture *directionalShadowTexture, const std::vector<Matrix> &directionalShadowMatrices, const Vector2 &directionalShadowInfo, const std::vector<uint64> &shadowCameraUIDs);

		Texture *GetDirectionalShadowTexture() const { return _directionalShadowTexture.Get(); }
		const std::vector<Matrix> &GetDirectionalShadowMatrices() const { return _directionalShadowMatrices; }
		const Vector2 &GetDirectionalShadowInfo() const { return _directionalShadowInfo; }
		RNAPI bool ContainsShadowCameraUID(uint64 uid) const;

	private:
		StrongRef<Texture> _directionalShadowTexture;
		std::vector<Matrix> _directionalShadowMatrices;
		Vector2 _directionalShadowInfo;
		std::vector<uint64> _shadowCameraUIDs;

		__RNDeclareMetaInternal(ShadowPassSnapshot)
	};
}

#endif /* __RAYNE_SHADOW_PASS_SNAPSHOT_H__ */
