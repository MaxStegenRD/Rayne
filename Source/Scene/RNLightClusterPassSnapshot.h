//
//  RNLightClusterPassSnapshot.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_LIGHT_CLUSTER_PASS_SNAPSHOT_H__
#define __RAYNE_LIGHT_CLUSTER_PASS_SNAPSHOT_H__

#include "RNLightManager.h"

namespace RN
{
	class LightClusterPassSnapshot : public Object
	{
	public:
		LightClusterPassSnapshot(const LightManager::DrawSnapshot &snapshot) :
			_snapshot(snapshot)
		{}

		const LightManager::DrawSnapshot &GetDrawSnapshot() const { return _snapshot; }

	private:
		LightManager::DrawSnapshot _snapshot;

		__RNDeclareMetaInternal(LightClusterPassSnapshot)
	};
}

#endif
