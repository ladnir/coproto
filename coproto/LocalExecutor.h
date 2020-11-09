#pragma once
#include "Scheduler.h"
#include "Proto.h"
#include <cassert>


#include "Queue.h"

namespace coproto
{


	class LocalExecutor
	{
	public:

		enum Type
		{
			interlace,
			blocking,
			async,
			asyncThread
		};

		struct InterlaceSock : public Socket
		{
			std::list<std::vector<u8>> mInbound, mOutbound;

			error_code recv(span<u8> data) override;
			error_code send(span<u8> data) override;
		};


		struct BlockingSock : public Socket
		{
			BlockingSock* mOther;
			BlockingQueue<std::vector<u8>> mInbound;

			error_code recv(span<u8> data) override;
			error_code send(span<u8> data) override;
		};


		struct AsyncSock : public AsyncSocket
		{
			struct Op
			{
				enum Type
				{
					send,
					recv,
					stop,
					cancel
				};
				u64 mIdx = ~0ull;
				Type mType;
				std::span<u8> mData;
				Continutation mCont;

				Op() = default;
				Op(u64 i, Type t, span<u8> d, Continutation&& cont)
					: mIdx(i)
					, mType(t)
					, mData(d)
					, mCont(std::move(cont))
				{}
				Op(u64 i, Type t, span<u8> d)
					: mIdx(i)
					, mType(t)
					, mData(d)
				{}

				Op(u64 i, Type t)
					: mIdx(i)
					, mType(t)
				{}

				operator bool() const
				{
					return mIdx != ~0ull;
				}
			};

			struct Worker
			{
				BlockingQueue<Op> mWorkQueue;

				bool mHasThread=false;
				std::thread mThread;

				std::array<Op, 2> mSend, mRecv;
				std::array<bool, 2> mStopped;
				bool mCanceled;

				void startThread();

				void completeOp(u64 i);
				void cancel();

				void process(Op op);

				void join()
				{
					if(mHasThread)
						mThread.join();
				}
			};

			Worker* mWorker = nullptr;
			u64 mIdx;

			void recv(span<u8> data, Continutation&& cont) override
			{
				assert(mWorker != nullptr);
				enqueue({ mIdx, Op::recv, data, std::move(cont) });
			}
			void send(span<u8> data, Continutation&& cont) override
			{
				assert(mWorker != nullptr);
				enqueue({ mIdx, Op::send, data, std::move(cont) });
			}

			void cancel() override
			{
				assert(mWorker != nullptr);
				enqueue({ mIdx, Op::cancel });
			}

			void stop()
			{
				assert(mWorker != nullptr);
				enqueue({ mIdx, Op::stop });
			}



			void enqueue(Op op)
			{
				if (mWorker->mHasThread == false)
					mWorker->process(std::move(op));
				else
					mWorker->mWorkQueue.emplace(std::move(op));

			}
		};

		std::array<InterlaceSock, 2> mSocks;
		std::array<BlockingSock, 2> mBlkSocks;
		std::array<AsyncSock, 2> mAsyncSock;
		std::array<Scheduler, 2> mScheds;

		void sendMsgs(u64 sender)
		{
			assert(mSocks[sender ^ 1].mInbound.size() == 0);
			mSocks[sender ^ 1].mInbound = std::move(mSocks[sender].mOutbound);
		}

		error_code execute(Resumable& p0, Resumable& p1, Type type = Type::interlace);
	};

}