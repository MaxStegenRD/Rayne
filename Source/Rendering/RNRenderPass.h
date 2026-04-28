//
//  RNRenderPass.h
//  Rayne
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//


#ifndef __RAYNE_RENDERPASS_H__
#define __RAYNE_RENDERPASS_H__

#include "../Base/RNBase.h"
#include "../Math/RNRect.h"
#include "../Objects/RNArray.h"
#include "RNFramebuffer.h"
#include "RNMaterial.h"

namespace RN
{
	class Renderer;
	struct RenderPassResources;

	class RenderPass : public Object
	{
	public:
		RN_OPTIONS(Flags, uint32,
				   ClearColor = (1 << 0),
				   LoadColor = (1 << 1),
				   StoreColor = (1 << 2),
				   ClearDepthStencil = (1 << 3),
				   LoadDepthStencil = (1 << 4),
				   StoreDepthStencil = (1 << 5),
				   Defaults = ClearDepthStencil | StoreColor);

		class SubpassSnapshot
		{
		public:
			bool GetReadsDepthStencil() const { return _readsDepthStencil; }
			bool GetWritesDepthStencil() const { return _writesDepthStencil; }
			bool GetUsesDepthStencil() const { return _readsDepthStencil || _writesDepthStencil; }
			bool GetReadsColorAttachment(uint32 index) const { return std::find(_readColorAttachments.begin(), _readColorAttachments.end(), index) != _readColorAttachments.end(); }
			bool GetWritesColorAttachment(uint32 index) const { return std::find(_writesColorAttachments.begin(), _writesColorAttachments.end(), index) != _writesColorAttachments.end(); }
			bool GetUsesColorAttachment(uint32 index) const { return GetReadsColorAttachment(index) || GetWritesColorAttachment(index); }
			bool GetIsFirstColorWriteAttachment(uint32 index) const { return std::find(_firstColorWriteAttachments.begin(), _firstColorWriteAttachments.end(), index) != _firstColorWriteAttachments.end(); }
			bool GetIsLastColorWriteAttachment(uint32 index) const { return std::find(_lastColorWriteAttachments.begin(), _lastColorWriteAttachments.end(), index) != _lastColorWriteAttachments.end(); }
			bool GetNeedsToStoreColorAttachment(uint32 index) const { return std::find(_colorAttachmentsToStore.begin(), _colorAttachmentsToStore.end(), index) != _colorAttachmentsToStore.end(); }
			bool GetIsFirstDepthStencilWrite() const { return _firstDepthStencilWrite; }
			bool GetIsLastDepthStencilWrite() const { return _lastDepthStencilWrite; }
			bool GetNeedsToStoreDepthStencil() const { return _depthStencilNeedsStore; }
			bool GetFirstUseIsRead(uint32 index) const { return (index < _firstUseIsRead.size())? _firstUseIsRead[index] : false; }
			bool GetLastUseIsRead(uint32 index) const { return (index < _lastUseIsRead.size())? _lastUseIsRead[index] : false; }
			bool GetDepthFirstUseIsRead() const { return _depthFirstUseIsRead; }
			bool GetDepthLastUseIsRead() const { return _depthLastUseIsRead; }
			size_t GetWritesColorAttachmentCount() const { return _writesColorAttachments.size(); }

		private:
			friend class RenderPass;

			std::vector<uint32> _writesColorAttachments;
			std::vector<uint32> _readColorAttachments;
			std::vector<uint32> _firstColorWriteAttachments;
			std::vector<uint32> _lastColorWriteAttachments;
			std::vector<uint32> _colorAttachmentsToStore;
			bool _writesDepthStencil = false;
			bool _readsDepthStencil = false;
			bool _firstDepthStencilWrite = false;
			bool _lastDepthStencilWrite = false;
			bool _depthStencilNeedsStore = false;
			std::vector<bool> _firstUseIsRead;
			std::vector<bool> _lastUseIsRead;
			bool _depthFirstUseIsRead = false;
			bool _depthLastUseIsRead = false;
		};

		class DrawSnapshot
		{
		public:
			Flags GetFlags() const { return _flags; }
			const Rect &GetFrame() const { return _frame; }
			Framebuffer *GetFramebuffer() const { return _framebuffer.Get(); }
			const Color &GetClearColor() const { return _clearColor; }
			float GetClearDepth() const { return _clearDepth; }
			uint8 GetClearStencil() const { return _clearStencil; }
			uint16 GetRenderGroupMask() const { return _renderGroupMask; }
			Shader::UsageHint GetShaderHint() const { return _shaderHint; }
			bool IsSubpass() const { return _isSubpass; }
			bool IsRoot() const { return _isRoot; }
			const SubpassSnapshot &GetSubpass() const { return _subpass; }

		private:
			friend class RenderPass;
			friend struct RenderPassResources;

