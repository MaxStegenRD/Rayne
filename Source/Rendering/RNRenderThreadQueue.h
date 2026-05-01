//
//  RNRenderThreadQueue.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_RENDERTHREADQUEUE_H_
#define __RAYNE_RENDERTHREADQUEUE_H_

#include "../Base/RNFunction.h"
#include "RNRenderingConfig.h"

namespace RN
{
	template<class T>
	class RenderThreadQueue
	{
	public:
		enum class WorkType
		{
			None,
			FrameSubmission,
			Task
		};

		bool WaitForSpace()
		{
			RN_PROFILE_SCOPE_N("WaitForRenderFrameQueueSpace");
			RN_PROFILE_ATRACE_SCOPE_N("RN RenderQueue WaitForFrameSlot");
			UniqueLock<Lockable> lock(_lock);
			_consumedCondition.Wait(lock, [this]() {
				return _isShuttingDown || _queuedFrameSubmissionCount < RN_RENDERING_FRAME_SUBMISSION_QUEUE_SIZE;
			});

			return !_isShuttingDown;
		}

		void Push(T &&submission)
		{
			UniqueLock<Lockable> lock(_lock);
			if(_isShuttingDown)
				return;

			RN_ASSERT(_queuedFrameSubmissionCount < RN_RENDERING_FRAME_SUBMISSION_QUEUE_SIZE, "Render thread frame submission queue is full");
			_workItems.emplace_back(std::move(submission));
			_queuedFrameSubmissionCount += 1;
			_queuedCondition.NotifyOne();
		}

		void PushTask(Function &&task)
		{
			UniqueLock<Lockable> lock(_lock);
			if(_isShuttingDown)
				return;

			_workItems.emplace_back(std::move(task));
			_queuedCondition.NotifyOne();
		}

		WorkType Pop(T &submission, Function &task)
		{
			RN_PROFILE_ATRACE_SCOPE_N("RN RenderQueue PopWork");
			UniqueLock<Lockable> lock(_lock);
			_queuedCondition.Wait(lock, [this]() {
				return _isShuttingDown || !_workItems.empty();
			});

			if(_isShuttingDown || _workItems.empty())
				return WorkType::None;

			WorkItem item(std::move(_workItems.front()));
			_workItems.pop_front();

			if(item.type == WorkType::FrameSubmission)
			{
				submission = std::move(item.submission);
				_queuedFrameSubmissionCount -= 1;
				_consumedCondition.NotifyOne();
				return WorkType::FrameSubmission;
			}

			task = std::move(item.task);
			return WorkType::Task;
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
			_workItems.clear();
			_queuedFrameSubmissionCount = 0;
			_queuedCondition.NotifyAll();
			_consumedCondition.NotifyAll();
		}

	private:
		struct WorkItem
		{
			WorkItem(T &&submission) :
				type(WorkType::FrameSubmission),
				submission(std::move(submission))
			{}

			WorkItem(Function &&task) :
				type(WorkType::Task),
				task(std::move(task))
			{}

			WorkItem(WorkItem &&other) RN_NOEXCEPT = default;
			WorkItem &operator=(WorkItem &&other) RN_NOEXCEPT = default;

			WorkType type;
			T submission;
			Function task;
		};

		std::deque<WorkItem> _workItems;
		Lockable _lock;
		Condition _queuedCondition;
		Condition _consumedCondition;
		size_t _queuedFrameSubmissionCount = 0;
		bool _isShuttingDown = false;
	};
}

#endif /* __RAYNE_RENDERTHREADQUEUE_H_ */
