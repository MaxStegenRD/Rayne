//
//  RNFrameSubmissionQueue.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_FRAMESUBMISSIONQUEUE_H_
#define __RAYNE_FRAMESUBMISSIONQUEUE_H_

#include "RNRenderingConfig.h"

namespace RN
{
	template<class T>
	class FrameSubmissionQueue
	{
	public:
		bool WaitForSpace()
		{
			UniqueLock<Lockable> lock(_lock);
			_consumedCondition.Wait(lock, [this]() {
				return _isShuttingDown || _submissions.size() < RN_RENDERING_FRAME_SUBMISSION_QUEUE_SIZE;
			});

			return !_isShuttingDown;
		}

		bool Push(T &&submission)
		{
			UniqueLock<Lockable> lock(_lock);
			if(_isShuttingDown)
				return false;

			RN_ASSERT(_submissions.size() < RN_RENDERING_FRAME_SUBMISSION_QUEUE_SIZE, "Frame submission queue is full");
			_submissions.push_back(std::move(submission));
			_queuedCondition.NotifyOne();
			return true;
		}

		bool Pop(T &submission)
		{
			UniqueLock<Lockable> lock(_lock);
			_queuedCondition.Wait(lock, [this]() {
				return _isShuttingDown || !_submissions.empty();
			});

			if(_submissions.empty())
				return false;

			submission = std::move(_submissions.front());
			_submissions.erase(_submissions.begin());
			_consumedCondition.NotifyOne();
			return true;
		}

		void Shutdown()
		{
			UniqueLock<Lockable> lock(_lock);
			_isShuttingDown = true;
			_queuedCondition.NotifyAll();
			_consumedCondition.NotifyAll();
		}

		void Drain()
		{
			UniqueLock<Lockable> lock(_lock);
			_submissions.clear();
			_queuedCondition.NotifyAll();
			_consumedCondition.NotifyAll();
		}

	private:
		std::vector<T> _submissions;
		Lockable _lock;
		Condition _queuedCondition;
		Condition _consumedCondition;
		bool _isShuttingDown = false;
	};
}

#endif /* __RAYNE_FRAMESUBMISSIONQUEUE_H_ */
