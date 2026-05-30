//
//  RNFramePass.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_FRAMEPASS_H__
#define __RAYNE_FRAMEPASS_H__

#include "../Base/RNBase.h"
#include "../Objects/RNArray.h"
#include "../Objects/RNObject.h"

namespace RN
{
	class FramePass : public Object
	{
	public:
		RNAPI FramePass();
		RNAPI ~FramePass() override;

		RNAPI void AddFramePass(FramePass *framePass);
		RNAPI void RemoveFramePass(FramePass *framePass);
		RNAPI void RemoveAllFramePasses();
		const Array *GetNextFramePasses() const { return _nextFramePasses; }

		RNAPI void AddFramePassDependency(FramePass *framePass);
		RNAPI void RemoveFramePassDependency(FramePass *framePass);
		RNAPI void RemoveAllFramePassDependencies();
		const std::vector<WeakRef<FramePass>> &GetFramePassDependencies() const { return _framePassDependencies; }

	protected:
		RNAPI virtual void WillAddFramePass(FramePass *framePass) const;
		RNAPI virtual void DidAddFramePass(FramePass *framePass);
		RNAPI virtual void DidRemoveFramePass(FramePass *framePass);
		RNAPI virtual void DidRemoveAllFramePasses();

	private:
		Array *_nextFramePasses;
		std::vector<WeakRef<FramePass>> _framePassDependencies;

		__RNDeclareMetaInternal(FramePass)
	};
} // namespace RN

#endif /* __RAYNE_FRAMEPASS_H__ */
