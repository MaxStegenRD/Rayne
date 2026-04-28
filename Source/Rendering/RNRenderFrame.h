//
//  RNRenderFrame.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_RENDERFRAME_H__
#define __RAYNE_RENDERFRAME_H__

#include "RNDrawable.h"
#include "RNRenderPass.h"

namespace RN
{
	class RenderFrame
	{
	public:
		class DrawItem
		{
		public:
			DrawItem(Drawable *sourceDrawable, const Drawable::DrawPacket &drawPacket) :
				_sourceDrawable(sourceDrawable),
				_drawPacket(drawPacket)
			{}

			Drawable *GetSourceDrawable() const { return _sourceDrawable; }
			const Drawable::DrawPacket &GetDrawPacket() const { return _drawPacket; }

		private:
			Drawable *_sourceDrawable;
			Drawable::DrawPacket _drawPacket;
		};

		class Pass
		{
		public:
			Pass(const RenderPass::DrawSnapshot &drawSnapshot, const Material::DrawSnapshot *overrideMaterialSnapshot, uint64 overrideMaterialSnapshotVersion) :
				_drawSnapshot(drawSnapshot),
				_hasOverrideMaterial(overrideMaterialSnapshot != nullptr),
				_overrideMaterialSnapshotVersion(overrideMaterialSnapshot ? overrideMaterialSnapshotVersion : 0)
			{
				if(_hasOverrideMaterial)
					_overrideMaterialSnapshot = *overrideMaterialSnapshot;
			}

			void AddDrawItem(Drawable *sourceDrawable, const Drawable::DrawPacket &drawPacket)
			{
				_drawItems.emplace_back(sourceDrawable, drawPacket);
			}

			const RenderPass::DrawSnapshot &GetDrawSnapshot() const { return _drawSnapshot; }
			const Material::DrawSnapshot *GetOverrideMaterialSnapshot() const { return _hasOverrideMaterial ? &_overrideMaterialSnapshot : nullptr; }
			uint64 GetOverrideMaterialSnapshotVersion() const { return _overrideMaterialSnapshotVersion; }
			const std::vector<DrawItem> &GetDrawItems() const { return _drawItems; }

		private:
			RenderPass::DrawSnapshot _drawSnapshot;
			bool _hasOverrideMaterial;
			uint64 _overrideMaterialSnapshotVersion;
			Material::DrawSnapshot _overrideMaterialSnapshot;
			std::vector<DrawItem> _drawItems;
		};

		static constexpr size_t InvalidPassIndex = static_cast<size_t>(-1);

		void Clear()
		{
			_passes.clear();
		}

		size_t AddPass(const RenderPass::DrawSnapshot &drawSnapshot, const Material::DrawSnapshot *overrideMaterialSnapshot, uint64 overrideMaterialSnapshotVersion)
		{
			_passes.emplace_back(drawSnapshot, overrideMaterialSnapshot, overrideMaterialSnapshotVersion);
			return _passes.size() - 1;
		}

		Pass &GetPass(size_t index)
		{
			RN_DEBUG_ASSERT(index < _passes.size(), "Invalid render frame pass index");
			return _passes[index];
		}

		const Pass &GetPass(size_t index) const
		{
			RN_DEBUG_ASSERT(index < _passes.size(), "Invalid render frame pass index");
			return _passes[index];
		}

	private:
		std::vector<Pass> _passes;
	};
} // namespace RN

#endif /* __RAYNE_RENDERFRAME_H__ */
