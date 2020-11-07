#pragma once
#include <mutex>
#include <condition_variable>
#include <queue>


namespace coproto
{

	template <typename T>
	class BlockingQueue
	{
	private:
		std::mutex              d_mutex;
		std::condition_variable d_condition;
		std::deque<T>           d_queue;
	public:
		void push(T&& value) {
			{
				std::unique_lock<std::mutex> lock(this->d_mutex);
				d_queue.emplace_front(std::move(value));
			}
			this->d_condition.notify_one();
		}

		template<typename... Args>
		void emplace(Args&&... args) {
			{
				std::unique_lock<std::mutex> lock(this->d_mutex);
				d_queue.emplace_front(std::forward<Args>(args)...);
			}
			this->d_condition.notify_one();
		}

		T pop() {
			std::unique_lock<std::mutex> lock(this->d_mutex);
			this->d_condition.wait(lock, [this] { return !this->d_queue.empty(); });
			T rc(std::move(this->d_queue.back()));
			this->d_queue.pop_back();
			return rc;
		}
	};

}