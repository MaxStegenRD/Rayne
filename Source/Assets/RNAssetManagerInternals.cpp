//
//  RNAssetManagerInternals.cpp
//  Rayne
//
//  Copyright 2016 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNAssetManagerInternals.h"
#include "../Objects/RNString.h"

namespace RN
{
	RNDefineMeta(PendingAsset, Object)
	RNDefineMeta(LoadedAsset, Object)

	LoadedAsset::LoadedAsset(Asset *asset, MetaClass *meta) :
		_asset(asset),
		_meta(meta)
	{}

	PendingAsset::PendingAsset(MetaClass *meta, String *name) :
		_isFinished(false),
		_meta(meta),
		_name(name)
	{
		_future = _promise.get_future().share();
	}

	void PendingAsset::SetAsset(Asset *asset)
	{
		LockGuard<Lockable> lock(_lock);
		if(_isFinished)
			return;

		_isFinished = true;
		_promise.set_value(StrongRef<Asset>(asset));
	}
} // namespace RN
