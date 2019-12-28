#pragma once

#include<atomic>
#include<optional>
#include<assert.h>

template<typename T, uint32_t kSize> 
class LockFreeQueue_SingleConsumer
{
	LockFreeQueue_SingleConsumer(const LockFreeQueue_SingleConsumer&) = delete;
	LockFreeQueue_SingleConsumer& operator=(const LockFreeQueue_SingleConsumer&) = delete;
	LockFreeQueue_SingleConsumer(const LockFreeQueue_SingleConsumer&&) = delete;
	LockFreeQueue_SingleConsumer& operator=(const LockFreeQueue_SingleConsumer&&) = delete;

	struct Block
	{
		T data[kSize];
		std::atomic_bool written[kSize] = { false };
		Block* next = nullptr;
	};

	struct alignas(8) State
	{
		Block* head = nullptr;
		uint16_t first = 0;
		uint16_t count = 0;
#ifndef NDEBUG
		uint16_t num_blocks = 0;
#endif
	};
	std::atomic<State> state_;
	std::atomic<Block*> free_list_head_ = nullptr;;

	void MoveToFreeList(Block* block)
	{
		//no need to check if all data were already read. It's single consumer queue
		assert(block);
		assert([&]() -> bool 
		{
			for (auto& written : block->written)
			{
				if (written) 
					return false;
			}
			return true; 
		}());
		assert(!block->next);
		Block* local_head = free_list_head_;
		do
		{
			block->next = local_head;
		} 
		while (!free_list_head_.compare_exchange_weak(local_head, block));
	}

	Block* GetBlockFromFreeList()
	{
		//no need to check if all data were already read. It's single consumer queue
		Block* local_head = free_list_head_;
		do
		{
			if (!local_head)
				return nullptr;
		} 
		while (!free_list_head_.compare_exchange_weak(local_head, local_head->next));
		local_head->next = nullptr;
		assert([&]() -> bool
		{
			for (auto& written : local_head->written)
			{
				if (written) 
					return false;
			}
			return true;
		}());
		return local_head;
	}

	Block* GetOrAllocateFreeBlock()
	{
		Block* block = GetBlockFromFreeList();
		return block ? block : new Block();
	}

public:
	LockFreeQueue_SingleConsumer(uint32_t initial_blocks = 3)
	{
		for (uint32_t i = 0; i < initial_blocks; i++)
		{
			MoveToFreeList(new Block());
		}
	}
	~LockFreeQueue_SingleConsumer()
	{
		for (Block* it = state_.load().head; it;)
		{
			Block* to_delete = it;
			it = it->next;
			delete to_delete;
		}

		for (Block* it = free_list_head_; it;)
		{
			Block* to_delete = it;
			it = it->next;
			delete to_delete;
		}
	}

	void Enqueue(T&& item)
	{
		uint16_t local_first = 0;
		Block* local_head = nullptr;
		State prev_state = state_;
		{
			State next_state;
			Block* new_block = nullptr;
			do
			{
				assert(prev_state.count > 0 || !prev_state.head || 1 == prev_state.num_blocks);
				if (!prev_state.first && (prev_state.count > 0 || !prev_state.head))
				{
					if (!new_block)
					{
						new_block = GetOrAllocateFreeBlock();
						assert(new_block);
					}
					new_block->next = prev_state.head;
				}
				else if(new_block)
				{
					new_block->next = nullptr;
					MoveToFreeList(new_block);
					new_block = nullptr;
				}		
				local_head = new_block ? new_block : prev_state.head;
				local_first = (prev_state.first - 1) % kSize;
				const uint16_t new_count = prev_state.count + 1;
#ifndef NDEBUG
				const uint16_t new_num_blocks = prev_state.num_blocks + (new_block ? 1 : 0);
#endif	
				next_state = { local_head, local_first, new_count
#ifndef NDEBUG
					, new_num_blocks
#endif				
				};
			} while (!state_.compare_exchange_weak(prev_state, next_state));
		}
		local_head->data[local_first] = item;
#ifdef NDEBUG
		local_head->written[local_first] = true;
#else
		assert(local_first < kSize);
		const bool bWasOccupied = local_head->written[local_first].exchange(true);
		assert(!bWasOccupied);
#endif
	}

	std::optional<T> Pop()
	{
		State prev_state = state_;
		{
			State next_state;
			do
			{
				if (!prev_state.count)
					return {};
				const uint16_t new_count = prev_state.count - 1;
				next_state = { prev_state.head, prev_state.first, new_count 
#ifndef NDEBUG
					, prev_state.num_blocks
#endif			
				};
			} while (!state_.compare_exchange_weak(prev_state, next_state));
		}
		assert(prev_state.first < kSize);

		int index_in_block = 0;
		Block* block = prev_state.head;
		Block* prev_block = nullptr;
		const uint32_t consecutive_index = prev_state.first + prev_state.count - 1;
		assert(consecutive_index < prev_state.num_blocks * kSize);
		{
			for (uint32_t it = 0; it < consecutive_index; it += kSize)
			{
				assert(block);
				const int idx_from_current_block_start = consecutive_index - it;
				if (idx_from_current_block_start < kSize)
				{
					index_in_block = idx_from_current_block_start;
					break;
				}
				assert(block->next);
				prev_block = block;
				block = block->next;
			}
		}
		assert(block);
		assert(index_in_block < kSize);
		while (!block->written[index_in_block]) {} //wait for producer to finish writing. TODO: make it atomic_wait
		std::optional<T> result(std::move(block->data[index_in_block]));
		
#ifdef NDEBUG
		block->written[index_in_block] = false;
#else
		const bool bWasOccupied = block->written[index_in_block].exchange(false);
		assert(bWasOccupied);
#endif

		if (0 == index_in_block)
		{
			assert(!block->next);
			if (prev_block)
			{
				assert(prev_block->next == block);
				prev_block->next = nullptr;
				MoveToFreeList(block);
			}
#ifndef NDEBUG
			{
				State next_state;
				do
				{
					assert(prev_state.num_blocks > 0);
					const uint16_t num_blocks = prev_state.num_blocks - (prev_block ? 1 : 0);
					next_state = { prev_state.head, prev_state.first, prev_state.count, num_blocks };
				} while (!state_.compare_exchange_weak(prev_state, next_state));
			}
#endif	
		}

		return result;
	}

	uint32_t Num() const { return state_.load().count; }

	void ClearFreeList()
	{
		for (Block* block = GetBlockFromFreeList(); block; block = GetBlockFromFreeList())
		{
			delete block;
		}
	}
};
