#pragma once

#include <coroutine>
#include "Defines.h"
#include "Buffers.h"
#include "Scheduler.h"
#include <vector>
#include <list>
#include <optional>
#include <iostream>
#include "Result.h"

namespace coproto
{
	std::string hexPtr(void* p);

	template<typename T>
	class ProtoPromise;
	template<typename T>
	class Proto;
	template<typename T>
	class Async;
	template<typename T, typename U>
	class ProtoAwaiter;
	template<typename T, typename U>
	class AsyncAwaiter;

	struct EndOfRound
	{
	};


	class ProtoBase
	{
	public:
		ProtoBase() = default;
		ProtoBase(const ProtoBase&) = default;
		ProtoBase(ProtoBase&&) = default;

		ProtoBase(Scheduler* s)
			:mSched(s)
		{}

		Scheduler* mSched = nullptr;
		std::vector<ProtoBase*> mDownstream;
		std::vector<ProtoBase*> mUptream;

		void finalize(error_code ec, std::exception_ptr p)
		{
			assert(mUptream.size() == 0);
			if (ec) {
				for (auto d : mDownstream) {
					d->setError(ec, p);
				}
			}
			for (auto d : mDownstream) {
				auto iter = std::find(d->mUptream.begin(), d->mUptream.end(), this);
				assert(iter != d->mUptream.end());
				std::swap(*iter, d->mUptream.back());
				d->mUptream.pop_back();

				if (d->mUptream.size() == 0)
				{
					mSched->scheduleReady(*d);
				}
			}
		}

		virtual error_code resume() = 0;
		virtual bool done() = 0;

		virtual void* getValue() { return nullptr; };
		virtual void setError(error_code e, std::exception_ptr p) = 0;
		virtual std::exception_ptr getExpPtr() = 0;
		virtual error_code getErrorCode() = 0;
	};


	template<typename T>
	struct EndOfRoundAwaiter
	{
		using coro_handle = std::coroutine_handle<ProtoPromise<T>>;
		ProtoBase* mParent;
		EndOfRoundAwaiter(ProtoBase* parent)
			:mParent(parent)
		{}

		bool await_ready()
		{
			mParent->setError(code::suspend, nullptr);
			mParent->mSched->scheduleNext(*mParent);
			return false;
		}
		void await_suspend(coro_handle h) { }

		void await_resume()
		{
		}
	};


	namespace internal
	{
		template<typename T>
		struct ResultWrapperHelper;
		template<>
		struct ResultWrapperHelper<void>
		{
			using type = error_code;
		};
		template<typename T>
		struct ResultWrapperHelper
		{
			using type = Result<T, error_code>;
		};
	}

	template<typename T>
	class ResultWrapper : public ProtoBase
	{
	public:

		using value_type = typename internal::ResultWrapperHelper<T>::type;
		using type = Proto<value_type>;

		internal::Inline<ProtoBase> mBase;

		value_type mRes = Err(make_error_code(code::success));
		std::exception_ptr mExPtr = nullptr;

		ResultWrapper() = delete;
		ResultWrapper(const ResultWrapper&) = delete;

		ResultWrapper(internal::Inline<ProtoBase>&& o)
			: mBase(std::move(o))
		{}

		ResultWrapper(ResultWrapper&& o)
			: ProtoBase(o.mSched),
			mBase(std::move(o.mBase))
		{
			o.mSched = nullptr;
		}

		error_code resume() override {
			mBase->mSched = mSched;


			error_code ec;
			if (!done())
			{
				ec = mBase->resume();
				if (ec && ec != code::suspend)
					setError(ec, mBase->getExpPtr());
			}

			if (ec == code::suspend)
			{
				mUptream.push_back(mBase.get());
				mBase->mDownstream.push_back(this);
				return ec;
			}

			assert(done());
			finalize({}, nullptr);
			return {};
		};

