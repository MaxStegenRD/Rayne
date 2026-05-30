//
//  RNFrameSubmissionPruner.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_FRAMESUBMISSIONPRUNER_H__
#define __RAYNE_FRAMESUBMISSIONPRUNER_H__

#include "RNRenderFrame.h"

namespace RN
{
	template<class RenderPassType>
	bool FrameSubmissionPassUsesFramePass(const RenderPassType &frameSubmissionRenderPass, FramePass *framePass)
	{
		return frameSubmissionRenderPass.renderPass == framePass;
	}

	template<class Pruner, class RenderPassType>
	void FrameSubmissionAddFramePassDependencies(Pruner &pruner, size_t consumerIndex, RenderPassType &frameSubmissionRenderPass)
	{
		pruner.AddExplicitFramePassDependencies(consumerIndex, frameSubmissionRenderPass.renderPass);
	}

	template<class Submission>
	class FrameSubmissionPruner
	{
	public:
		using RenderPassType = typename Submission::RenderPassType;
		using FramebufferType = typename Submission::FramebufferType;
		using SwapChainType = typename Submission::SwapChainType;

		FrameSubmissionPruner(Submission &submission) :
			_submission(submission)
		{}

		void Prune()
		{
			if(_submission.renderPasses.empty()) return;

			_dependencies.clear();
			_passStates.clear();
			_passStates.resize(_submission.renderPasses.size());

			BuildDependencies();
			MarkLivePasses();
			PropagateLiveDependencies();
			ErasePrunedPasses();
		}

		void AddExplicitFramePassDependencies(size_t consumerIndex, FramePass *framePass)
		{
			if(!framePass) return;

			for(const WeakRef<FramePass> &dependency : framePass->GetFramePassDependencies())
			{
				AddDependency(consumerIndex, FindFramePassIndex(dependency.Load()));
			}
		}

		void AddRenderFramePassSnapshotDependencies(size_t consumerIndex, size_t renderFramePassIndex)
		{
			if(renderFramePassIndex == RenderFrame::InvalidPassIndex)
				return;

			RenderPassDependencyCollector collector;
			const RenderFrame::Pass &framePass = _submission.renderFrame.GetPass(renderFramePassIndex);
			framePass.EnumerateAttachmentSnapshots([&](Object *snapshot) {
				RenderPassDependencyProvider *provider = snapshot->Downcast<RenderPassDependencyProvider>();
				if(provider)
					provider->CollectRenderPassDependencies(framePass, collector);
			});

			for(Texture *texture : collector.GetReadTextures())
			{
				AddTextureDependency(consumerIndex, texture);
			}
		}

	private:
		struct PassState
		{
			bool skippedBySwapChain = false;
			bool pruneIfUnused = false;
			bool live = false;
		};

		struct DependencyEdge
		{
			size_t consumerIndex;
			size_t producerIndex;
		};

		static constexpr size_t InvalidIndex = static_cast<size_t>(-1);

		bool UsesSubmittedSwapChain(SwapChainType *swapChain) const
		{
			if(!swapChain) return false;

			for(SwapChainType *submittedSwapChain : _submission.swapChains)
			{
				if(submittedSwapChain == swapChain)
					return true;
			}

			return false;
		}

		bool FramebufferTargetsSubmittedSwapChain(const FramebufferType *framebuffer) const
		{
			return framebuffer && UsesSubmittedSwapChain(framebuffer->GetSwapChain());
		}

		bool FramebufferTargetsSkippedSwapChain(const FramebufferType *framebuffer) const
		{
			return framebuffer && framebuffer->GetSwapChain() && !UsesSubmittedSwapChain(framebuffer->GetSwapChain());
		}

		bool FramebufferProducesTexture(const FramebufferType *framebuffer, Texture *texture) const
		{
			if(!framebuffer || !texture) return false;

			for(uint32 i = 0; i < framebuffer->GetColorTargetCount(); i++)
			{
				if(framebuffer->GetColorTexture(i) == texture) return true;
			}

			return framebuffer->GetDepthStencilTexture() == texture;
		}

		bool PassProducesTexture(const RenderPassType &renderPass, Texture *texture) const
		{
			return FramebufferProducesTexture(renderPass.framebuffer, texture) || FramebufferProducesTexture(renderPass.resolveFramebuffer, texture);
		}

