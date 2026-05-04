//
//  RNShadowPassSnapshot.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNShadowPassSnapshot.h"

namespace RN
{
	RNDefineMeta(ShadowPassSnapshot, RenderPassDependencyProvider)

	ShadowPassSnapshot::ShadowPassSnapshot(Texture *directionalShadowTexture, const std::vector<Matrix> &directionalShadowMatrices, const Vector2 &directionalShadowInfo, const std::vector<uint64> &shadowCameraUIDs) :
		_directionalShadowTexture(directionalShadowTexture),
		_directionalShadowMatrices(directionalShadowMatrices),
		_directionalShadowInfo(directionalShadowInfo),
		_shadowCameraUIDs(shadowCameraUIDs)
	{}

	bool ShadowPassSnapshot::ContainsShadowCameraUID(uint64 uid) const
	{
		for(uint64 shadowCameraUID : _shadowCameraUIDs)
		{
			if(shadowCameraUID == uid)
				return true;
		}

		return false;
	}

	void ShadowPassSnapshot::CollectRenderPassDependencies(const RenderFrame::Pass &pass, RenderPassDependencyCollector &collector) const
	{
		if(!ContainsShadowCameraUID(pass.GetCameraSnapshot().GetSourceCameraUID()))
			collector.ReadsTexture(_directionalShadowTexture.Get());
	}
}