		void* getValue() override
		{

			if constexpr (!std::is_same_v<error_code, value_type>) {
				if (!mRes.error()) {
					auto v = (T*)mBase.get()->getValue();
					assert(v);

					T& vv = *v;
					mRes = Ok(std::move(vv));
				}
			}

			return &mRes;
		}

		bool done() override {
			return mBase.get()->done();
		}

		void setError(error_code ec, std::exception_ptr p) override {
			assert(ec);
			mRes = Err(std::move(ec));
			mExPtr = std::move(p);
		}
		std::exception_ptr getExpPtr() override {
			return mExPtr;
		}

		error_code getErrorCode() override {
			if constexpr (!std::is_same_v<error_code, value_type>)
			{
				if (mRes.hasError())
					return mRes.error();
				return {};
			}
			else
			{
				return mRes;
			}
		}
	};




	template<typename T>
	class Async
	{
	public:

		// will return a T
		struct Controller : public ProtoBase
		{
			internal::Inline<ProtoBase> mBase;

			enum class State
			{
				Init,
				InProgress,
				Done
			};
			State mState = State::Init;
			error_code mEc;
			std::exception_ptr mExPtr;


			error_code resume() override
			{
				assert(mSched);
				mBase->mSched = mSched;

				if (mState == State::Init)
				{
					auto ec = mBase->resume();
					if (ec == code::suspend)
					{
						mState = State::InProgress;
						mUptream.push_back(mBase.get());
						mBase->mDownstream.push_back(this);
					}
					else
					{
						mState = State::Done;
						assert(mBase->done());

						if (ec)
							setError(ec, mBase->getExpPtr());

						//

					}
				}
				else if (mState == State::InProgress)
				{
					assert(mBase->done());
					mState = State::Done;
					finalize(mEc, mExPtr);
				}
				return {};
			}

			bool done() override {
				return mState == State::Done;
			};

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
			if (mBase && mBase->mState == Controller::State::InProgress)
			{
				assert(0 && "the caller must join the async operation before the Async is destroyed.");
				std::terminate();
			}
		}
	};

	// will return an Async<T>
	template<typename T>
	class AsyncWrapper : public ProtoBase
	{
	public:

		using Controller = typename Async<T>::Controller;

		internal::Inline<ProtoBase> mBase;
		Async<T> mRet;

		AsyncWrapper(internal::Inline<ProtoBase>&& o)
			: mBase(std::move(o))
		{}

		AsyncWrapper(AsyncWrapper&& o)
			: ProtoBase(o.mSched),
			mBase(std::move(o.mBase))
		{
			o.mSched = nullptr;
		}

		error_code resume() override
		{
			assert(!done());
			assert(mSched);
			mRet.mBase.reset(new Controller);
			auto ptr = (Controller*)mRet.mBase.get();
			ptr->mBase = std::move(mBase);
			ptr->mSched = mSched;
			return ptr->resume();			
		}

		bool done() override {
			return mRet.mBase.get() != nullptr;
		};

