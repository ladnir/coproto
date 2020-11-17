#pragma once
#include "coproto/Resumable.h"
#include "coproto/Proto.h"
#include "coproto/Buffers.h"

namespace coproto
{
	template<typename ReturnType>
	struct NativeProtoV : public Resumable, public internal::ReturnStorage<ReturnType>
	{
		error_code mEc = {};
		std::exception_ptr mExPtr = nullptr;
		u64 _resume_idx_ = 0;
#ifdef COPROTO_LOGGING
		u64 mProtoIdx;
#endif
		using return_type = ReturnType;

		bool mDone = false;
		//bool mRoE = false;
		Scheduler* mSched = nullptr;

		NativeProtoV() {
#ifdef COPROTO_LOGGING
			mProtoIdx = gProtoIdx++; 
			setName("NativeProto_" + std::to_string(mProtoIdx));
#endif
		}
		NativeProtoV(const NativeProtoV&) = delete;
		NativeProtoV(NativeProtoV&&) = default;



		virtual error_code resume() = 0;
		//bool& resumeOnError() { return mRoE; }

		internal::InlinePoly<Resumable, internal::inlineSize> mAsyncOp;

		//error_code initOp()
		//{
		//	assert(mAsyncOp.get() && mAsyncOp->done() == false);

		//	mSched->addDep(*this, *mAsyncOp.get());
		//	auto ec = mSched->resume(mAsyncOp.get());

		//	return ec;
		//}

		//template<typename Container>
		//error_code send(Container& cont)
		//{
		//	assert(mAsyncOp.get() == nullptr || mAsyncOp->done());
		//	mAsyncOp.emplace<RefSendProto<Container>>(cont);

		//	return initOp();
		//}

		//template<typename Container>
		//error_code send(Container&& cont)
		//{
		//	assert(mAsyncOp.get() == nullptr || mAsyncOp->done());
		//	mAsyncOp.emplace<MvSendProto<Container>>(std::forward<Container>(cont));

		//	return initOp();
		//}


		error_code await(const EndOfRound&)
		{
			if (mSched->mReady.size())
			{
				mSched->mReady.push_back(this);
				return code::suspend;
			}
			return {};
		}

		template<typename T>
		error_code await(ProtoV<T>&& proto)
		{
			assert(mAsyncOp.get() == nullptr || mAsyncOp->done());
			mAsyncOp = std::move(proto.mBase);

			mSched->addDep(*this, *mAsyncOp.get());
			auto ec = mSched->resume(mAsyncOp.get());

			return ec;
		}


		template<typename T>
		error_code await(Async<T>&& proto)
		{
			assert(mAsyncOp.get() == nullptr || mAsyncOp->done());
			//mAsyncOp = std::move(proto.mBase.get()->mBase);

			if (proto.mBase.get()->mBase->done())
				return {};
			else
				mSched->addDep(*this, *proto.mBase.get()->mBase.get());

			return code::suspend;
		}

		template<typename T>
		error_code await(Async<T>& proto)
		{
			//mAsyncOp = std::move(proto.mBase.get()->mBase);
			if (proto.mBase.get()->mBase->done())
				return proto.mBase.get()->mBase->getErrorCode();
			else
				mSched->addDep(*this, *proto.mBase.get()->mBase.get());

			return code::suspend;
		}

		void* getAwaitReturn()
		{
			return mAsyncOp->getValue();
		}

		void* getValue() override
		{
			return internal::ReturnStorage<return_type>::getValue();
		}

		error_code resume_(Scheduler& sched) override
		{
			mSched = &sched;
			//if (mEc = code::suspend)
			//	mEc = {};

			assert(mAsyncOp.get() == nullptr || mAsyncOp->done());

			try
			{
				if (!mEc)
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

	using NativeProto = NativeProtoV<void>;

	template<typename P, typename... Args>
	ProtoV<typename P::return_type> makeProto(Args&&... args)
	{
		ProtoV<typename P::return_type> p;
		internal::InlinePoly<Resumable, internal::inlineSize>& b = p.mBase;

		b.emplace<P>(std::forward<Args>(args)...);
		return p;
	}


	namespace tests
	{
		void Native_StrSendRecv_Test();

		void Native_ZeroSendRecv_Test();
		void Native_ZeroSend_ErrorCode_Test();

		void Native_returnValue_Test();

		void Native_BadRecvSize_Test();
		void Native_BadRecvSize_ErrorCode_Test();


		void Native_nestedProtocol_Test();
		void Native_nestedProtocol_Throw_Test();
		void Native_nestedProtocol_ErrorCode_Test();

		void Native_asyncProtocol_Test();
		void Native_asyncProtocol_Throw_Test();
		void Native_endOfRound_Test();
		void Native_errorSocket_Test();

	}


}