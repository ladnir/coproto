#pragma once
#include "coproto/Defines.h"
#include "coproto/Scheduler.h"


namespace coproto
{
	struct EndOfRound { };

#ifdef COPROTO_CPP20
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
#endif // COPROTO_CPP20

}

