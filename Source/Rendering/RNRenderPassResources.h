//
//  RNRenderPassResources.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//


#ifndef __RAYNE_RENDERPASSRESOURCES_H__
#define __RAYNE_RENDERPASSRESOURCES_H__

#include "../Base/RNBase.h"
#include "RNMaterial.h"
#include "RNRenderPass.h"

namespace RN
{
	class Renderer;

	struct RenderPassResources
	{
		RNAPI RenderPassResources(Renderer *renderer);
		RNAPI virtual ~RenderPassResources();

		RNAPI void Delete();
		RNAPI void Update(RenderPass *renderPass, Material *effectiveOverrideMaterial);
		RNAPI void UpdateOverrideMaterial(Material *effectiveOverrideMaterial);

		const Material::DrawSnapshot *GetOverrideMaterialSnapshot() const
		{
			return overrideMaterialSource.Get() ? &overrideMaterialSnapshot : nullptr;
		}
		uint64 GetOverrideMaterialSnapshotVersion() const { return overrideMaterialSnapshotVersion; }
		const RenderPass::DrawSnapshot &GetDrawSnapshot() const { return _drawSnapshot; }

	private:
		uint64 drawSnapshotVersion = 0;
		RenderPass::DrawSnapshot _drawSnapshot;
		StrongRef<Material> overrideMaterialSource;
		// Source version refreshes the snapshot; snapshot version invalidates drawable caches.
		uint64 overrideMaterialSourceVersion = 0;
		uint64 overrideMaterialSnapshotVersion = 0;
		Material::DrawSnapshot overrideMaterialSnapshot;

		Renderer *_renderer;
	};
} // namespace RN


#endif /* __RAYNE_RENDERPASSRESOURCES_H__ */
