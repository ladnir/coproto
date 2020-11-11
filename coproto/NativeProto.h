#pragma once
#include "coproto/Resumable.h"
#include "coproto/Proto.h"
#include "coproto/Buffers.h"

namespace coproto
{

	struct NativeProto : public Resumable
	{
		error_code mEc = {};
		std::exception_ptr mExPtr = nullptr;
		u64 mState = 0;


		bool mDone = false;
		bool mRoE = false;
		Scheduler* mSched = nullptr;


		virtual error_code resume() = 0;
		bool& resumeOnError() { return mRoE; }

		internal::InlinePoly<Resumable, internal::inlineSize> mAsyncOp;

		error_code initOp()
		{
			assert(mAsyncOp.get() && mAsyncOp->done() == false);

			mSched->addDep(*this, *mAsyncOp.get());
			auto ec = mSched->resume(mAsyncOp.get());

			return ec;
		}

		template<typename Container>
		error_code send(Container& cont)
		{
			assert(mAsyncOp.get() == nullptr || mAsyncOp->done());
			mAsyncOp.emplace<RefSendProto<Container>>(cont);

			return initOp();
		}

		template<typename Container>
		error_code send(Container&& cont)
		{
			assert(mAsyncOp.get() == nullptr || mAsyncOp->done());
			mAsyncOp.emplace<MvSendProto<Container>>(std::forward<Container>(cont));

			return initOp();
		}


		template<typename Container>
		error_code recv(Container& cont)
		{
			assert(mAsyncOp.get() == nullptr || mAsyncOp->done());
			mAsyncOp.emplace<RefRecvProto<Container>>(cont);

			return initOp();
		}


		error_code await(Proto&& proto)
		{
			assert(mAsyncOp.get() == nullptr || mAsyncOp->done());
			mAsyncOp = std::move(proto.mBase);

			return initOp();
		}

		error_code endOfRound()
		{
			if (mSched->mReady.size())
				return code::suspend;
			else
				return {};
		}

		error_code getAwaitResult()
		{
			return mEc;
		}

		error_code resume_(Scheduler& sched) override
		{
			mSched = &sched;
			//if (mEc = code::suspend)
			//	mEc = {};

			assert(mAsyncOp.get() == nullptr || mAsyncOp->done());

			try
			{
				if (!mEc || (mEc && mRoE))
				{
					auto ec = resume();
					if (ec != code::suspend)
					{
						mDone = true;
						mEc = ec;
					}
				}
				else
					mDone = true;

			}
			catch (...)
			{
				mEc = code::uncaughtException;
				mExPtr = std::current_exception();
				mDone = true;
			}

			if (done())
			{
				assert(mEc != code::suspend);
				mSched->fulfillDep(*this, mEc, std::move(mExPtr));
				return mEc;
			}

			return code::suspend;
		}
		bool done() override
		{
			return mDone;
		}
		void setError(error_code e, std::exception_ptr p) override
		{
			assert(e != code::suspend);
			mEc = e;
			mExPtr = p;
		}
		std::exception_ptr getExpPtr() override { return mExPtr; }
		error_code getErrorCode() override { return mEc; }

	};

	template<typename P, typename... Args>
	Proto makeProto(Args&&... args)
	{
		Proto p;
		p.mBase.emplace<P>(std::forward<Args>(args)...);
		return p;
	}


	namespace tests
	{
		void Native_StrSendRecv_Test();

		void Native_ZeroSendRecv_Test();
		void Native_ZeroSend_ErrorCode_Test();

		void Native_BadRecvSize_Test();
		void Native_BadRecvSize_ErrorCode_Test();

		void Native_NestedSendRecv_Test();
	}


}