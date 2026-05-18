//
//  RNAssetManagerInternals.h
//  Rayne
//
//  Copyright 2016 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_ASSETMANAGERINTERNALS_H_
#define __RAYNE_ASSETMANAGERINTERNALS_H_

#include "../Base/RNBase.h"
#include "../Objects/RNObject.h"
#include "RNAsset.h"

namespace RN
{
	class LoadedAsset : public Object
	{
	public:
		LoadedAsset(Asset *asset, MetaClass *meta);

		MetaClass *GetMeta() const { return _meta; }
		Asset *GetAsset() const { return _asset.Load(); }

	private:
		WeakRef<Asset> _asset;
		MetaClass *_meta;

		__RNDeclareMetaInternal(LoadedAsset)
	};

	class PendingAsset : public Object
	{
	public:
		PendingAsset(MetaClass *meta, String *name);

		void SetAsset(Asset *asset);

		MetaClass *GetMeta() const { return _meta; }
		String *GetName() const { return _name; }
		AssetLoadFuture GetFuture() const { return _future; }

	private:
		Lockable _lock;
		std::promise<StrongRef<Asset>> _promise;
		AssetLoadFuture _future;
		bool _isFinished;
		MetaClass *_meta;
		StrongRef<String> _name;

		__RNDeclareMetaInternal(PendingAsset)
	};
} // namespace RN

#endif /* __RAYNE_ASSETMANAGERINTERNALS_H_ */
