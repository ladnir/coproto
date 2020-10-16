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

	template<typename T, typename U>
	class ProtoAwaiter;


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
			if (ec)
			{
				for (auto d : mDownstream)
				{
					d->setError(ec, p);
				}
			}

			for (auto d : mDownstream)
			{
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

		//Scheduler& getSched()
		//{
		//	assert(mSched);
		//	return *mSched;
		//};
	};



	template<typename T>
	struct Optional;

	template<>
	struct Optional<void>
	{};

	template<typename T>
	struct Optional : public std::optional<T>
	{};


	//// TODO("merge this with ProtoPromise");
	//template<typename T>
	//class ProtoPromiseBase : public ProtoBase
	//{
	//public:
	//	using coro_handle = std::coroutine_handle<ProtoPromise<T>>;
	//	coro_handle mHandle;

	//	ProtoPromiseBase(coro_handle t)
	//		:mHandle(t)
	//	{}

	//	ProtoPromiseBase() = delete;
	//	ProtoPromiseBase(const ProtoPromiseBase&) = delete;
	//	ProtoPromiseBase(ProtoPromiseBase&& o)
	//		: ProtoBase(o.mSched),
	//		mHandle(o.mHandle)
	//	{
	//		o.mSched = nullptr;
	//		o.mHandle = nullptr;
	//	}

	//	~ProtoPromiseBase()
	//	{
	//		if (mHandle)
	//			mHandle.destroy();
	//	}


	//	error_code resume() override {

	//		assert(!mHandle.done());
	//		assert(mSched);
	//		auto& prom = mHandle.promise();

	//		prom.mSched = mSched;

	//		if (prom.mEc == code::suspend)
	//			prom.mEc = {};

	//		if(!prom.mEc)
	//			mHandle.resume();

	//		if (done())
	//			finalize();

	//		return prom.mEc;
	//	};

	//	Optional<T> mVal;
	//	void* getValue()
	//	{
	//		if constexpr (std::is_void_v<T>)
	//			return nullptr;
	//		else
	//		{
	//			return &mVal.value();
	//		}
	//	}

	//	bool done() override {
	//		return hasError() || mHandle.done();
	//	}


	//	void setError(error_code& ec, std::exception_ptr&& p) override {
	//		auto& prom = mHandle.promise();
	//		assert(!prom.mEc || prom.mEc == code::suspend);
	//		prom.mEc = ec;
	//		prom.mExPtr = std::move(p);
	//	}
	//	std::exception_ptr getExpPtr() override {
	//		auto& prom = mHandle.promise();
	//		return prom.mExPtr;
	//	}
	//	error_code getErrorCode() override {
	//		auto& prom = mHandle.promise();
	//		return prom.mEc;
	//	}

	//	bool hasError()
	//	{
	//		return mHandle.promise().mEc &&
	//			mHandle.promise().mEc != code::suspend;
	//	}
	//};


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
	class ProtoPromise : public ProtoBase
	{
	public:
		using coro_handle = std::coroutine_handle<ProtoPromise<T>>;
		std::exception_ptr mExPtr;
		error_code mEc;
		Optional<T> mVal;

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
		void return_void() {}
		void unhandled_exception() {
			mExPtr = std::current_exception();
			mEc = code::uncaughtException;
		}

		template<typename U>
		ProtoAwaiter<U, T> await_transform(Proto<U>&& p);












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
				return &mVal.value();
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


	//template<typename T>
	//struct WrapResult;
	//template<>
	//struct WrapResult<void>
	//{
	//	using type = Proto<error_code>;
	//};


	//template<typename T>
	//struct WrapResult
	//{
	//	using type = Proto<Result<T, error_code>>;
	//};

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

			//prom.mSched->addProto(proto);

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

			//if (proto.done())
			//{
			//	if (ec)
			//		prom.mEc = ec;

			//	return !ec;
			//}
			//else if (ec == code::suspend)
			//{
			//	//prom.mSched->scheduleNext(proto);

			//	prom.mEc = code::suspend;
			//}
		}
		void await_suspend(coro_handle h)
		{
			///*mTask.mBase.get()->await_suspend(h)*/; 
		}

		T await_resume()
		{
			if constexpr (!std::is_same<void, T>::value)
			{
				//auto& proto = *static_cast<ProtoPromise<T>*>(mTask.mBase.get());
				//return std::move(proto.mVal.value());
				//assert(proto.done());
				auto ptr = (T*)mTask.mBase->getValue();
				assert(ptr);
				//auto& prom = proto.mHandle.promise();
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
			void scheduleReady(ProtoBase& proto);
			//void removeProto(ProtoBase& proto) override;

			//void addProto(ProtoAwaiter<void>& awaiter) override;


			void runOne();

			bool done();
		};

		std::array<std::list<std::vector<u8>>, 2> mBuffs;
		std::array<Sched, 2> mScheds;

		error_code execute(Proto<void>& p0, Proto<void>& p1);
	};


	namespace tests
	{
		void strSendRecvTest();
		void resultSendRecvTest();
		void typedRecvTest();

		void zeroSendRecvTest();
		void badRecvSizeTest();

		void zeroSendErrorCodeTest();
		void badRecvSizeErrorCodeTest();

		void throwsTest();

		void nestedSendRecvTest();
		void nestedProtocolThrowTest();
		void nestedProtocolErrorCodeTest();
	}

	template<typename T>
	template<typename U>
	inline ProtoAwaiter<U, T> ProtoPromise<T>::await_transform(Proto<U>&& p)
	{
		return ProtoAwaiter<U, T>(
			coro_handle::from_promise(*this),
			std::move(p));
	}

}
