#pragma once
#include "Defines.h"
#include "Scheduler.h"


namespace coproto
{
	struct EndOfRound { };

	struct EndOfRoundAwaiter
	{
		using coro_handle = std::coroutine_handle<void>;
		bool mReady;
		EndOfRoundAwaiter(bool ready)
			:mReady(ready)
		{}

		bool await_ready()
		{
			//	if (mParent->mSched->mReady.size())
			//	{
			//		mParent->mSched->mReady.push_back(mParent);
			//		return false;
			//	}
			//	return true;

			return mReady;
		}
		void await_suspend(coro_handle h) { }

		void await_resume()
		{
		}
	};
}

