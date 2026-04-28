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

		struct DrawSnapshot
		{
			Flags flags = Flags::Defaults;
			Rect frame;
			StrongRef<Framebuffer> framebuffer;
			Color clearColor;
			float clearDepth = 0.0f;
			uint8 clearStencil = 0;
			uint16 renderGroupMask = 0xffff;
			Shader::UsageHint shaderHint = Shader::UsageHint::Default;
			bool isSubpass = false;
			bool isRoot = false;
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
		bool GetSubpassFirstColorWriteAttachment(uint32 index) const { return std::find(_subpassFirstColorWriteAttachment.begin(), _subpassFirstColorWriteAttachment.end(), index) != _subpassFirstColorWriteAttachment.end(); }
		bool GetSubpassLastColorWriteAttachment(uint32 index) const { return std::find(_subpassLastColorWriteAttachment.begin(), _subpassLastColorWriteAttachment.end(), index) != _subpassLastColorWriteAttachment.end(); }
		bool GetSubpassNeedToStoreColorAttachment(uint32 index) const { return std::find(_subpassNeedToStoreColorAttachment.begin(), _subpassNeedToStoreColorAttachment.end(), index) != _subpassNeedToStoreColorAttachment.end(); }
		bool GetSubpassFirstDepthStencilWrite() const { return _subpassFirstDepthStencilWrite; }
		bool GetSubpassLastDepthStencilWrite() const { return _subpassLastDepthStencilWrite; }
		bool GetSubpassNeedToStoreDepthStencil() const { return _subpassNeedToStoreDepthStencil; }
		bool GetSubpassFirstUseIsRead(uint32 index) const { return (index < _subpassFirstUseIsRead.size())? _subpassFirstUseIsRead[index] : false; }
		bool GetSubpassLastUseIsRead(uint32 index) const { return (index < _subpassLastUseIsRead.size())? _subpassLastUseIsRead[index] : false; }
		bool GetSubpassFirstDepthStencilUseIsRead() const { return _depthFirstUseIsRead; }
		bool GetSubpassLastDepthStencilUseIsRead() const { return _depthLastUseIsRead; }

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

		uint64 drawSnapshotVersion = 0;

		StrongRef<Material> overrideMaterialSource;
		// Tracks the source material version we snapshotted; the snapshot version below tracks changes visible to drawable caches.
		uint64 overrideMaterialSourceVersion = 0;
		uint64 overrideMaterialSnapshotVersion = 0;
		Material::DrawSnapshot overrideMaterialSnapshot;

		RenderPass::DrawSnapshot drawSnapshot;

	private:
		Renderer *_renderer;
	};
} // namespace RN


#endif /* __RAYNE_RENDERPASS_H__ */
