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

		uint64 GetIdentity() const { return _identity; }
		const Material::DrawSnapshot *GetOverrideMaterialSnapshot() const { return _hasOverrideMaterial ? &_overrideMaterialSnapshot : nullptr; }
		uint64 GetOverrideMaterialSnapshotVersion() const { return _overrideMaterialSnapshotVersion; }
		const RenderPass::DrawSnapshot &GetDrawSnapshot() const { return _drawSnapshot; }
		size_t GetLastDrawItemCount() const { return _lastDrawItemCount; }
		void SetLastDrawItemCount(size_t count) { _lastDrawItemCount = count; }

	private:
		void UpdateOverrideMaterial(Material *effectiveOverrideMaterial);

		uint64 _identity;

		uint64 _drawSnapshotSourceVersion = 0;
		RenderPass::DrawSnapshot _drawSnapshot;

		bool _hasOverrideMaterial = false;
		StrongRef<Material> _overrideMaterialSource;
		uint64 _overrideMaterialSourceVersion = 0;
		uint64 _overrideMaterialSnapshotVersion = 0;
		Material::DrawSnapshot _overrideMaterialSnapshot;

		size_t _lastDrawItemCount = 0;

		Renderer *_renderer;
	};
} // namespace RN


#endif /* __RAYNE_RENDERPASSRESOURCES_H__ */
