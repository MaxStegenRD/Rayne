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
#include "RNRenderingConfig.h"

namespace RN
{
	class Renderer;

	struct RenderPassResources
	{
		RNAPI RenderPassResources(Renderer *renderer);
		RNAPI virtual ~RenderPassResources();

		struct DrawPacket
		{
			const Material::DrawSnapshot *GetOverrideMaterialSnapshot() const
			{
				return _hasOverrideMaterial ? &_overrideMaterialSnapshot : nullptr;
			}

			uint64 GetOverrideMaterialSnapshotVersion() const { return _overrideMaterialSnapshotVersion; }
			const RenderPass::DrawSnapshot &GetDrawSnapshot() const { return _drawSnapshot; }

		private:
			friend struct RenderPassResources;

			uint64 _drawSnapshotSourceVersion = 0;
			RenderPass::DrawSnapshot _drawSnapshot;

			bool _hasOverrideMaterial = false;
			uint64 _overrideMaterialSourceSequence = 0;
			uint64 _overrideMaterialSourceVersion = 0;
			uint64 _overrideMaterialSnapshotVersion = 0;
			Material::DrawSnapshot _overrideMaterialSnapshot;
		};

		RNAPI void Delete();
		RNAPI void Update(RenderPass *renderPass, Material *effectiveOverrideMaterial);
		RNAPI void UpdateOverrideMaterial(Material *effectiveOverrideMaterial);

		const DrawPacket &GetDrawPacket() const { return _drawPackets[_activeDrawPacketIndex]; }
		const Material::DrawSnapshot *GetOverrideMaterialSnapshot() const { return GetDrawPacket().GetOverrideMaterialSnapshot(); }
		uint64 GetOverrideMaterialSnapshotVersion() const { return GetDrawPacket().GetOverrideMaterialSnapshotVersion(); }
		const RenderPass::DrawSnapshot &GetDrawSnapshot() const { return GetDrawPacket().GetDrawSnapshot(); }

	private:
		DrawPacket &GetMutableDrawPacket() { return _drawPackets[_activeDrawPacketIndex]; }

		DrawPacket _drawPackets[RN_RENDERING_PACKET_SLOT_COUNT];
		uint8 _activeDrawPacketIndex = 0;

		StrongRef<Material> overrideMaterialSource;
		uint64 overrideMaterialSourceSequence = 0;
		uint64 overrideMaterialSourceVersion = 0;
		uint64 overrideMaterialSnapshotVersion = 0;

		Renderer *_renderer;
	};
} // namespace RN


#endif /* __RAYNE_RENDERPASSRESOURCES_H__ */
