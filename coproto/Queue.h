#pragma once
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>


namespace coproto
{
	template<typename T, typename... Args>
	std::unique_ptr<T> make_unique(Args&&... args)
	{
		return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
	}

	template <typename T>
	class BlockingQueue
	{
	private:
		struct State
		{
			std::mutex              d_mutex;
			std::condition_variable d_condition;
			std::deque<T>           d_queue;
		};

		std::unique_ptr<State> mState;
	public:

		BlockingQueue()
			: mState(make_unique<State>())
		{}


		BlockingQueue(BlockingQueue&& q)
			: mState(std::move(q.mState))
		{
			q.mState = make_unique<State>();
		}

		BlockingQueue& operator=(BlockingQueue&& q)
		{
			mState = (std::move(q.mState));
			q.mState = make_unique<State>();
			return *this;
		}


		void clear()
		{
			std::unique_lock<std::mutex> lock(mState->d_mutex);
			mState->d_queue.clear();
		}


		void push(T&& value) {
			{
				std::unique_lock<std::mutex> lock(mState->d_mutex);
				mState->d_queue.emplace_front(std::move(value));
			}
			mState->d_condition.notify_one();
		}

		template<typename... Args>
		void emplace(Args&&... args) {
			{
				std::unique_lock<std::mutex> lock(mState->d_mutex);
				mState->d_queue.emplace_front(std::forward<Args>(args)...);
			}
			mState->d_condition.notify_one();
		}

		T pop() {
			std::unique_lock<std::mutex> lock(mState->d_mutex);
			mState->d_condition.wait(lock, [this] { return !mState->d_queue.empty(); });
			T rc(std::move(mState->d_queue.back()));
			mState->d_queue.pop_back();
			return rc;
		}


		T popWithSize(u64& size) {
			std::unique_lock<std::mutex> lock(mState->d_mutex);
			mState->d_condition.wait(lock, [this] { return !mState->d_queue.empty(); });
			T rc(std::move(mState->d_queue.back()));
			mState->d_queue.pop_back();
			size = mState->d_queue.size();
			return rc;
		}
	};

}