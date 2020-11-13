#pragma once

#include <coroutine>
#include <vector>
#include <list>
#include <sstream>
#include <iostream>
#include <optional>

#include "coproto/Defines.h"
#include "coproto/Scheduler.h"
#include "coproto/Result.h"
#include "coproto/InlineVector.h"
#include "coproto/InlinePoly.h"
#include "coproto/Async.h"
#include "coproto/EndOfRound.h"
#include "coproto/Name.h"
#include "coproto/Resumable.h"
//#include "coproto/"
#include <cassert>


namespace coproto
{
	std::string hexPtr(void* p);

	namespace internal
	{
		struct ProtoImpl : public InlinePoly<Resumable, inlineSize>
		{
			
			std::unique_ptr<Scheduler> mSched;


			error_code evaluate(Socket& sock, bool print)
			{
				if (mSched == nullptr)
				{
					mSched = std::make_unique<Scheduler>();
					mSched->scheduleReady(*get());
					get()->mSlotIdx = 0;
				}

				mSched->mSock = &sock;
				mSched->mPrint = print;

				mSched->run();

				if (get()->done())
					return get()->getErrorCode();
				else
					return code::suspend;
			}

			void evaluate(AsyncSocket& sock, std::function<void(error_code)>&& cont, Executor& ex, bool print)
			{
				assert(mSched == nullptr);
				mSched = std::make_unique<Scheduler>();
				mSched->scheduleReady(*get());
				get()->mSlotIdx = 0;

				mSched->mASock = &sock;
				mSched->mExecutor = &ex;
				mSched->mCont = std::move(cont);
				mSched->mPrint = print;

				mSched->dispatch([this]() {
					mSched->run();
					});

			}
		};
	}

	template<typename T = void>
	class ProtoV
	{
	public:
		using promise_type = internal::ProtoPromise<T>;
		using coro_handle = std::coroutine_handle<promise_type>;

		internal::ProtoImpl mBase;


		ProtoV() = default;
		ProtoV(const ProtoV&) = delete;
		ProtoV(ProtoV&&) = default;

		using return_type = T;

		typename internal::ResultPromise<T>::type wrap()
		{
			typename internal::ResultPromise<T>::type r;
			r.mBase.emplace<internal::ResultPromise<T>>(std::move(mBase));
			return r;
		}

		ProtoV<Async<T>> async()
		{
			ProtoV<Async<T>> r;
			auto ptr = new internal::AsyncPromise<T>(std::move(mBase));
			COPROTO_REG_NEW(ptr, "async");
			r.mBase.setOwned(ptr);
			//++gNewDel;
			//std::cout << "new " << hexPtr(r.mBase.get()) << std::endl;
			return r;
		}

		void setName(std::string name)
		{
#ifdef COPROTO_LOGGING
			mBase->setName(name);
#endif
		}

		error_code evaluate(Socket& sock)
		{
			return mBase.evaluate(sock, false);
		}

		void evaluate(AsyncSocket& sock, std::function<void(error_code)>&& cont, Executor& ex)
		{
			mBase.evaluate(sock, std::move(cont), ex, false);

		}

		operator internal::ProtoImpl& ()
		{
			return mBase;
		}
	};
	using Proto = ProtoV<void>;


	namespace internal
	{

		template<typename T, typename U>
		class ProtoAwaiter
		{
		public:
			ProtoV<T> mTask;
			using coro_handle = std::coroutine_handle<ProtoPromise<U>>;
			coro_handle mHandle;

			ProtoAwaiter(coro_handle handle, ProtoV<T>&& t)
				: mTask(std::move(t))
				, mHandle(handle)
			{
			}

