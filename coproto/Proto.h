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
#include <sstream>
#include "InlineVector.h"
//#define USE_INLINE

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
	const int inlineSize = 100;

	class Resumable
	{
	public:
		Resumable() = default;
		Resumable(const Resumable&) = default;
		Resumable(Resumable&&) = default;

		internal::InlineVector<Resumable*, 4> mUpstream, mDwstream;

		u32 mSlotIdx = ~0;
		u32& getSlot()
		{
			return mSlotIdx;
		}

#ifdef COPROTO_LOGGING
		std::string mName;
		void setName(std::string name)
		{
			mName = name;
		}

		virtual std::string getName()
		{
			if (mName.size() == 0)
			{
				mName = "unknown_" + hexPtr(this);
			}
			return mName;
		}
#endif

		virtual ~Resumable() {}
		virtual error_code resume_(Scheduler& sched) = 0;
		virtual bool done() = 0;
		virtual void* getValue() { return nullptr; };
		virtual void setError(error_code e, std::exception_ptr p) = 0;
		virtual std::exception_ptr getExpPtr() = 0;
		virtual error_code getErrorCode() = 0;
	};


	class IoProto : public Resumable, public BufferInterface
	{};

	template<typename T>
	struct EndOfRoundAwaiter
	{
		using coro_handle = std::coroutine_handle<ProtoPromise<T>>;
		ProtoPromise<T>* mParent;
		EndOfRoundAwaiter(ProtoPromise<T>* parent)
			:mParent(parent)
		{}

		bool await_ready()
		{

			//mParent->mSched->setEndOfRound();
			//return true;
			if (mParent->mSched->mReady.size())
			{
				mParent->mSched->mReady.push_back(mParent);
				return false;
			}
			return true;
			//mParent->setError(code::suspend, nullptr);
			//mParent->mSched->scheduleNext(*mParent);
			//return false;
		}
		void await_suspend(coro_handle h) { }

		void await_resume()
		{
		}
	};

	struct Name
	{
		Name(std::string n)
			:mName(n)
		{}

		std::string mName;
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
	class ResultWrapper : public Resumable
	{
	public:

		using value_type = typename internal::ResultWrapperHelper<T>::type;
		using type = Proto<value_type>;

		internal::InlinePoly<Resumable, inlineSize> mBase;

		enum class Status
		{
			Init,
			InProgress,
			Done
		};
		Status mStatus = Status::Init;

		value_type mRes = Err(make_error_code(code::success));
		std::exception_ptr mExPtr = nullptr;

		ResultWrapper() = delete;
		ResultWrapper(const ResultWrapper&) = delete;

		ResultWrapper(internal::InlinePoly<Resumable, inlineSize>&& o)
			: mBase(std::move(o))
		{}

		ResultWrapper(ResultWrapper&& o)
			: mBase(std::move(o.mBase))
		{
		}

		error_code resume_(Scheduler& sched) override {

			error_code ec;
			if (mStatus == Status::Init)
			{
				mStatus = Status::InProgress;
				assert(mBase->done() == false);

				sched.addDep(*this, *mBase.get());

				ec = sched.resume(mBase.get());
				if (ec == code::suspend)
					return ec;
			}

			if (mStatus == Status::InProgress)
			{
				mStatus = Status::Done;
				assert(mBase->done());
				sched.fulfillDep(*this, {}, nullptr);
			}
			else
				assert(0 && COPROTO_LOCATION);

			return {};
		};

#ifdef COPROTO_LOGGING
		std::string getName() override
		{
			return mBase->getName() + "_wrap";
		}
#endif

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
			return mStatus == Status::Done;
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
		struct Controller : public Resumable
		{
			internal::InlinePoly<Resumable, inlineSize> mBase;

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

					auto ec = sched.resume(mBase.get());
					if (ec == code::suspend)
					{
						mStatus = Status::InProgress;
						sched.addDep(*this, *mBase.get());
					}
					else
					{
						mStatus = Status::Done;
						assert(mBase->done());

						if (ec)
							setError(ec, mBase->getExpPtr());

						//

					}
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

	// will return an Async<T>
	template<typename T>
	class AsyncWrapper : public Resumable
	{
	public:

		using Controller = typename Async<T>::Controller;

		internal::InlinePoly<Resumable, inlineSize> mBase;
		Async<T> mRet;

		enum class Status
		{
			Init,
			Done
		};
		Status mStatus = Status::Init;

		AsyncWrapper(internal::InlinePoly<Resumable, inlineSize>&& o)
			: mBase(std::move(o))
		{
#ifdef COPROTO_LOGGING
			mName = mBase->getName() + "_async";
#endif
		}

		AsyncWrapper(AsyncWrapper&& o)
			: mBase(std::move(o.mBase))
#ifdef COPROTO_LOGGING
			, mName(std::move(o.mName))
#endif
		{
		}
		~AsyncWrapper()
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
			mRet.mBase.reset(new Controller);
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

		void* getValue() override { return &mRet; };
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
		AsyncAwaiter<U, T> await_transform(Async<U>&& p);
		template<typename U>
		AsyncAwaiter<U, T> await_transform(Async<U>& p);
		EndOfRoundAwaiter<T> await_transform(EndOfRound& p);
		EndOfRoundAwaiter<T> await_transform(EndOfRound&& p);

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



	template<typename T = void>
	class Proto
	{
	public:
		using promise_type = ProtoPromise<T>;
		using coro_handle = std::coroutine_handle<promise_type>;
		internal::InlinePoly<Resumable, inlineSize> mBase;

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

		void setName(std::string name)
		{
#ifdef COPROTO_LOGGING
			mBase->setName(name);
#endif
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




	class LocalScheduler
	{
	public:

		struct Sock : public Socket
		{
			//u64 mIdx;
			//LocalScheduler* mSched;

			std::list<std::vector<u8>> mInbound, mOutbound;

			error_code recv(span<u8> data) override;
			error_code send(span<u8> data) override;
		};

		std::array<Sock, 2> mSocks;
		std::array<Scheduler, 2> mScheds;

		void sendMsgs(u64 sender)
		{
			assert(mSocks[sender ^ 1].mInbound.size() == 0);
			mSocks[sender ^ 1].mInbound = std::move(mSocks[sender].mOutbound);
		}

		template<typename T>
		error_code execute(Proto<T>& p0, Proto<T>& p1)
		{
#ifdef COPROTO_LOGGING
			if (p0.mBase->mName.size() == 0)
				p0.mBase->setName("main");
			if (p1.mBase->mName.size() == 0)
				p1.mBase->setName("main");
#endif

			//mScheds[0].mPrint = true;

			//bool v = false;
			//mSocks[0].mIdx = 0;
			//mSocks[0].mSched = this;
			//mSocks[1].mIdx = 1;
			//mSocks[1].mSched = this;

			mScheds[0].mSock = &mSocks[0];
			mScheds[1].mSock = &mSocks[1];


			p0.mBase->mSlotIdx = 0;
			p1.mBase->mSlotIdx = 0;
			mScheds[0].scheduleReady(*p0.mBase.get());
			mScheds[1].scheduleReady(*p1.mBase.get());
			mScheds[0].mRoundIdx = 0;
			mScheds[1].mRoundIdx = 0;
			while (
				mScheds[0].done() == false ||
				mScheds[1].done() == false)
			{

				if (mScheds[0].done() == false)
				{
					mScheds[0].runRound();

					if (mScheds[0].done())
					{
						auto e0 = p0.mBase->getErrorCode();
						if (e0)
							return e0;
					}

					sendMsgs(0);
					if (mScheds[0].mPrint)
						std::cout << "-------------- p0 suspend --------------" << std::endl;
				}

				if (mScheds[1].done() == false)
				{
					mScheds[1].runRound();


					if (mScheds[1].done())
					{
						auto e1 = p1.mBase->getErrorCode();
						if (e1)
							return e1;
					}

					sendMsgs(1);

					if (mScheds[0].mPrint)
						std::cout << "-------------- p1 suspend --------------" << std::endl;
				}
			}

			//std::cout << mScheds[0].getDot() << std::endl;
			//std::cout << mScheds[1].getDot() << std::endl;

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

		void endOfRoundTest();
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
