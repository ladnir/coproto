#pragma once
#include <coroutine>
#include "coproto/Resumable.h"
#include "coproto/Scheduler.h"
#include "coproto/error_code.h"
#include "coproto/InlinePoly.h"
#include <cassert>

namespace coproto
{


	template<typename T>
	class Async
	{
	public:

		// will return a T
		struct Controller : public Resumable
		{
			internal::InlinePoly<Resumable, internal::inlineSize> mBase;

			enum class Status
			{
				Init,
				InProgress,
				Done
			};
			Status mStatus = Status::Init;
			error_code mEc;
			std::exception_ptr mExPtr;


			error_code resume_(Scheduler& sched) override
			{
				if (mStatus == Status::Init)
				{
					mBase->mSlotIdx = sched.mNextSlot++;


					sched.scheduleReady(*mBase.get());
					//auto ec = sched.resume();
					//if (ec == code::suspend)
					//{
						mStatus = Status::InProgress;
						sched.addDep(*this, *mBase.get());
					//}
					//else
					//{
					//	mStatus = Status::Done;
					//	assert(mBase->done());

					//	if (ec)
					//		setError(ec, mBase->getExpPtr());

					//	//

					//}
				}
				else if (mStatus == Status::InProgress)
				{
					assert(mBase->done());
					mStatus = Status::Done;
					sched.fulfillDep(*this, mEc, mExPtr);

				}
				return {};
			}

			bool done() override {
				return mStatus == Status::Done;
			};


#ifdef COPROTO_LOGGING
			std::string getName() override
			{
				return mBase->getName() + "_join";
			}
#endif

			void* getValue() override { return mBase->getValue(); };
			void setError(error_code e, std::exception_ptr p)override {
				mEc = e;
				mExPtr = std::move(p);
			}
			std::exception_ptr getExpPtr() override {
				return mExPtr;
			}
			error_code getErrorCode() override {
				return mEc;
			}
		};

		std::unique_ptr<Controller> mBase;


		Async() = default;
		Async(const Async<T>&) = delete;
		Async(Async<T>&&) = default;


		Async<T>& operator=(Async<T>&&) = default;

		~Async()
		{
			if (mBase && mBase->mStatus == Controller::Status::InProgress)
			{
				assert(0 && "the caller must join the async operation before the Async is destroyed.");
				std::terminate();
			}
		}
	};

	namespace internal
	{


		// will return an Async<T>
		template<typename T>
		class AsyncPromise : public Resumable
		{
		public:

			using Controller = typename Async<T>::Controller;

			internal::InlinePoly<Resumable, internal::inlineSize> mBase;
			Async<T> mRet;

			enum class Status
			{
				Init,
				Done
			};
			Status mStatus = Status::Init;

			AsyncPromise(internal::InlinePoly<Resumable, internal::inlineSize>&& o)
				: mBase(std::move(o))
			{
#ifdef COPROTO_LOGGING
				mName = mBase->getName() + "_async";
#endif
			}

			AsyncPromise(AsyncPromise&& o)
				: mBase(std::move(o.mBase))
#ifdef COPROTO_LOGGING
				, mName(std::move(o.mName))
#endif
			{
			}
			~AsyncPromise()
			{
				//std::cout << "~AsyncWrapper " << hexPtr(this) << std::endl;
			}

			error_code resume_(Scheduler& sched) override
			{
				assert(mStatus == Status::Init);
				mStatus = Status::Done;

#ifdef COPROTO_LOGGING
				sched.logEdge(*this, *mBase.get(), true);
#endif
				mRet.mBase = std::make_unique<Controller>();
				auto ptr = (Controller*)mRet.mBase.get();
				ptr->mBase = std::move(mBase);


				sched.resume(ptr);

				sched.fulfillDep(*this, {}, nullptr);
				//ptr->resume(sched);

				return {};
			}

			bool done() override {
				return mStatus == Status::Done;
			};

			void* getValue() override { 
				return &mRet; 
			};
			void setError(error_code e, std::exception_ptr p)override {
				assert(0);
			}

			std::exception_ptr getExpPtr() override {
				assert(0); //return mExPtr;
				std::terminate();
			}
			error_code getErrorCode() override {
				assert(0);// return mEc;
				std::terminate();
			};
		};



		template<typename T>
		class AsyncAwaiter
		{
		public:
			Async<T> mTask;
			using coro_handle = std::coroutine_handle<ProtoPromise<void>>;
			coro_handle mHandle;

			AsyncAwaiter(coro_handle handle, Async<T>&& t)
				: mTask(std::move(t))
				, mHandle(handle)
			{
			}

			bool await_ready()
			{
				auto& prom = mHandle.promise();
				auto& proto = *mTask.mBase.get();


				if (proto.done())
				{
#ifdef COPROTO_LOGGING
					prom.mSched->logEdge(prom, proto);
					prom.mSched->logEdge(proto, prom);
#endif
					return true;
				}
				else
				{
					prom.mSched->addDep(prom, proto);
					return false;
				}

			}
			void await_suspend(coro_handle h) { }

			T await_resume()
			{
				if constexpr (!std::is_same<void, T>::value)
				{
					auto ptr = (T*)mTask.mBase->getValue();
					assert(ptr);
					return std::move(*ptr);
				}
			}
		};

	}
}