		bool PassTargetsSubmittedSwapChain(const RenderPassType &renderPass) const
		{
			return FramebufferTargetsSubmittedSwapChain(renderPass.framebuffer) || FramebufferTargetsSubmittedSwapChain(renderPass.resolveFramebuffer);
		}

		bool PassTargetsSkippedSwapChain(const RenderPassType &renderPass) const
		{
			return FramebufferTargetsSkippedSwapChain(renderPass.framebuffer) || FramebufferTargetsSkippedSwapChain(renderPass.resolveFramebuffer);
		}

		size_t FindFramePassIndex(FramePass *framePass) const
		{
			if(!framePass) return InvalidIndex;

			for(size_t i = 0; i < _submission.renderPasses.size(); i++)
			{
				if(FrameSubmissionPassUsesFramePass(_submission.renderPasses[i], framePass))
					return i;
			}

			return InvalidIndex;
		}

		size_t FindFramebufferProducer(FramebufferType *framebuffer) const
		{
			if(!framebuffer) return InvalidIndex;

			for(size_t i = 0; i < _submission.renderPasses.size(); i++)
			{
				const RenderPassType &renderPass = _submission.renderPasses[i];
				if(renderPass.framebuffer == framebuffer || renderPass.resolveFramebuffer == framebuffer)
					return i;
			}

			return InvalidIndex;
		}

		void AddDependency(size_t consumerIndex, size_t producerIndex)
		{
			if(consumerIndex == InvalidIndex || producerIndex == InvalidIndex) return;
			if(consumerIndex == producerIndex) return;

			DependencyEdge dependency = { consumerIndex, producerIndex };
			_dependencies.push_back(dependency);
			_passStates[producerIndex].pruneIfUnused = true;
		}

		void AddTextureDependency(size_t consumerIndex, Texture *texture)
		{
			if(!texture) return;

			for(size_t producerIndex = 0; producerIndex < _submission.renderPasses.size(); producerIndex++)
			{
				if(PassProducesTexture(_submission.renderPasses[producerIndex], texture))
					AddDependency(consumerIndex, producerIndex);
			}
		}

		void BuildDependencies()
		{
			for(size_t i = 0; i < _submission.renderPasses.size(); i++)
			{
				RenderPassType &renderPass = _submission.renderPasses[i];

				if(renderPass.previousStoredFramebuffer)
					AddDependency(i, FindFramebufferProducer(renderPass.previousStoredFramebuffer));

				AddRenderFramePassSnapshotDependencies(i, renderPass.renderFramePassIndex);
				FrameSubmissionAddFramePassDependencies(*this, i, renderPass);
			}
		}

		void MarkLivePasses()
		{
			for(size_t i = 0; i < _submission.renderPasses.size(); i++)
			{
				const RenderPassType &renderPass = _submission.renderPasses[i];
				PassState &passState = _passStates[i];
				passState.skippedBySwapChain = PassTargetsSkippedSwapChain(renderPass);

				if(passState.skippedBySwapChain) continue;
				if(!passState.pruneIfUnused || PassTargetsSubmittedSwapChain(renderPass))
					passState.live = true;
			}
		}

		void PropagateLiveDependencies()
		{
			bool changed = true;
			while(changed)
			{
				changed = false;
				for(const DependencyEdge &dependency : _dependencies)
				{
					if(!_passStates[dependency.consumerIndex].live) continue;
					if(_passStates[dependency.producerIndex].skippedBySwapChain) continue;
					if(_passStates[dependency.producerIndex].live) continue;

					_passStates[dependency.producerIndex].live = true;
					changed = true;
				}
			}
		}

		void ErasePrunedPasses()
		{
			size_t index = 0;
			for(auto iterator = _submission.renderPasses.begin(); iterator != _submission.renderPasses.end();)
			{
				if(_passStates[index].skippedBySwapChain || !_passStates[index].live)
					iterator = _submission.renderPasses.erase(iterator);
				else
					iterator++;

				index++;
			}
		}

		Submission &_submission;
		std::vector<PassState> _passStates;
		std::vector<DependencyEdge> _dependencies;
	};
}

#endif /* __RAYNE_FRAMESUBMISSIONPRUNER_H__ */