			bool await_ready()
			{
				auto& prom = mHandle.promise();
				auto& proto = *mTask.mBase.get();

	#ifdef COPROTO_LOGGING
				prom.mSched->logEdge(prom, proto);
	#endif
				prom.mSched->addDep(prom, proto);


				prom.mEc = prom.mSched->resume(&proto);
				//= prom.mSched->startSubproto(prom, proto);

				//prom.mSched->addDep(prom, proto);
				//prom.mEc = prom.mSched->resume(&proto);

				return !prom.mEc;
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

		template<typename T>
		struct ReturnStorage;

		template<>
		struct ReturnStorage<void>
		{
			void return_void()
			{}


			void* getValue()
			{
				return nullptr;
			}
		};

		template<typename T>
		struct ReturnStorage
		{
			static_assert(std::is_void_v<T> == false, "");
			std::optional<T> mVal;

			void return_value(T&& t)
			{
				mVal = std::forward<T>(t);
			}

			void return_value(const T& t)
			{
				mVal = t;
			}

			void* getValue()
			{
				return &mVal.value();
			}


		};

		template<typename T>
		class ProtoPromise : public Resumable, public ReturnStorage<T>
		{
		public:
			using coro_handle = std::coroutine_handle<ProtoPromise<T>>;
			Scheduler* mSched = nullptr;
			std::exception_ptr mExPtr;
			error_code mEc;
			u64 mResumeIdx = 0;
			u64 mProtoIdx = 0;
#ifdef COPROTO_LOGGING
			std::string mLabel;
#endif


			ProtoPromise() {
				mProtoIdx = gProtoIdx++;
#ifdef COPROTO_LOGGING
				setName("Proto_" + std::to_string(mProtoIdx));
#endif
			}
			~ProtoPromise() { }

			coro_handle getHandle()
			{
				return coro_handle::from_promise(*this);
			}

			ProtoV<T> get_return_object()
			{
				ProtoV<T> r;
				r.mBase.setBorrowed(this);
				return r;
			}
			std::suspend_always initial_suspend() { return {}; }
			std::suspend_always final_suspend() noexcept { return {}; }

			void unhandled_exception() {
				mExPtr = std::current_exception();
				mEc = code::uncaughtException;
			}

			template<typename U>
			ProtoAwaiter<U, T> await_transform(ProtoV<U>&& p);
			template<typename U>
			AsyncAwaiter<U> await_transform(Async<U>&& p);
			template<typename U>
			AsyncAwaiter<U> await_transform(Async<U>& p);
			EndOfRoundAwaiter await_transform(const EndOfRound& p);

			std::suspend_never await_transform(const Name& name)
			{
#ifdef COPROTO_LOGGING
				mLabel = (name.mName);
#endif
				return std::suspend_never{};
			}

			error_code resume_(Scheduler& sched) override {

				mSched = &sched;

				if (!done())
				{
					mEc = {};
					auto handle = getHandle();
					handle.resume();
				}

				if (done())
				{
					mSched->fulfillDep(*this, mEc, mExPtr);
#ifdef COPROTO_LOGGING
					mSched->logProto(mName, mProtoIdx, mLabel, mResumeIdx);
#endif
				}
				else
					++mResumeIdx;
				return mEc;
			};

			void* getValue() override
			{
				return ReturnStorage<T>::getValue();
			}

			bool done() override {

				return hasError() || getHandle().done();
			}
#ifdef COPROTO_LOGGING
			std::string getName() override
			{
				return mName + "_" + std::to_string(mResumeIdx);
			}
#endif
			void setError(error_code ec, std::exception_ptr p) override {
				assert(!hasError());
				mEc = ec;
				mExPtr = std::move(p);
			}
			std::exception_ptr getExpPtr() override {
				return mExPtr;
			}
			error_code getErrorCode() override {
				return mEc;
			}

			bool hasError()
			{
				return mEc &&
					mEc != code::suspend;
			}
		};



		template<typename T>
		template<typename U>
		inline ProtoAwaiter<U, T> ProtoPromise<T>::await_transform(ProtoV<U>&& p)
		{
			return ProtoAwaiter<U, T>(
				coro_handle::from_promise(*this),
				std::move(p));
		}

		template<typename T>
		template<typename U>
		inline AsyncAwaiter<U> ProtoPromise<T>::await_transform(Async<U>&& p)
		{
			return AsyncAwaiter<U>(
				coro_handle::from_promise(*this),
				std::move(p));
		}
		template<typename T>
		template<typename U>
		inline AsyncAwaiter<U> ProtoPromise<T>::await_transform(Async<U>& p)
		{
			return AsyncAwaiter<U>(
				coro_handle::from_promise(*this),
				std::move(p));
		}



		template<typename T>
		EndOfRoundAwaiter ProtoPromise<T>::await_transform(const EndOfRound& p)
		{
				
			if (mSched->mReady.size())
			{
				mSched->mReady.push_back(this);
				mEc = code::suspend;
				return false;
			}
			return true;
		}


	}


	namespace tests
	{
		void coawait_strSendRecv_Test();
		void coawait_resultSendRecv_Test();
		void coawait_returnValue_Test();
		void coawait_typedRecv_Test();
		
		void coawait_zeroSendRecv_Test();
		void coawait_zeroSendRecv_ErrorCode_Test();

		void coawait_badRecvSize_Test();
		void coawait_badRecvSize_ErrorCode_Test();

		void coawait_throws_Test();
		
		void coawait_nestedProtocol_Test();
		void coawait_nestedProtocol_Throw_Test();
		void coawait_nestedProtocol_ErrorCode_Test();
		
		void coawait_asyncProtocol_Test();
		void coawait_asyncProtocol_Throw_Test();
		
		void coawait_endOfRound_Test();
		void coawait_errorSocket_Test();

	}

}
