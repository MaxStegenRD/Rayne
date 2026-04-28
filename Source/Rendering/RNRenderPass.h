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
		private:
			enum ColorAttachmentFlag : uint8
			{
				ColorAttachmentReads = (1 << 0),
				ColorAttachmentWrites = (1 << 1),
				ColorAttachmentFirstWrite = (1 << 2),
				ColorAttachmentLastWrite = (1 << 3),
				ColorAttachmentNeedsStore = (1 << 4),
				ColorAttachmentFirstUseIsRead = (1 << 5),
				ColorAttachmentLastUseIsRead = (1 << 6),
				ColorAttachmentNeedsPreserve = (1 << 7)
			};

		public:
			class ColorAttachmentSnapshot
			{
			public:
				bool GetReads() const { return _flags & ColorAttachmentReads; }
				bool GetWrites() const { return _flags & ColorAttachmentWrites; }
				bool GetUses() const { return _flags & (ColorAttachmentReads | ColorAttachmentWrites); }
				bool GetIsFirstWrite() const { return _flags & ColorAttachmentFirstWrite; }
				bool GetIsLastWrite() const { return _flags & ColorAttachmentLastWrite; }
				bool GetNeedsStore() const { return _flags & ColorAttachmentNeedsStore; }
				bool GetFirstUseIsRead() const { return _flags & ColorAttachmentFirstUseIsRead; }
				bool GetLastUseIsRead() const { return _flags & ColorAttachmentLastUseIsRead; }
				bool GetNeedsPreserve() const { return _flags & ColorAttachmentNeedsPreserve; }

			private:
				friend class SubpassSnapshot;

				ColorAttachmentSnapshot(uint8 flags) :
					_flags(flags)
				{}

				uint8 _flags;
			};

			bool GetReadsDepthStencil() const { return _readsDepthStencil; }
			bool GetWritesDepthStencil() const { return _writesDepthStencil; }
			bool GetUsesDepthStencil() const { return _readsDepthStencil || _writesDepthStencil; }
			ColorAttachmentSnapshot GetColorAttachment(uint32 index) const { return ColorAttachmentSnapshot(GetColorAttachmentFlags(index)); }
			bool GetIsFirstDepthStencilWrite() const { return _firstDepthStencilWrite; }
			bool GetIsLastDepthStencilWrite() const { return _lastDepthStencilWrite; }
			bool GetNeedsToStoreDepthStencil() const { return _depthStencilNeedsStore; }
			bool GetNeedsToPreserveDepthStencil() const { return _depthStencilNeedsPreserve; }
			bool GetDepthFirstUseIsRead() const { return _depthFirstUseIsRead; }
			bool GetDepthLastUseIsRead() const { return _depthLastUseIsRead; }
			size_t GetWritesColorAttachmentCount() const { return _writesColorAttachmentCount; }

		private:
			friend class RenderPass;

			uint8 GetColorAttachmentFlags(uint32 index) const { return (index < _colorAttachmentFlags.size()) ? _colorAttachmentFlags[index] : 0; }
			void SetColorAttachmentFlag(uint32 index, uint8 flag)
			{
				if(index >= _colorAttachmentFlags.size()) _colorAttachmentFlags.resize(index + 1, 0);
				_colorAttachmentFlags[index] |= flag;
			}
			void SetColorAttachmentFlags(const std::vector<uint32> &attachments, uint8 flag)
			{
				for(uint32 index : attachments)
					SetColorAttachmentFlag(index, flag);
			}
			void SetColorAttachmentStates(const std::vector<uint32> &writes, const std::vector<uint32> &reads, const std::vector<uint32> &firstWrites, const std::vector<uint32> &lastWrites, const std::vector<uint32> &needsStore, const std::vector<uint32> &needsPreserve,
										  const std::vector<bool> &firstUseIsRead, const std::vector<bool> &lastUseIsRead)
			{
				_colorAttachmentFlags.clear();
				_writesColorAttachmentCount = writes.size();
				size_t colorAttachmentCount = firstUseIsRead.size();
				if(lastUseIsRead.size() > colorAttachmentCount) colorAttachmentCount = lastUseIsRead.size();
				_colorAttachmentFlags.resize(colorAttachmentCount, 0);
				SetColorAttachmentFlags(writes, ColorAttachmentWrites);
				SetColorAttachmentFlags(reads, ColorAttachmentReads);
				SetColorAttachmentFlags(firstWrites, ColorAttachmentFirstWrite);
				SetColorAttachmentFlags(lastWrites, ColorAttachmentLastWrite);
				SetColorAttachmentFlags(needsStore, ColorAttachmentNeedsStore);
				SetColorAttachmentFlags(needsPreserve, ColorAttachmentNeedsPreserve);
				for(size_t i = 0; i < firstUseIsRead.size(); i++)
				{
					if(firstUseIsRead[i]) SetColorAttachmentFlag(static_cast<uint32>(i), ColorAttachmentFirstUseIsRead);
				}
				for(size_t i = 0; i < lastUseIsRead.size(); i++)
				{
					if(lastUseIsRead[i]) SetColorAttachmentFlag(static_cast<uint32>(i), ColorAttachmentLastUseIsRead);
				}
			}

			std::vector<uint8> _colorAttachmentFlags;
			size_t _writesColorAttachmentCount = 0;
			bool _writesDepthStencil = false;
			bool _readsDepthStencil = false;
			bool _firstDepthStencilWrite = false;
			bool _lastDepthStencilWrite = false;
			bool _depthStencilNeedsStore = false;
			bool _depthStencilNeedsPreserve = false;
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
		std::vector<uint32> _subpassNeedToPreserveColorAttachment;
		bool _subpassFirstDepthStencilWrite;
		bool _subpassLastDepthStencilWrite;
		bool _subpassNeedToStoreDepthStencil;
		bool _subpassNeedToPreserveDepthStencil;
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

} // namespace RN

#include "RNRenderPassResources.h"

#endif /* __RAYNE_RENDERPASS_H__ */
