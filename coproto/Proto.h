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


			error_code evaluate(Socket& sock)
			{
				if (mSched == nullptr)
				{
					mSched.reset(new Scheduler);
					mSched->scheduleReady(*get());
					get()->mSlotIdx = 0;
				}

				mSched->mSock = &sock;
				mSched->run();

				if (get()->done())
					return get()->getErrorCode();
				else
					return code::suspend;
			}

			void evaluate(AsyncSocket& sock, std::function<void(error_code)>&& cont, Executor& ex)
			{
				assert(mSched == nullptr);
				mSched.reset(new Scheduler);
				mSched->scheduleReady(*get());
				get()->mSlotIdx = 0;

				mSched->mASock = &sock;
				mSched->mExecutor = &ex;
				mSched->mCont = std::move(cont);

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

		using value_type = T;

		typename internal::ResultPromise<T>::type wrap()
		{
			typename internal::ResultPromise<T>::type r;
			r.mBase.emplace<internal::ResultPromise<T>>(std::move(mBase));
			return r;
		}

		ProtoV<Async<T>> async()
		{
			ProtoV<Async<T>> r;
			r.mBase.setOwned(new internal::AsyncPromise<T>(std::move(mBase)));
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
			return mBase.evaluate(sock);
		}

		void evaluate(AsyncSocket& sock, std::function<void(error_code)>&& cont, Executor& ex)
		{
			mBase.evaluate(sock, std::move(cont), ex);

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
			int& value() { int i; return i; }

			void return_void()
			{}
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

			T& value()
			{
				return mVal.value();
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
			std::string mLabel;

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
				mLabel = (name.mName);
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

			void* getValue()
			{
				if constexpr (std::is_void_v<T>)
					return nullptr;
				else
				{
					return &ReturnStorage<T>::value();
				}
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
		void strSendRecvTest();
		void resultSendRecvTest();
		void returnValueTest();
		void typedRecvTest();

		void zeroSendRecvTest();
		void badRecvSizeTest();

		void zeroSendErrorCodeTest();
		void badRecvSizeErrorCodeTest();

		void throwsTest();

		void nestedSendRecvTest();
		void nestedProtocolThrowTest();
		void nestedProtocolErrorCodeTest();
		void asyncProtocolTest();
		void asyncThrowProtocolTest();

		void endOfRoundTest();
		
		void errorSocketTest();

	}

}
