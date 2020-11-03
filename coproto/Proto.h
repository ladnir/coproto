#pragma once

#include <coroutine>
#include <vector>
#include <list>
#include <sstream>
#include <iostream>
#include <optional>

#include "Defines.h"
#include "Scheduler.h"
#include "Result.h"
#include "InlineVector.h"
#include "InlinePoly.h"
#include "Async.h"
#include "EndOfRound.h"
#include "Name.h"
#include "Resumable.h"

#include <cassert>


namespace coproto
{
	std::string hexPtr(void* p);


	template<typename T = void>
	class Proto
	{
	public:
		using promise_type = internal::ProtoPromise<T>;
		using coro_handle = std::coroutine_handle<promise_type>;
		internal::InlinePoly<Resumable, internal::inlineSize> mBase;

		using value_type = T;

		typename internal::ResultPromise<T>::type wrap()
		{
			typename internal::ResultPromise<T>::type r;
			r.mBase.emplace<internal::ResultPromise<T>>(std::move(mBase));
			return r;
		}

		Proto<Async<T>> async()
		{
			Proto<Async<T>> r;
			r.mBase.setOwned(new internal::AsyncPromise<T>(std::move(mBase)));
			return r;
		}

		void setName(std::string name)
		{
#ifdef COPROTO_LOGGING
			mBase->setName(name);
#endif
		}


		operator Resumable& ()
		{
			assert(mBase.get());
			return *mBase.get();
		}
	};


	namespace internal
	{

		template<typename T, typename U>
		class ProtoAwaiter
		{
		public:
			Proto<T> mTask;
			using coro_handle = std::coroutine_handle<ProtoPromise<U>>;
			coro_handle mHandle;

			ProtoAwaiter(coro_handle handle, Proto<T>&& t)
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

			Proto<T> get_return_object()
			{
				Proto<T> r;
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
			ProtoAwaiter<U, T> await_transform(Proto<U>&& p);
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
		inline ProtoAwaiter<U, T> ProtoPromise<T>::await_transform(Proto<U>&& p)
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
	}

}
