//
//  RNRenderPass.cpp
//  Rayne
//
//  Copyright 2017 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNRenderPass.h"
#include "../Rendering/RNRenderer.h"

namespace RN
{
	RNDefineMeta(RenderPass, Object)

	RenderPass::RenderPass(bool isSubpass) :
		_flags(Flags::Defaults), _framebuffer(nullptr), _clearDepth(0.0f), _clearStencil(0), _nextRenderPasses(new Array()), _isSubpass(isSubpass), _isRoot(false), _subpassWritesDepthStencil(false), _subpassReadDepthStencilAttachment(false)
	{
	}

	RenderPass::~RenderPass()
	{
		SafeRelease(_framebuffer);
	}

	// Setter
	void RenderPass::SetFramebuffer(Framebuffer *framebuffer)
	{
		RN_ASSERT(!_isSubpass, "Cannot set framebuffer for subpass");

		SafeRelease(_framebuffer);
		_framebuffer = framebuffer->Retain();
	}

	void RenderPass::SetFlags(Flags flags)
	{
		_flags = flags;
	}

	void RenderPass::SetFrame(const Rect &frame)
	{
		RN_ASSERT(!_isSubpass, "Cannot set frame for subpass");
		_frame = std::move(frame.GetIntegral());
	}

	void RenderPass::SetClearColor(const Color &color)
	{
		RN_ASSERT(!_isSubpass, "Cannot set clear color for subpass");
		_clearColor = color;
	}

	void RenderPass::SetClearDepthStencil(float depth, uint8 stencil)
	{
		RN_ASSERT(!_isSubpass, "Cannot set clear depth stencil for subpass");
		_clearDepth = depth;
		_clearStencil = stencil;
	}

	//Getter
	Framebuffer *RenderPass::GetFramebuffer() const
	{
		RN_ASSERT(!_isSubpass, "Cannot get framebuffer for subpass");
		if(!_framebuffer)
		{
			return Renderer::GetActiveRenderer()->GetMainWindow()->GetFramebuffer();
		}

		return _framebuffer;
	}

	Rect RenderPass::GetFrame() const
	{
		RN_ASSERT(!_isSubpass, "Cannot get frame for subpass");
		if(std::abs(_frame.GetArea()) > 0.0001)
		{
			return _frame;
		}

		if(_framebuffer)
		{
			Rect frame(Vector2(), _framebuffer->GetSize());
			return frame;
		}

		Renderer *renderer = Renderer::GetActiveRenderer();
		Vector2 mainWindowSize = renderer->GetMainWindow()->GetSize();
		Rect windowFrame(Vector2(), mainWindowSize);
		return windowFrame;
	}

	void RenderPass::SetSubpassWritesDepthStencil(bool writesDepthStencil)
	{
		RN_ASSERT(_isSubpass, "Cannot set subpass writes depth stencil for non-subpass");
		RN_ASSERT(_subpassReadDepthStencilAttachment != writesDepthStencil, "Cannot set subpass writes depth stencil to the same value");
		_subpassWritesDepthStencil = writesDepthStencil;
	}

	void RenderPass::SetSubpassReadDepthStencilAttachment(bool depthStencilAttachment)
	{
		RN_ASSERT(_isSubpass, "Cannot set subpass read depth stencil attachment for non-subpass");
		RN_ASSERT(_subpassWritesDepthStencil != depthStencilAttachment, "Cannot set subpass read depth stencil attachment to the same value");
		_subpassReadDepthStencilAttachment = depthStencilAttachment;
	}
	
	void RenderPass::SetSubpassWritesColorAttachments(std::vector<uint32> colorAttachments)
	{
		RN_ASSERT(_isSubpass, "Cannot set subpass writes color attachments for non-subpass");
		std::sort(colorAttachments.begin(), colorAttachments.end());
		bool hasDuplicates = std::adjacent_find(colorAttachments.begin(), colorAttachments.end()) != colorAttachments.end();
		RN_ASSERT(!hasDuplicates, "Cannot set duplicate color write attachments");
		std::vector<uint32> intersection;
		std::set_intersection(colorAttachments.begin(), colorAttachments.end(), _subpassReadColorAttachments.begin(), _subpassReadColorAttachments.end(), std::back_inserter(intersection));
		RN_ASSERT(intersection.empty(), "Subpass can not read and write the same color attachment");
		_subpassWritesColorAttachments = colorAttachments;
	}
	
