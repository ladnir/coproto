#pragma once

#include <coroutine>
#include "Defines.h"
#include "Buffers.h"
#include "Scheduler.h"
#include <vector>
#include <list>
#include <optional>
#include <iostream>

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

		//using void_handle = std::coroutine_handle<void>;

		virtual error_code resume() = 0;
		virtual bool done() = 0;

		virtual void* getValue() { return nullptr; };
		//virtual void await_suspend(void_handle h) = 0;
		//virtual T await_resume() = 0;

		//void setSched(Scheduler& s) { 
		//	mSched = &s; 
		//	s.addProto(*this);
		//};

		Scheduler& getSched()
		{
			assert(mSched);
			return *mSched;
		};

	};


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
		ProtoPromiseBase(ProtoPromiseBase&&o)
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
			
			assert(!done());
			assert(mSched);
			auto& prom = mHandle.promise();

			prom.mSched = mSched;
			assert(!prom.mEc || prom.mEc == code::noMessageAvailable);
			prom.mEc = {};
			mHandle.resume();
			return mHandle.promise().mEc;

		};

		bool done() override {
			return hasError() || mHandle.done();
		}

		bool hasError()
		{
			return mHandle.promise().mEc &&
				mHandle.promise().mEc != code::noMessageAvailable;
		}
	};

	template<typename T>
	struct ValueStorage;

	template<>
	struct ValueStorage<void>
	{
		void value() {}
	};

	template<typename T> 
	struct ValueStorage
	{
		std::optional<T> mVal = std::nullopt;;
		T value()
		{
			assert(mVal.has_value());
			return std::move(mVal.value());
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

		ValueStorage<T> mRet;

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

	template<typename T = void>
	class Proto
	{
	public:
		using promise_type = ProtoPromise<T>;
		using coro_handle = std::coroutine_handle<promise_type>;
		internal::Inline<ProtoBase> mBase;

		using value_type = T;
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

			prom.mEc = proto.resume();

			if(!proto.done())
				prom.mSched->addProto(proto);

			return !prom.mEc;

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
