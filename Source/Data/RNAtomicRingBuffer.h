//
//  RNAtomicRingBuffer.h
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_ATOMICRINGBUFFER_H__
#define __RAYNE_ATOMICRINGBUFFER_H__

#include "../Base/RNBase.h"

namespace RN
{
	template<class T, size_t Size>
	class AtomicRingBuffer
	{
	public:
		AtomicRingBuffer() :
			_head(0),
			_tail(0)
		{
			for(uint64 i = 0; i < Capacity; ++i)
			{
				_buffer[i].seq.store(i, std::memory_order_relaxed);
			}
		}

		bool PushWithIndex(const T &value, size_t &outIndex)
		{
			for(;;)
			{
				uint64 pos = _tail.load(std::memory_order_relaxed);
				Slot &slot = _buffer[static_cast<size_t>(pos % Capacity)];
				uint64 seq = slot.seq.load(std::memory_order_acquire);
				const int64 diff = static_cast<int64>(seq) - static_cast<int64>(pos);

				if(diff == 0)
				{
					if(_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel))
					{
						slot.value = value;
						slot.seq.store(pos + 1, std::memory_order_release);
						outIndex = static_cast<size_t>(pos % Capacity);
						return true;
					}
				}
				else if(diff < 0)
				{
					return false; // full
				}
			}
		}

		bool PopWithIndex(T &value, size_t &outIndex)
		{
			for(;;)
			{
				uint64 pos = _head.load(std::memory_order_relaxed);
				Slot &slot = _buffer[static_cast<size_t>(pos % Capacity)];
				uint64 seq = slot.seq.load(std::memory_order_acquire);
				const int64 diff = static_cast<int64>(seq) - static_cast<int64>(pos + 1);

				if(diff == 0)
				{
					if(_head.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel))
					{
						outIndex = static_cast<size_t>(pos % Capacity);
						value = std::move(slot.value);
						slot.value = T();
						slot.seq.store(pos + Capacity, std::memory_order_release);
						return true;
					}
				}
				else if(diff < 0)
				{
					return false; // empty
				}
			}
		}

		void NullSlot(size_t index)
		{
			if(index < Capacity)
			{
				_buffer[index].value = T();
			}
		}

		bool Push(const T &value)
		{
			for(;;)
			{
				uint64 pos = _tail.load(std::memory_order_relaxed);
				Slot &slot = _buffer[static_cast<size_t>(pos % Capacity)];
				uint64 seq = slot.seq.load(std::memory_order_acquire);
				const int64 diff = static_cast<int64>(seq) - static_cast<int64>(pos);

				if(diff == 0)
				{
					if(_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel))
					{
						slot.value = value;
						slot.seq.store(pos + 1, std::memory_order_release);
						return true;
					}
				}
				else if(diff < 0)
				{
					return false; // full
				}
			}
		}

		bool Push(T &&value)
		{
			for(;;)
			{
				uint64 pos = _tail.load(std::memory_order_relaxed);
				Slot &slot = _buffer[static_cast<size_t>(pos % Capacity)];
				uint64 seq = slot.seq.load(std::memory_order_acquire);
				const int64 diff = static_cast<int64>(seq) - static_cast<int64>(pos);

				if(diff == 0)
				{
					if(_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel))
					{
						slot.value = std::move(value);
						slot.seq.store(pos + 1, std::memory_order_release);
						return true;
					}
				}
				else if(diff < 0)
				{
					return false; // full
				}
			}
		}

		bool Pop(T &value)
		{
			for(;;)
			{
				uint64 pos = _head.load(std::memory_order_relaxed);
				Slot &slot = _buffer[static_cast<size_t>(pos % Capacity)];
				uint64 seq = slot.seq.load(std::memory_order_acquire);
				const int64 diff = static_cast<int64>(seq) - static_cast<int64>(pos + 1);

				if(diff == 0)
				{
					if(_head.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel))
					{
						value = std::move(slot.value);
						slot.value = T();
						slot.seq.store(pos + Capacity, std::memory_order_release);
						return true;
					}
				}
				else if(diff < 0)
				{
					return false; // empty
				}
			}
		}

		bool WasEmpty() const
		{
			return (_head.load(std::memory_order_acquire) == _tail.load(std::memory_order_acquire));
		}

		bool IsLockFree() const
		{
			return (_head.is_lock_free() && _tail.is_lock_free());
		}

	private:
		static RN_CONSTEXPR size_t Capacity = Size;

		struct Slot
		{
			std::atomic<uint64> seq;
			T value;
		};

		std::atomic<uint64> _head;
		std::atomic<uint64> _tail;

		std::array<Slot, Capacity> _buffer;
	};
} // namespace RN

#endif /* __RAYNE_ATOMICRINGBUFFER_H__ */