	void RenderPass::SetSubpassReadColorAttachments(std::vector<uint32> colorAttachments)
	{
		RN_ASSERT(_isSubpass, "Cannot set subpass read color attachments for non-subpass");
		std::sort(colorAttachments.begin(), colorAttachments.end());
		bool hasDuplicates = std::adjacent_find(colorAttachments.begin(), colorAttachments.end()) != colorAttachments.end();
		RN_ASSERT(!hasDuplicates, "Cannot set duplicate color read attachments");
		std::vector<uint32> intersection;
		std::set_intersection(colorAttachments.begin(), colorAttachments.end(), _subpassWritesColorAttachments.begin(), _subpassWritesColorAttachments.end(), std::back_inserter(intersection));
		RN_ASSERT(intersection.empty(), "Subpass can not read and write the same color attachment");
		_subpassReadColorAttachments = colorAttachments;
	}
	
	void RenderPass::UpdateSubpassChain(std::vector<uint32> &loadedColorTargets, bool &loadedDepthStencil)
	{
		GetNextRenderPasses()->Enumerate<RenderPass>([&](RenderPass *nextPass, size_t index, bool &stop) {
			if(nextPass->GetIsSubpass())
			{
				nextPass->_flags = _flags;
				nextPass->_clearDepth = _clearDepth;
				nextPass->_clearStencil = _clearStencil;
				nextPass->_clearColor = _clearColor;
				nextPass->_subpassFirstDepthStencilWrite = false;
				nextPass->_subpassFirstColorWriteAttachment.clear();

				for(uint32 attachment : nextPass->GetSubpassWritesColorAttachments())
				{
					if(std::find(loadedColorTargets.begin(), loadedColorTargets.end(), attachment) == loadedColorTargets.end())
					{
						nextPass->_subpassFirstColorWriteAttachment.push_back(attachment);
						loadedColorTargets.push_back(attachment);
					}
				}

				if(!loadedDepthStencil)
				{
					if(nextPass->_subpassWritesDepthStencil)
					{
						nextPass->_subpassFirstDepthStencilWrite = true;
						loadedDepthStencil = true;
					}
				}

				nextPass->UpdateSubpassChain(loadedColorTargets, loadedDepthStencil);
			}
		});
	}

	void RenderPass::AddRenderPass(RenderPass *renderPass)
	{
		_nextRenderPasses->AddObject(renderPass);

		_isRoot = false;
		if(renderPass->GetIsSubpass() && !_isSubpass)
		{
			_isRoot = true;

			//Subpasses inherit the root pass's flags, clear depth, and clear stencil
			UpdateSubpassChain();
		}
	}

	void RenderPass::RemoveRenderPass(RenderPass *renderPass)
	{
		_nextRenderPasses->RemoveObject(renderPass);

		_isRoot = false;
		_nextRenderPasses->Enumerate<RenderPass>([&](RenderPass *nextPass, size_t index, bool &stop) {
			if(nextPass->GetIsSubpass() && !_isSubpass)
			{
				_isRoot = true;
				stop = true;
			}
		});
	}

	void RenderPass::RemoveAllRenderPasses()
	{
		_nextRenderPasses->RemoveAllObjects();
		_isRoot = false;
	}

	void RenderPass::UpdateSubpassChain()
	{
		if(!_isRoot) return; // only meaningful for roots
		std::vector<uint32> loadedColorTargets;
		bool loadedDepthStencil = false;
		UpdateSubpassChain(loadedColorTargets, loadedDepthStencil);
	}
} // namespace RN
