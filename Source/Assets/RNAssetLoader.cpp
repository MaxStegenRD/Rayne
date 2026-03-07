//
//  RNAssetLoader.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNAssetLoader.h"
#include "../Debug/RNLogger.h"
#include "../Objects/RNAutoreleasePool.h"
#include "../Threads/RNWorkQueue.h"
#include "RNAssetManager.h"

namespace RN
{
	RNDefineMeta(AssetLoader, Object)

	AssetLoader::AssetLoader(const Config &config) :
		_magicBytes(nullptr),
		_magicBytesOffset(0),
		_fileExtensions(nullptr),
		_priority(config.priority),
		_supportsBackgroundLoading(config.supportsBackgroundLoading),
		_supportsVirtualFiles(config.supportsVirtualFiles),
		_resourceClasses(config.resourceClasses)
	{
		_magicBytes = SafeRetain(config._magicBytes);
		_magicBytesOffset = config._magicBytesOffset;
		_fileExtensions = SafeRetain(config._extensions);

		for(MetaClass *meta : _resourceClasses)
		{
			RN_ASSERT(meta->InheritsFromClass(Asset::GetMetaClass()), "AssetLoader must support loading Asset subclasses only");
		}
	}

	AssetLoader::~AssetLoader()
	{
		SafeRelease(_magicBytes);
		SafeRelease(_fileExtensions);
	}


	Asset *AssetLoader::Load(File *file, const LoadOptions &options)
	{
		throw NotImplementedException("Load(File *, const LoadOptions &) not implemented");
	}

	Asset *AssetLoader::Load(const String *name, const LoadOptions &options)
	{
		throw NotImplementedException("Load(const String *, const LoadOptions &) not implemented");
	}

	Expected<Asset *> AssetLoader::__Load(Object *fileOrName, const LoadOptions &options) RN_NOEXCEPT
	{
		try
		{
			if(fileOrName->IsKindOfClass(File::GetMetaClass()))
			{
				File *file = static_cast<File *>(fileOrName);
				return Load(file, options);
			}
			else
			{
				String *name = static_cast<String *>(fileOrName);
				return Load(name, options);
			}
		}
		catch(...)
		{
			return std::current_exception();
		}
	}

	void AssetLoader::__LoadInBackground(Object *fileOrName, const LoadOptions &options, void *token)
	{
		fileOrName->Retain();

		options.queue->Perform([=]() {
			Expected<Asset *> asset;
			Asset *result = nullptr;

			AutoreleasePool::PerformBlock([&] {
				asset = __Load(fileOrName, options);
				if(asset.IsValid()) result = SafeRetain(asset.Get());
			});

			AssetManager *manager = AssetManager::GetSharedInstance();
			if(result)
			{
				manager->__FinishLoadingAsset(token, result);
				result->Release();
			}
			else
			{
				manager->__FinishLoadingAsset(token, std::move(asset));
			}

			fileOrName->Release();
		});
	}

	bool AssetLoader::SupportsLoadingFile(File *file) const
	{
		return true;
	}

	bool AssetLoader::SupportsLoadingName(const String *name) const
	{
		return _supportsVirtualFiles;
	}

	bool AssetLoader::SupportsResourceClass(MetaClass *meta) const
	{
		for(auto tmeta : _resourceClasses)
		{
			if(meta->InheritsFromClass(tmeta))
				return true;
		}

		return false;
	}
} // namespace RN
