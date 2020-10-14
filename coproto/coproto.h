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
#include "Scheduler.h"
//#include "Task.h"

namespace coproto
{
	std::string hexPtr(void* p);

	namespace v1
	{

		class Proto;
		struct EcProto;
		struct ProtoAwaiter;
		struct EcProtoAwaiter;

		class Scheduler
		{
		public:
			virtual error_code recv(Buffer& data) = 0;
			virtual error_code send(Buffer& data) = 0;

			virtual void addProto(ProtoAwaiter& proto) = 0;
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

		template<typename Container>
		struct TypedRecv
		{
			using value_type = Container;
		};


		struct Send
		{
			internal::Inline<Buffer> mStorage;
			WithEC<Send> getErrorCode() {
				return WithEC<Send>(std::move(*this));
			}
		};

		template<typename Container>
		static typename std::enable_if<is_trivial_container_v<Container> || std::is_trivial_v<Container>, Recv>::type
			receive(Container& r) {
			Recv recv;
			recv.mStorage.emplace<RefBuffer<Container>>(r);
			return recv;
		}

		template<typename Container>
		static typename std::enable_if<is_trivial_container_v<Container> || std::is_trivial_v<Container>, TypedRecv<Container>>::type
			receive() {
			return TypedRecv<Container>{};
		}

