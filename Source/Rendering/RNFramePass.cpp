//
//  RNFramePass.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNFramePass.h"

namespace RN
{
	RNDefineMeta(FramePass, Object)

	FramePass::FramePass() :
		_nextFramePasses(new Array())
	{}

	FramePass::~FramePass()
	{
		SafeRelease(_nextFramePasses);
	}

	void FramePass::AddFramePass(FramePass *framePass)
	{
		RN_ASSERT(framePass, "Cannot add an empty frame pass");

		WillAddFramePass(framePass);
		_nextFramePasses->AddObject(framePass);
		DidAddFramePass(framePass);
	}

	void FramePass::RemoveFramePass(FramePass *framePass)
	{
		_nextFramePasses->RemoveObject(framePass);
		DidRemoveFramePass(framePass);
	}

	void FramePass::RemoveAllFramePasses()
	{
		_nextFramePasses->RemoveAllObjects();
		DidRemoveAllFramePasses();
	}

	void FramePass::AddFramePassDependency(FramePass *framePass)
	{
		RN_ASSERT(framePass, "Cannot add an empty frame pass dependency");
		RN_ASSERT(framePass != this, "Cannot add a frame pass dependency on itself");

		for(const WeakRef<FramePass> &dependency : _framePassDependencies)
		{
			if(dependency.Load() == framePass) return;
		}

		_framePassDependencies.push_back(framePass);
	}

	void FramePass::RemoveFramePassDependency(FramePass *framePass)
	{
		for(auto iterator = _framePassDependencies.begin(); iterator != _framePassDependencies.end();)
		{
			if(iterator->Load() == framePass)
				iterator = _framePassDependencies.erase(iterator);
			else
				iterator++;
		}
	}

	void FramePass::RemoveAllFramePassDependencies()
	{
		_framePassDependencies.clear();
	}

	void FramePass::WillAddFramePass(FramePass *) const
	{}

	void FramePass::DidAddFramePass(FramePass *)
	{}

	void FramePass::DidRemoveFramePass(FramePass *)
	{}

	void FramePass::DidRemoveAllFramePasses()
	{}
} // namespace RN
