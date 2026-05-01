//
//  RNRenderPassResources.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNRenderPassResources.h"
#include "RNRenderer.h"

namespace RN
{
	std::atomic<uint64> __RenderPassResourceIdentities(1);

	RenderPassResources::RenderPassResources(Renderer *renderer) :
		_identity(__RenderPassResourceIdentities.fetch_add(1)),
		_renderer(renderer)
	{}

	RenderPassResources::~RenderPassResources()
	{}

	void RenderPassResources::Delete()
	{
		_renderer->DeleteRenderPassResources(this);
	}

	void RenderPassResources::Update(RenderPass *renderPass, Material *effectiveOverrideMaterial)
	{
		uint64 snapshotVersion = renderPass->GetDrawSnapshotVersion();
		if(_drawSnapshotSourceVersion != snapshotVersion)
		{
			renderPass->GetDrawSnapshot(_drawSnapshot);
			_drawSnapshotSourceVersion = snapshotVersion;
		}
		else
		{
			renderPass->UpdateDrawSnapshotFrame(_drawSnapshot);
		}

		UpdateOverrideMaterial(effectiveOverrideMaterial);
	}

	void RenderPassResources::UpdateOverrideMaterial(Material *effectiveOverrideMaterial)
	{
		bool overrideMaterialSourceChanged = _overrideMaterialSource.Get() != effectiveOverrideMaterial;
		uint64 overrideSourceVersion = effectiveOverrideMaterial ? effectiveOverrideMaterial->GetDrawSnapshotVersion() : 0;
		if(overrideMaterialSourceChanged)
		{
			_overrideMaterialSource = effectiveOverrideMaterial;
		}

		if(overrideMaterialSourceChanged || _overrideMaterialSourceVersion != overrideSourceVersion)
		{
			_overrideMaterialSourceVersion = overrideSourceVersion;
			_overrideMaterialSnapshotVersion += 1;
			_hasOverrideMaterial = effectiveOverrideMaterial != nullptr;

			if(effectiveOverrideMaterial)
				effectiveOverrideMaterial->GetDrawSnapshot(_overrideMaterialSnapshot);
			else
				_overrideMaterialSnapshot.Reset();
		}
	}
} // namespace RN
