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
			for(size_t i = 0; i < Capacity; ++i)
			{
				_buffer[i].seq.store(i, std::memory_order_relaxed);
			}
		}

		bool PushWithIndex(const T &value, size_t &outIndex)
		{
			for(;;)
			{
				size_t pos = _tail.load(std::memory_order_relaxed);
				size_t ticket = pos % Wrap;
				Slot &slot = _buffer[pos % Capacity];
				size_t seq = slot.seq.load(std::memory_order_acquire);
				const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

				if(diff == 0)
				{
					if(_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel))
					{
						slot.value = value;
						slot.seq.store((ticket + 1) % Wrap, std::memory_order_release);
						outIndex = pos % Capacity;
						return true;
					}
				}
				else if(diff < 0 && diff >= -static_cast<intptr_t>(Capacity))
				{
					return false; // full
				}
			}
		}

		bool PopWithIndex(T &value, size_t &outIndex)
		{
			for(;;)
			{
				size_t pos = _head.load(std::memory_order_relaxed);
				size_t ticket = pos % Wrap;
				Slot &slot = _buffer[pos % Capacity];
				size_t seq = slot.seq.load(std::memory_order_acquire);
				const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>((ticket + 1) % Wrap);

				if(diff == 0)
				{
					if(_head.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel))
					{
						outIndex = pos % Capacity;
						value = std::move(slot.value);
						slot.value = T();
						slot.seq.store((ticket + Capacity) % Wrap, std::memory_order_release);
						return true;
					}
				}
				else if(diff < 0 && diff >= -static_cast<intptr_t>(Capacity))
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
				size_t pos = _tail.load(std::memory_order_relaxed);
				size_t ticket = pos % Wrap;
				Slot &slot = _buffer[pos % Capacity];
				size_t seq = slot.seq.load(std::memory_order_acquire);
				const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

				if(diff == 0)
				{
					if(_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel))
					{
						slot.value = value;
						slot.seq.store((ticket + 1) % Wrap, std::memory_order_release);
						return true;
					}
				}
				else if(diff < 0 && diff >= -static_cast<intptr_t>(Capacity))
				{
					return false; // full
				}
			}
		}

		bool Push(T &&value)
		{
			for(;;)
			{
				size_t pos = _tail.load(std::memory_order_relaxed);
				size_t ticket = pos % Wrap;
				Slot &slot = _buffer[pos % Capacity];
				size_t seq = slot.seq.load(std::memory_order_acquire);
				const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(ticket);

				if(diff == 0)
				{
					if(_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel))
					{
						slot.value = std::move(value);
						slot.seq.store((ticket + 1) % Wrap, std::memory_order_release);
						return true;
					}
				}
				else if(diff < 0 && diff >= -static_cast<intptr_t>(Capacity))
				{
					return false; // full
				}
			}
		}

		bool Pop(T &value)
		{
			for(;;)
			{
				size_t pos = _head.load(std::memory_order_relaxed);
				size_t ticket = pos % Wrap;
				Slot &slot = _buffer[pos % Capacity];
				size_t seq = slot.seq.load(std::memory_order_acquire);
				const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>((ticket + 1) % Wrap);

				if(diff == 0)
				{
					if(_head.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel))
					{
						value = std::move(slot.value);
						slot.value = T();
						slot.seq.store((ticket + Capacity) % Wrap, std::memory_order_release);
						return true;
					}
				}
				else if(diff < 0 && diff >= -static_cast<intptr_t>(Capacity))
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
		static RN_CONSTEXPR size_t Wrap = Capacity * 2;

		struct Slot
		{
			std::atomic<size_t> seq;
			T value;
		};

		std::atomic<size_t> _head;
		std::atomic<size_t> _tail;

		std::array<Slot, Capacity> _buffer;
	};
} // namespace RN

#endif /* __RAYNE_ATOMICRINGBUFFER_H__ */
