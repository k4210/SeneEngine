#pragma once

#include<atomic>
#include<optional>
#include<assert.h>

namespace MessageQueue
{
	template<typename T>
	struct TNode
	{
		std::byte* GetRaw()
		{
			return data;
		}

		T& GetCasted()
		{
			return *reinterpret_cast<T*>(data);
		}
	private:
		alignas(T) std::byte data[sizeof(T)];
	public:
		TNode<T>* next = nullptr;
	};

	template<typename T>
	struct TLockFreeStack
	{
		using Node = TNode<T>;

		void Push(Node& new_node)
		{
			new_node.next = head.load(std::memory_order_relaxed);

			while (!head.compare_exchange_weak(new_node.next, &new_node,
				std::memory_order_release,
				std::memory_order_relaxed))
				;
		}

		Node* Pop()
		{
			Node* top = head.load(std::memory_order_relaxed);
			do
			{
				if (!top)
				{
					return nullptr;
				}
			} while (!head.compare_exchange_weak(top, top->next));
			top->next = nullptr;
			return top;
		}

	private:
		std::atomic<Node*> head = nullptr;
	};

	template<typename T>
	struct TQueueState
	{
		using Node = TNode<T>;
		Node* first_ = nullptr; //Consume from here
		Node* last_ = nullptr; //Add here
	};

	template<typename T>
	struct TLockFreeQueue
	{
		using Node = TNode<T>;

		void Enqueue(Node& new_node)
		{
			//assert(state_.is_lock_free());
			TQueueState<T> state = state_.load(std::memory_order_relaxed);
			TQueueState<T> new_state;

			do
			{
				new_state.last_ = &new_node;
				new_state.first_ = state.first_ ? state.first_ : &new_node;
				new_node.next = state.last_;
			} while (!state_.compare_exchange_weak(state, new_state));
		}

		Node* Pop()
		{
			TQueueState<T> state = state_.load(std::memory_order_relaxed);
			TQueueState<T> new_state;

			do
			{
				if (!state.first_)
				{
					return nullptr;
				}
				new_state.first_ = state.first_->next;
				new_state.last_ = (state.last_ == state.first_) ? nullptr : state.last_;
			} while (!state_.compare_exchange_weak(state, new_state));
			state.first_->next = nullptr;
			return state.first_;
		}

		TQueueState<T> TakeAll()
		{
			TQueueState<T> state = state_.load(std::memory_order_relaxed);
			while (!state_.compare_exchange_weak(state, TQueueState<T>{}))
				;
			return state;
		}

	private:
		std::atomic<TQueueState<T>> state_;
	};

	template<typename T>
	struct TMemoryPool
	{
		using Node = TNode<T>;

		// No move, no copy
		TMemoryPool() = default;
		~TMemoryPool()
		{
			size_t all_pool_elements = all_pool_elements_;
			assert(free_elements_num_ == all_pool_elements);
			assert(!preallocated_array_ == !preallocated_size_);
			if (preallocated_array_)
			{
				delete[] preallocated_array_;
				all_pool_elements -= preallocated_size_;
			}

			if (!all_pool_elements)
			{
				return;
			}

			auto IsPreallocated = [&](Node* node) -> bool
			{
				return preallocated_array_ 
					&& (node >= preallocated_array_) 
					&& (node < (preallocated_array_ + preallocated_size_));
			};

			while (Node* node = free_elements_.Pop())
			{
				if (!IsPreallocated(node))
				{
					delete node;
					--all_pool_elements;
				}
			}

			assert(!all_pool_elements);
		}

		void Preallocate(const size_t delta)
		{
			assert(!preallocated_size_ && !preallocated_array_);
			preallocated_array_ = IncreasePool(delta);
			preallocated_size_ = delta;
			free_elements_num_ += preallocated_size_;
		}

		Node& Give()
		{
			if (Node* node = free_elements_.Pop())
			{
				free_elements_num_--;
				return *node;
			}

			all_pool_elements_++;
			Node* new_element = new Node;
			return *new_element;
		}

		void Take(Node& node)
		{
			free_elements_.Push(node);
			free_elements_num_++;
		}

		size_t GetNumOfUsed() const
		{
			return all_pool_elements_.load(std::memory_order_relaxed)
				- free_elements_num_.load(std::memory_order_relaxed);
		}

	private:
		Node* IncreasePool(const size_t delta = 1)
		{
			if (!delta)
			{
				return nullptr;
			}

			all_pool_elements_ += delta;
			Node* new_elements = new Node[delta];
			for (size_t idx = 0; idx < delta; ++idx)
			{
				free_elements_.Push(new_elements[idx]);
			}
			return new_elements;
		}

		MessageQueue::TLockFreeStack<T> free_elements_;
		std::atomic_size_t free_elements_num_ = 0;
		std::atomic_size_t all_pool_elements_ = 0;

		size_t preallocated_size_ = 0;
		Node* preallocated_array_ = nullptr;
	};
}

template<typename T>
struct TMessageQueue
{
	using Node = MessageQueue::TNode<T>;

	TMessageQueue() = default;
	//No move, no copy

	~TMessageQueue()
	{
		while (Node * node = messages_.Pop())
		{
			T& node_data = node->GetCasted();
			node_data.~T();
			memory_pool_.Take(*node);
		}
	}

	void Preallocate(const size_t num)
	{
		memory_pool_.Preallocate(num);
	}

	std::optional<T> Pop()
	{
		if (Node* node = messages_.Pop())
		{
			assert(memory_pool_.GetNumOfUsed());
			T& node_data = node->GetCasted();
			T msg = std::move(node_data);
			node_data.~T();
			
			memory_pool_.Take(*node);
			return msg;
		}
		return {};
	}

	void Enqueue(T&& msg)
	{
		Node& node = memory_pool_.Give();
		assert(memory_pool_.GetNumOfUsed());
		new(node.GetRaw()) T(std::forward<T>(msg));
		messages_.Enqueue(node);
	}

private:
	MessageQueue::TLockFreeQueue<T> messages_;
	MessageQueue::TMemoryPool<T> memory_pool_;
	//TQueueState pending_;
};



