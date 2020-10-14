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

		virtual error_code resume() = 0;
		virtual bool done() = 0;

		virtual void* getValue() { return nullptr; };
		virtual void setError(error_code& e, std::exception_ptr&& p) = 0;
		virtual std::exception_ptr getExpPtr() = 0;
		virtual error_code getErrorCode() = 0;

		Scheduler& getSched()
		{
			assert(mSched);
			return *mSched;
		};
	};



	template<typename T>
	struct Optional;

	template<>
	struct Optional<void>
	{};

	template<typename T>
	struct Optional : public std::optional<T>
	{};


	// TODO("merge this with ProtoPromise");
	template<typename T>
	class ProtoPromiseBase : public ProtoBase
	{
	public:
		using coro_handle = std::coroutine_handle<ProtoPromise<T>>;
		coro_handle mHandle;

		ProtoPromiseBase(coro_handle t)
			:mHandle(t)
		{}

		ProtoPromiseBase() = delete;
		ProtoPromiseBase(const ProtoPromiseBase&) = delete;
		ProtoPromiseBase(ProtoPromiseBase&& o)
			: ProtoBase(o.mSched),
			mHandle(o.mHandle)
		{
			o.mSched = nullptr;
			o.mHandle = nullptr;
		}

		~ProtoPromiseBase()
		{
			if (mHandle)
				mHandle.destroy();
		}


		error_code resume() override {

			assert(!mHandle.done());
			assert(mSched);
			auto& prom = mHandle.promise();

			prom.mSched = mSched;
			//assert(!prom.mEc || );
			if (prom.mEc == code::noMessageAvailable)
				prom.mEc = {};

			if(!prom.mEc)
				mHandle.resume();

			return prom.mEc;
		};

		Optional<T> mVal;
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
			return hasError() || mHandle.done();
		}


		void setError(error_code& ec, std::exception_ptr&& p) override {
			auto& prom = mHandle.promise();
			assert(!prom.mEc || prom.mEc == code::noMessageAvailable);
			prom.mEc = ec;
			prom.mExPtr = std::move(p);
		}
		std::exception_ptr getExpPtr() override {
			auto& prom = mHandle.promise();
			return prom.mExPtr;
		}
		error_code getErrorCode() override {
			auto& prom = mHandle.promise();
			return prom.mEc;
		}

		bool hasError()
		{
			return mHandle.promise().mEc &&
				mHandle.promise().mEc != code::noMessageAvailable;
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
			auto ec = mBase->resume();

			if constexpr (std::is_same_v<error_code, value_type>)
				mRes = ec;
			else
				mRes = Err(ec);

			if (ec == code::noMessageAvailable)
				return ec;
			return {};
		};

		void* getValue() override
		{
			if constexpr (!std::is_same_v<error_code, value_type>)
			{
				error_code ec = mRes.error();
				if (!ec)
				{
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

		void setError(error_code& ec, std::exception_ptr&& p) override {
			mRes = Err(std::move(ec));
			mExPtr = std::move(p);
		}
		std::exception_ptr getExpPtr() override {
			return mExPtr;
		}

		error_code getErrorCode() override {
			if constexpr (!std::is_same_v<error_code, value_type>)
			{
				if(mRes.hasError())
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
	class ProtoPromise
	{
	public:
		using coro_handle = std::coroutine_handle<ProtoPromise<T>>;
		std::exception_ptr mExPtr;
		error_code mEc;
		Scheduler* mSched = nullptr;


		ProtoPromise()
		{
			//std::cout << " ProtoPromise " << hexPtr(this) << std::endl;
		}


		~ProtoPromise()
		{
			//std::cout << " ~ProtoPromise " << hexPtr(this) << std::endl;
		}

		//ValueStorage<T> mRet;

		bool done()
		{
			return mEc || coro_handle::from_promise(*this).done();
		}

		Proto<T> get_return_object()
		{
			Proto<T> r;
			r.mBase.emplace<ProtoPromiseBase<T>>(coro_handle::from_promise(*this));
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

			prom.mSched->addProto(proto);

			auto ec = proto.resume();

			if (proto.done())
			{
				prom.mSched->removeProto(proto);

				if (ec)
					prom.mEc = ec;
			}
			else if(ec == code::noMessageAvailable)
				prom.mEc = ec;

			return !ec;

		}
		void await_suspend(coro_handle h)
		{
			///*mTask.mBase.get()->await_suspend(h)*/; 
		}

		T await_resume()
		{
			if constexpr (!std::is_same<void, T>::value)
			{
				auto& proto = *static_cast<ProtoPromiseBase<T>*>(mTask.mBase.get());
				assert(proto.done());
				auto ptr = (T*)proto.getValue();
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


			std::vector<ProtoBase*> mStack;

			error_code recv(Buffer& data) override;
			error_code send(Buffer& data) override;
			void addProto(ProtoBase& proto) override;
			void removeProto(ProtoBase& proto) override;

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