			Flags _flags = Flags::Defaults;
			Rect _frame;
			StrongRef<Framebuffer> _framebuffer;
			Color _clearColor;
			float _clearDepth = 0.0f;
			uint8 _clearStencil = 0;
			uint16 _renderGroupMask = 0xffff;
			Shader::UsageHint _shaderHint = Shader::UsageHint::Default;
			bool _isSubpass = false;
			bool _isRoot = false;
			SubpassSnapshot _subpass;
		};

		RNAPI RenderPass(bool isSubpass = false);
		RNAPI ~RenderPass();

		RNAPI void SetFramebuffer(Framebuffer *framebuffer);
		RNAPI virtual void SetFlags(Flags flags);
		RNAPI void SetFrame(const Rect &frame);
		RNAPI void SetClearColor(const Color &color);
		RNAPI void SetClearDepthStencil(float depth, uint8 stencil);

		RNAPI void SetRenderGroupMask(uint16 mask);

		RNAPI void SetShaderHint(Shader::UsageHint hint);
		RNAPI void SetOverrideMaterial(Material *material);

		RNAPI void SetSubpassWritesDepthStencilAttachment(bool writesDepthStencil);
		RNAPI void SetSubpassReadsDepthStencilAttachment(bool depthStencilAttachment);
		RNAPI void SetSubpassWritesColorAttachments(std::vector<uint32> colorAttachments);
		RNAPI void SetSubpassReadsColorAttachments(std::vector<uint32> colorAttachments);

		RNAPI Framebuffer *GetFramebuffer() const;
		Flags GetFlags() const { return _flags; }
		RNAPI Rect GetFrame() const;
		const Color &GetClearColor() const { return _clearColor; }
		float GetClearDepth() const { return _clearDepth; }
		uint8 GetClearStencil() const { return _clearStencil; }
		uint16 GetRenderGroupMask() const { return _renderGroupMask; }

		Shader::UsageHint GetShaderHint() const { return _shaderHint; }
		Material *GetOverrideMaterial() const { return _overrideMaterial; }
		RNAPI virtual Material *GetEffectiveOverrideMaterial() const;
		RNAPI RenderPassResources *GetRenderResources(Renderer *renderer);
		uint64 GetDrawSnapshotVersion() const { return _drawSnapshotVersion; }
		RNAPI void GetDrawSnapshot(DrawSnapshot &snapshot) const;

		bool GetIsSubpass() const { return _isSubpass; }
		bool GetIsRoot() const { return _isRoot; }
		const std::vector<uint32> &GetSubpassWritesColorAttachments() const { return _subpassWritesColorAttachments; }
		const std::vector<uint32> &GetSubpassReadColorAttachments() const { return _subpassReadColorAttachments; }
		bool GetSubpassWritesDepthStencil() const { return _subpassWritesDepthStencil; }
		bool GetSubpassReadDepthStencilAttachment() const { return _subpassReadDepthStencilAttachment; }
		bool GetSubpassReadColorAttachment(uint32 index) const { return std::find(_subpassReadColorAttachments.begin(), _subpassReadColorAttachments.end(), index) != _subpassReadColorAttachments.end(); }
		bool GetSubpassWritesColorAttachment(uint32 index) const { return std::find(_subpassWritesColorAttachments.begin(), _subpassWritesColorAttachments.end(), index) != _subpassWritesColorAttachments.end(); }

		RNAPI void AddRenderPass(RenderPass *renderPass);
		RNAPI void RemoveRenderPass(RenderPass *renderPass);
		RNAPI void RemoveAllRenderPasses();
		const Array *GetNextRenderPasses() const { return _nextRenderPasses; }
		RNAPI void UpdateSubpassChain();

	private:
		void MarkDrawSnapshotDirty();
	
		Flags _flags;
		Rect _frame;
		Framebuffer *_framebuffer;
		Color _clearColor;
		float _clearDepth;
		uint8 _clearStencil;
		bool _isSubpass;
		bool _isRoot;
		uint16 _renderGroupMask;

		Shader::UsageHint _shaderHint;
		Material *_overrideMaterial;

		bool _subpassWritesDepthStencil;
		bool _subpassReadDepthStencilAttachment;
		std::vector<uint32> _subpassWritesColorAttachments;
		std::vector<uint32> _subpassReadColorAttachments;
		std::vector<uint32> _subpassFirstColorWriteAttachment;
		std::vector<uint32> _subpassLastColorWriteAttachment;
		std::vector<uint32> _subpassNeedToStoreColorAttachment;
		bool _subpassFirstDepthStencilWrite;
		bool _subpassLastDepthStencilWrite;
		bool _subpassNeedToStoreDepthStencil;
		std::vector<bool> _subpassFirstUseIsRead;
		std::vector<bool> _subpassLastUseIsRead;
		bool _depthFirstUseIsRead;
		bool _depthLastUseIsRead;
		size_t _subpassIndex;

		Array *_nextRenderPasses;
		RenderPassResources *_renderResources;
		uint64 _drawSnapshotVersion;

		__RNDeclareMetaInternal(RenderPass)
	};

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


#endif /* __RAYNE_RENDERPASS_H__ */