		void* getValue() override { return &mRet; };
		void setError(error_code e, std::exception_ptr p)override {
			assert(0);
			//mEc = e;
			//mExPtr = std::move(p);
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
	class ProtoPromise : public ProtoBase, public ReturnStorage<T>
	{
	public:
		using coro_handle = std::coroutine_handle<ProtoPromise<T>>;
		std::exception_ptr mExPtr;
		error_code mEc;

		ProtoPromise() { }
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
		AsyncAwaiter<U, T> await_transform(Async<U>&& p);
		template<typename U>
		AsyncAwaiter<U, T> await_transform(Async<U>& p);
		EndOfRoundAwaiter<T> await_transform(EndOfRound& p);
		EndOfRoundAwaiter<T> await_transform(EndOfRound&& p);

		error_code resume() override {


			if (!done())
			{
				mEc = {};
				getHandle().resume();
			}

			if (done())
				finalize(mEc, mExPtr);

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



	template<typename T = void>
	class Proto
	{
	public:
		using promise_type = ProtoPromise<T>;
		using coro_handle = std::coroutine_handle<promise_type>;
		internal::Inline<ProtoBase> mBase;

		using value_type = T;

		typename ResultWrapper<T>::type wrap()
		{
			typename ResultWrapper<T>::type r;
			r.mBase.emplace<ResultWrapper<T>>(std::move(mBase));
			return r;
		}

		Proto<Async<T>> async()
		{
			Proto<Async<T>> r;
			r.mBase.setOwned(new AsyncWrapper<T>(std::move(mBase)));
			return r;
		}
	};


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
			proto.mSched = prom.mSched;
			auto ec = proto.resume();

			if (ec == code::suspend)
			{
				prom.mUptream.push_back(&proto);
				proto.mDownstream.push_back(&prom);
				prom.mEc = ec;
			}
			else if (ec)
			{
				prom.mEc = ec;
			}
			return !ec;
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



	template<typename T, typename U>
	class AsyncAwaiter
	{
	public:
		Async<T> mTask;
		using coro_handle = std::coroutine_handle<ProtoPromise<U>>;
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
			proto.mSched = prom.mSched;


			if (proto.done())
			{
				return true;
			}
			else
			{
				prom.mUptream.push_back(&proto);
				proto.mDownstream.push_back(&prom);
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




	class LocalScheduler
	{
	public:

		struct Sched : public Scheduler
		{
			u64 mIdx;
			LocalScheduler* mSched;


			std::list<ProtoBase*> mReady, mNext;

			error_code recv(Buffer& data) override;
			error_code send(Buffer& data) override;
			void scheduleNext(ProtoBase& proto) override;
			void scheduleReady(ProtoBase& proto) override;

			void runOne();
			bool done();
		};

		std::array<std::list<std::vector<u8>>, 2> mBuffs;
		std::array<Sched, 2> mScheds;

		template<typename T>
		error_code execute(Proto<T>& p0, Proto<T>& p1)
		{
			bool v = false;
			mScheds[0].mIdx = 0;
			mScheds[0].mSched = this;
			mScheds[1].mIdx = 1;
			mScheds[1].mSched = this;

			mScheds[0].scheduleReady(*p0.mBase.get());
			mScheds[1].scheduleReady(*p1.mBase.get());

			while (
				mScheds[0].done() == false ||
				mScheds[1].done() == false)
			{

				if (mScheds[0].done() == false)
				{
					mScheds[0].runOne();
					if (v)
						std::cout << "p0 end of round " << std::endl;

					if (mScheds[0].done())
					{
						auto e0 = p0.mBase->getErrorCode();
						if (e0)
							return e0;
					}
				}

				if (mScheds[1].done() == false)
				{
					mScheds[1].runOne();

					if (v)
						std::cout << "p1 end of round " << std::endl;

					if (mScheds[1].done())
					{
						auto e1 = p1.mBase->getErrorCode();
						if (e1)
							return e1;
					}
				}
			}

			return {};
		}
	};


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
	}

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
	inline AsyncAwaiter<U, T> ProtoPromise<T>::await_transform(Async<U>&& p)
	{
		return AsyncAwaiter<U, T>(
			coro_handle::from_promise(*this),
			std::move(p));
	}
	template<typename T>
	template<typename U>
	inline AsyncAwaiter<U, T> ProtoPromise<T>::await_transform(Async<U>& p)
	{
		return AsyncAwaiter<U, T>(
			coro_handle::from_promise(*this),
			std::move(p));
	}



	template<typename T>
	EndOfRoundAwaiter<T> ProtoPromise<T>::await_transform(EndOfRound& p)
	{
		return EndOfRoundAwaiter<T>(this);
	}
	template<typename T>
	EndOfRoundAwaiter<T> ProtoPromise<T>::await_transform(EndOfRound&& p)
	{
		return EndOfRoundAwaiter<T>(this);
	}

}
