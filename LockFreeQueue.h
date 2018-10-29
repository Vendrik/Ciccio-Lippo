#pragma once
#include<atomic>
#include<mutex>

template<class T, std::size_t size>
class LockFreeQueue {
private:
	
	std::size_t capacity;
	std::atomic<std::size_t> reader, writer;
	T data[size + 1];

public:

	LockFreeQueue() :
		reader(0), writer(0), capacity(size + 1)
	{

	};

	bool push(const T& elem)
	{
		std::size_t wr_pos = writer.load(std::memory_order_relaxed);
		std::size_t next_pos = (wr_pos + 1) % capacity;

		if (next_pos == reader.load(std::memory_order_acquire))
			return false;

		data[wr_pos] = elem;
		writer.store(next_pos, std::memory_order_release);

		return true;
	};

	bool pop(T& elem)
	{
		std::size_t rd_pos = reader.load(std::memory_order_relaxed);

		if (rd_pos == writer.load(std::memory_order_acquire))
			return false;

		elem = data[rd_pos];
		std::size_t next_pos = (rd_pos + 1) % capacity;

		reader.store(next_pos, std::memory_order_release);

		return true;
	};
};

template<class T, std::size_t size>
class UnboundedLockFreeQueue {

private:
	
	struct MemoryBlock {
		std::atomic<std::size_t> reader, writer;
		std::atomic<MemoryBlock*> next_block;
		T data[size + 1];

		MemoryBlock() : reader(0), writer(0), next_block(nullptr) {};

	};

	std::size_t capacity;
	MemoryBlock *head, *tail;
	std::atomic<bool> stop;

public:
	UnboundedLockFreeQueue() : capacity(size + 1), stop(false)
	{
		head = tail = new MemoryBlock();
	};

	~UnboundedLockFreeQueue()
	{
		stop.store(true, std::memory_order_release);

		auto current_head = head;

		for (auto next_block = current_head->next_block.load(std::memory_order_acquire); next_block != nullptr; next_block = head->next_block.load(std::memory_order_acquire))
		{
			head = next_block;
			current_head->next_block.store(nullptr, std::memory_order_release);
			delete current_head;

			current_head = head;
		}

		delete head;

	}

	bool push(const T& elem)
	{
		if (!stop.load(std::memory_order_acquire))
		{
			auto current_tail = tail;

			auto wr_pos = current_tail->writer.load(std::memory_order_relaxed);
			auto next_pos = (wr_pos + 1) % capacity;

			if (next_pos == current_tail->reader.load(std::memory_order_acquire)) {
				auto next_block = new MemoryBlock();
				if (next_block == nullptr)
					return false;

				current_tail->next_block.store(next_block, std::memory_order_release);
				current_tail = next_block;
				tail = current_tail;

				wr_pos = 0;
				next_pos = (wr_pos + 1) % capacity;

			}

			current_tail->data[wr_pos] = elem;
			current_tail->writer.store(next_pos, std::memory_order_release);

			return true;
		}

		return false;
	};

	bool pop(T& elem)
	{
		if (!stop.load(std::memory_order_acquire))
		{
			auto current_head = head;

			auto rd_pos = current_head->reader.load(std::memory_order_relaxed);
			if (rd_pos == current_head->writer.load(std::memory_order_acquire)) {

				auto next_block = current_head->next_block.load(std::memory_order_acquire);
				if (next_block == nullptr)
					return false;

				head = next_block;
				delete current_head;
				current_head = next_block;

				rd_pos = 0;
				if (rd_pos == current_head->writer.load(std::memory_order_acquire))
					return false;

			}

			elem = current_head->data[rd_pos];
			current_head->reader.store((rd_pos + 1) % capacity, std::memory_order_release);

			return true;
		}

		return false;
	}; 

};

// This class can be used to test performance differences between lock-free/mutexed implementations
template<class T, std::size_t size>
class MutexedQueue {
private:

	T data[size + 1];
	std::size_t capacity;
	std::mutex lock;
	std::size_t reader, writer;

public:
	MutexedQueue() : reader(0), writer(0), capacity(size + 1) {};

	bool push(const T& elem)
	{
		std::lock_guard<std::mutex> lg(lock);
		std::size_t next_pos = (writer + 1) % capacity;

		if (next_pos == reader)
			return false;

		data[writer] = elem;
		writer = next_pos;

		return true;

	};

	bool pop(T& elem)
	{
		std::lock_guard<std::mutex> lg(lock);

		if (reader == writer)
			return false;

		elem = data[reader];
		std::size_t next_pos = (reader + 1) % capacity;
		reader = next_pos;

		return true;

	};

};