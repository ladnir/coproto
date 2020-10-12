// coproto.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING

#include <iostream>
// coproto.cpp : Defines the entry point for the application.
//

#include "cryptoTools/Network/Channel.h"
#include <coroutine>
#include <iostream>
#include "error_code.h"
#include "TypeTraits.h"
#include "Buffers.h"

namespace coproto
{

	class Scheduler
	{
	public:
		virtual error_code recv(oc::u64 id, Buffer& data) = 0;
		virtual error_code send(oc::u64 id, Buffer& data) = 0;
	};

	template<typename Op>
	struct WithEC
	{
		Op mOp;
		WithEC(Op&& r)
			: mOp(std::forward<Op>(r))
		{}

	};


	struct Recv
	{
		internal::Inline<Buffer> mStorage;

		WithEC<Recv> getErrorCode() {
			return WithEC<Recv>(std::move(*this));
		}
	};

	struct Send
	{
		internal::Inline<Buffer> mStorage;
		WithEC<Send> getErrorCode() {
			return WithEC<Send>(std::move(*this));
		}
	};

	template<typename Container>
	static typename std::enable_if<is_trivial_container_v<Container> || std::is_trivial_v<Container>, Recv>::type receive(Container& r) {
		Recv recv;
		recv.mStorage.emplace<RefBuffer<Container>>(r);
		return recv;
	}

	
	template<typename Container>
	static typename std::enable_if<is_trivial_container_v<Container> || std::is_trivial_v<Container>, Recv>::type 
		receiveFixedSize(Container& r) 
	{
		Recv recv;
		recv.mStorage.emplace<RefBuffer<Container, false>>(r);
		return recv;
	}

	template<typename Container>
	static Send send(Container& s) {
		Send send;
		send.mStorage.emplace<RefBuffer<Container>>(s);
		return send;
	}

	struct RecvAwaiter;
	struct SendAwaiter;
	template<typename Interface>
	struct ECAwaiter;
	struct ProtoPromise;

	struct RecvAwaiter
	{
		Recv mRecv;
		bool mHasResult = false, mReturnErrors = false;
		using coro_handle = std::coroutine_handle<ProtoPromise>;
		coro_handle mHandle;
		error_code mEc;

		RecvAwaiter(coro_handle handle, Recv&& data)
			: mRecv(std::move(data))
			, mHandle(handle)
		{}

		bool await_ready();
		void await_suspend(coro_handle h) { }
		void await_resume();
		error_code get_error_code() { return mEc; }
	};


	struct SendAwaiter
	{
		Send mData;
		bool mHasResult = false, mReturnErrors = false;
		using coro_handle = std::coroutine_handle<ProtoPromise>;
		coro_handle mHandle;
		error_code mEc;
		SendAwaiter(coro_handle handle, Send&& data)
			: mData(std::move(data))
			, mHandle(handle)
		{}

		bool await_ready();
		void await_suspend(coro_handle h) { }
		void await_resume();
		error_code get_error_code() { return mEc; }
	};

	template<typename T>
	struct ECAwaiter
	{
		using coro_handle = std::coroutine_handle<ProtoPromise>;
		T mOp;
		ECAwaiter(T&& op)
			: mOp(std::forward<T>(op))
		{
			mOp.mReturnErrors = true;
		}

		bool await_ready() { return mOp.await_ready(); };

		void await_suspend(coro_handle h) { mOp.await_suspend(h); }

		error_code await_resume() {
			mOp.await_resume();
			return mOp.get_error_code();
		}

	};
	class Proto;

	struct ProtoPromise {

		Scheduler* mSched = nullptr;
		oc::u64 mId = -1;
		error_code mEc;
		std::exception_ptr mExPtr;

		void setError(error_code ec)
		{
			assert(ec);
			mEc = ec;
		}

		Proto get_return_object();
		std::suspend_always initial_suspend() { return {}; }
		std::suspend_always final_suspend() noexcept { return {}; }
		void return_void() {}
		void unhandled_exception() {
			mExPtr = std::current_exception();
			setError(code::uncaughtException);
		}


		RecvAwaiter await_transform(Recv&& recv)
		{
			return RecvAwaiter(std::coroutine_handle<ProtoPromise>::from_promise(*this), std::move(recv));
		}
		SendAwaiter await_transform(Send&& send)
		{
			return SendAwaiter(std::coroutine_handle<ProtoPromise>::from_promise(*this),std::move(send));
		}
		ECAwaiter<RecvAwaiter> await_transform(WithEC<Recv>&& recv)
		{
			return ECAwaiter<RecvAwaiter>(await_transform(std::move(recv.mOp)));
		}
		ECAwaiter<SendAwaiter> await_transform(WithEC<Send>&& send)
		{
			return ECAwaiter<SendAwaiter>(await_transform(std::move(send.mOp)));
		}
	};


	class Proto
	{
	public:

		using promise_type = ProtoPromise;
		using coro_handle = std::coroutine_handle<ProtoPromise>;
		coro_handle mHandle;
		Proto(ProtoPromise& prom)
			:mHandle(coro_handle::from_promise(prom))
		{ }

		~Proto()
		{
			mHandle.destroy();
		}

		void setScheduler(Scheduler& sched, oc::u64 id)
		{
			mHandle.promise().mSched = &sched;
			mHandle.promise().mId = id;
		}

		error_code getErrorCode()
		{
			return mHandle.promise().mEc;
		}

		void getException()
		{
			std::rethrow_exception(mHandle.promise().mExPtr);
		}

		bool done()
		{
			return getErrorCode() || mHandle.done();
		}
		void resume()
		{
			mHandle.resume();
		}



	};


	class InterlaceScheduler : public Scheduler
	{
	public:

		std::array<std::list<std::vector<oc::u8>>, 2> mBuffs;

		error_code recv(oc::u64 id, Buffer& data) override;
		error_code send(oc::u64 id, Buffer& data) override;
		error_code execute(Proto& p0, Proto& p1);
	};

	namespace tests
	{

		void strSendRecvTest();
		void arraySendRecvTest();
		void intSendRecvTest();
		void resizeSendRecvTest();
		void zeroSendRecvTest();
		void badRecvSizeTest();

		void zeroSendErrorCodeTest();
		void badRecvSizeErrorCodeTest();

		void throwsTest();

	}


}