		template<typename T>
		static typename std::enable_if<std::is_trivial_v<T>, TypedRecv<std::vector<T>>>::type
			receiveVec() {
			return TypedRecv<std::vector<T>>{};
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

		template<typename Container>
		static Send send(Container&& s) {
			Send send;
			send.mStorage.emplace<MoveBuffer<Container>>(std::forward<Container>(s));
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

		template<typename Container>
		struct TypedRecvAwaiter
		{
			Container mContainer;
			bool mHasResult = false, mReturnErrors = false;
			using coro_handle = std::coroutine_handle<ProtoPromise>;
			coro_handle mHandle;
			error_code mEc;

			TypedRecvAwaiter(coro_handle handle)
				: mHandle(handle)
			{}

			bool await_ready()
			{
				auto buff = RefBuffer(mContainer);
				mEc = mHandle.promise().mSched->recv(buff);

				// we have a result (message or error) so long as 
				// we done have a no_message_available code.
				mHasResult = mEc != code::noMessageAvailable;

				// Check if we should stop the protocol
				// and have it output an error.
				if (mEc && mHasResult && !mReturnErrors)
				{
					mHandle.promise().setError(mEc);
					return false;
				}
				return mHasResult;
			}

			void await_suspend(coro_handle h) { }
			Container await_resume()
			{
				if (mHasResult == false)
				{
					await_ready();
					assert(mHasResult);
				}
				return std::move(mContainer);
			}
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
		struct ProtoAwaiter;
		struct AsyncProtoAwaiter;



		struct ProtoPromise {

			ProtoPromise()
			{
				//std::cout << " ProtoPromise " << hexPtr(this) << std::endl;
			}
			~ProtoPromise()
			{
				//std::cout << " ~ProtoPromise " << hexPtr(this) << std::endl;
			}

			ProtoPromise(const ProtoPromise&) = delete;
			ProtoPromise(ProtoPromise&&) = delete;

			Scheduler* mSched = nullptr;
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

			template<typename T>
			TypedRecvAwaiter<T> await_transform(TypedRecv<T> recv)
			{
				return TypedRecvAwaiter<T>(std::coroutine_handle<ProtoPromise>::from_promise(*this));
			}

			SendAwaiter await_transform(Send&& send)
			{
				return SendAwaiter(std::coroutine_handle<ProtoPromise>::from_promise(*this), std::move(send));
			}
			ECAwaiter<RecvAwaiter> await_transform(WithEC<Recv>&& recv)
			{
				return ECAwaiter<RecvAwaiter>(await_transform(std::move(recv.mOp)));
			}
			ECAwaiter<SendAwaiter> await_transform(WithEC<Send>&& send)
			{
				return ECAwaiter<SendAwaiter>(await_transform(std::move(send.mOp)));
			}

			ProtoAwaiter await_transform(Proto&&);

			EcProtoAwaiter await_transform(EcProto&&);


			//template<typename T>
			//TaskAwaiter<T> await_transform(Task<T>& t)
			//{
			//	return TaskAwaiter<T>(
			//		std::coroutine_handle<ProtoPromise>::from_promise(*this),
			//		t);
			//}

		};

		template<typename Handle>
		struct SharedHandle
		{

			struct ControlBlock
			{
				Handle mHandle;
				u64 mCount = 1;

				ControlBlock(Handle handle)
					: mHandle(std::move(handle))
				{}
			};

			ControlBlock* mHandle = nullptr;

			SharedHandle(Handle handle)
				: mHandle(new ControlBlock(std::move(handle)))
			{
				//std::hash<Handle> h;
				//std::cout << " SharedHandle " << h(mHandle->mHandle) << std::endl;

			}

			SharedHandle() = default;

			SharedHandle(const SharedHandle& o)
				: mHandle(o.mHandle)
			{
				if (mHandle)
					++mHandle->mCount;
			}

			SharedHandle(SharedHandle&& o)
				: mHandle(o.mHandle)
			{
				o.mHandle = nullptr;
			}

			~SharedHandle()
			{
				if (mHandle && --mHandle->mCount == 0)
				{
					//std::hash<Handle> h;
					//std::cout << " ~SharedHandle " << h(mHandle->mHandle) << std::endl;
					mHandle->mHandle.destroy();
					delete mHandle;
				}
			}

			Handle& get()
			{
				assert(mHandle);
				return mHandle->mHandle;
			}
			const Handle& get()const
			{
				assert(mHandle);
				return mHandle->mHandle;
			}

			operator bool() const
			{
				return mHandle != nullptr;
			}
		};

		struct EcProto
		{
			using coro_handle = std::coroutine_handle<ProtoPromise>;
			SharedHandle<coro_handle> mHandle;

			operator error_code() const
			{
				return mHandle.get().promise().mEc;
			}

			operator bool() const
			{
				return static_cast<bool>(mHandle.get().promise().mEc);
			}
		};


		struct AsyncProto
		{
			using coro_handle = std::coroutine_handle<ProtoPromise>;
			SharedHandle<coro_handle> mHandle;
		};

		class Proto
		{
		public:



			using promise_type = ProtoPromise;
			using coro_handle = std::coroutine_handle<ProtoPromise>;
			SharedHandle<coro_handle> mHandle;
			Proto(ProtoPromise& prom)
				:mHandle(coro_handle::from_promise(prom))
			{
				//std::cout << " Proto " << hexPtr(this) << std::endl;
			}
			Proto(SharedHandle<coro_handle>&& prom)
				:mHandle(std::move(prom))
			{
				//std::cout << " Proto " << hexPtr(this) << std::endl;
			}


			Proto(const Proto& proto) = delete;
			Proto(Proto&& p)
				: mHandle(std::move(p.mHandle))
			{
			}

			~Proto()
			{

			}

			void setScheduler(Scheduler& sched)
			{
				mHandle.get().promise().mSched = &sched;
			}

			EcProto getErrorCode()
			{
				return { mHandle };
			}

			void getException()
			{
				std::rethrow_exception(mHandle.get().promise().mExPtr);
			}

			AsyncProto async()
			{
				return { mHandle };
			}

			bool done()
			{
				return getErrorCode() || mHandle.get().done();
			}
			void resume()
			{
				mHandle.get().resume();
			}
		};


		struct ProtoAwaiter
		{
			Proto mProto;
			bool mReturnErrors = false;
			using coro_handle = std::coroutine_handle<ProtoPromise>;
			coro_handle mParentHandle;

			ProtoAwaiter(coro_handle parent, SharedHandle<coro_handle>&& proto)
				: mProto(std::move(proto))
				, mParentHandle(parent)
			{
			}

			ProtoAwaiter(const ProtoAwaiter&) = delete;
			ProtoAwaiter(ProtoAwaiter&&) = delete;

			bool await_ready()
			{
				if (!mProto.done())
				{
					mParentHandle.promise().mSched->addProto(*this);
				}

				return mProto.done();
			}

			void await_suspend(coro_handle h) { }
			void await_resume() {
				assert(!mProto.mHandle.get().promise().mEc);
			}
		};
		//struct CompletionToken
		//{

		//};

		//struct AsyncProtoAwaiter
		//{
		//	using coro_handle = std::coroutine_handle<ProtoPromise>;
		//	SharedHandle<coro_handle> mHandle;

		//	bool await_ready()
		//	{
		//		return true;
		//	}

		//	void await_suspend(coro_handle h) { }
		//	CompletionToken await_resume() {
		//	}
		//};

		struct EcProtoAwaiter : public ProtoAwaiter
		{

			EcProtoAwaiter(coro_handle parent, EcProto&& proto)
				: ProtoAwaiter(parent, std::move(proto.mHandle))
			{
				mReturnErrors = true;
			}

			EcProtoAwaiter(const ProtoAwaiter&) = delete;
			EcProtoAwaiter(ProtoAwaiter&&) = delete;

			error_code await_resume() {
				return mProto.mHandle.get().promise().mEc;
			}
		};

		class LocalScheduler
		{
		public:

			struct Sched : public Scheduler
			{
				u64 mIdx;
				LocalScheduler* mSched;

				struct P
				{
					Proto* mProto;
					ProtoAwaiter* mAwaiter;
				};

				std::vector<P> mStack;

				error_code recv(Buffer& data) override;
				error_code send(Buffer& data) override;

				void addProto(ProtoAwaiter& awaiter) override;


				void runOne();
			};

			std::array<std::list<std::vector<oc::u8>>, 2> mBuffs;
			std::array<Sched, 2> mScheds;

			error_code execute(Proto& p0, Proto& p1);
		};

		namespace tests
		{

			void strSendRecvTest();
			void arraySendRecvTest();
			void intSendRecvTest();
			void resizeSendRecvTest();
			void moveSendRecvTest();
			void typedRecvTest();

			void nestedProtocolTest();
			void nestedProtocolThrowTest();
			void nestedProtocolErrorCodeTest();

			void zeroSendRecvTest();
			void badRecvSizeTest();

			void zeroSendErrorCodeTest();
			void badRecvSizeErrorCodeTest();

			void throwsTest();

		}

	}

}