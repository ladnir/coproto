#pragma once
#include "coproto/Scheduler.h"
#include "coproto/Proto.h"
#include <cassert>


#include "coproto/Queue.h"
#include <iostream>

namespace coproto
{


	class LocalEvaluator
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
			bool mCanceled = false;

			LocalEvaluator* mEval = nullptr;

			error_code recv(span<u8> data) override;
			error_code send(span<u8> data) override;


			void cancel() override
			{
				mInbound.clear();
				mOutbound.clear();
				mCanceled = true;
			}
		};


		struct BlockingSock : public Socket
		{
			BlockingSock() = default;
			BlockingSock(BlockingSock&& o)
			{
				*this = std::move(o);
			}

			BlockingSock& operator=(BlockingSock&& o)
			{
				assert(mOther == nullptr);
				mOther = o.mOther;
				mEval = o.mEval;
				mCanceled = o.mCanceled;
				mInbound = std::move(o.mInbound);

				mOther->mOther = this;

				o.mOther = nullptr;
				o.mEval = nullptr;
				o.mCanceled = false;

				return *this;
			}

			BlockingSock* mOther = nullptr;
			LocalEvaluator* mEval = nullptr;
			bool mCanceled = false;
			BlockingQueue<std::vector<u8>> mInbound;

			error_code recv(span<u8> data) override;
			error_code send(span<u8> data) override;

			void cancel() override
			{
				mCanceled = true;
				mOther->mInbound.emplace();
			}
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
					cancel,
					join
				};
				u64 mIdx = ~0ull;
				Type mType;
				span<u8> mData;
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
				Worker() = default;
				Worker(Worker&&) = default;
				~Worker()
				{
					join();
				}
				BlockingQueue<Op> mWorkQueue;

				LocalEvaluator* mEval = nullptr;
				bool mHasThread = false;
				std::thread mThread;

				std::array<Op, 2> mSend, mRecv;
				std::array<bool, 2> mStopped{ false, false };
				bool mCanceled;

				void clear()
				{
					join();
					mWorkQueue.clear();
					mSend = std::array<Op, 2>{};
					mRecv = std::array<Op, 2>{};
					mStopped = { false, false };
					mCanceled = false;
				}

				void startThread();

				void completeOp(u64 i);
				void cancel();

				void process(Op op);

				void join()
				{
					if (mHasThread)
					{
						mWorkQueue.emplace(0, Op::join);
						mThread.join();
						mHasThread = false;
					}
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
		AsyncSock::Worker mSocketWorker;;


		std::array<BlockingSock, 2> getSocketPair()
		{
			std::array<BlockingSock, 2> bb;
			bb[0].mEval = this;
			bb[1].mEval = this;
			bb[0].mOther = &bb[1];
			bb[1].mOther = &bb[0];

			return bb;
		}
		//std::array<Scheduler, 2> mScheds;

		LocalEvaluator() 
		{

			mSocks[0].mEval = this;
			mSocks[1].mEval = this;
			mBlkSocks[0].mEval = this;
			mBlkSocks[1].mEval = this;
			mBlkSocks[0].mOther = &mBlkSocks[1];
			mBlkSocks[1].mOther = &mBlkSocks[0];

			mAsyncSock[0].mIdx = 0;
			mAsyncSock[1].mIdx = 1;
			mAsyncSock[0].mWorker = &mSocketWorker;
			mAsyncSock[1].mWorker = &mSocketWorker;
			mSocketWorker.mEval = this;
			
			//auto socketWorker = AsyncSock::Worker();
			//mAsyncSock[0].mIdx = 0;
			//mAsyncSock[1].mIdx = 1;
			//mAsyncSock[0].mWorker = &socketWorker;
			//mAsyncSock[1].mWorker = &socketWorker;
			//socketWorker.mEval = this;
			//socketWorker.mEval = this;
		}

		u64 mOpIdx = 0;
		u64 mErrorIdx = ~0ull;

		error_code getError()
		{
			if (mOpIdx++ == mErrorIdx)
				return code::ioError;
			return {};
		}

		void sendMsgs(u64 sender, error_code ec)
		{
			auto error = ec && ec != code::suspend;

			if (mSocks[sender].mCanceled || error)
				mSocks[sender ^ 1].cancel();
			else
			{
				//assert(mSocks[sender ^ 1].mInbound.size() == 0);
				mSocks[sender ^ 1].mInbound = std::move(mSocks[sender].mOutbound);
			}
		}

		error_code execute(internal::ProtoImpl& p0, internal::ProtoImpl& p1, Type type = Type::interlace, bool print = false);
	};



	inline std::ostream& operator<<(std::ostream& o, const LocalEvaluator::Type& t)
	{

		switch (t)
		{

		case LocalEvaluator::Type::interlace:
			o << "interlace";
			return o;
		case LocalEvaluator::Type::blocking:
			o << "blocking";
			return o;
		case LocalEvaluator::Type::async:
			o << "async";
			return o;
		case LocalEvaluator::Type::asyncThread:
			o << "asyncThread";
			return o;
		default:
			break;
		}
		return o;
	}
}