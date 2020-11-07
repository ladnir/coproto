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
			async
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
					sendBegin,
					sendComplete,
					stop
				};
				Type mType;
				std::span<u8> mData;
				Continutation mCont;

				Op() = default;
				Op(Type t, span<u8> d, Continutation&& cont)
					: mType(t)
					, mData(d)
					, mCont(std::move(cont))
				{}
				Op(Type t, span<u8> d)
					: mType(t)
					, mData(d)
				{}

				Op(Type t)
					: mType(t)
				{}

				void clear()
				{
					mType = stop;
					mData = {};
					mCont = {};
				}
			};
			AsyncSock* mOther;
			BlockingQueue<Op> mWorkQueue;

			std::thread mWorker;

			void recv(span<u8> data, Continutation&& cont)
			{
				mWorkQueue.emplace(Op::recv, data, std::move(cont));
			}
			void send(span<u8> data, Continutation&& cont)
			{
				mWorkQueue.emplace(Op::send, data, std::move(cont));
			}

			void startWorker()
			{
				mWorker = std::thread([this]() {
					
					std::vector<std::vector<u8>> inboundData;
					Op curSend;
					Op curRecv;

					auto complete = [&](span<u8> src, span<u8> dest, Continutation& cont)
					{
						// there is an active send op.
						if (src.size() == dest.size())
						{
							std::copy(src.begin(), src.end(), dest.begin());
							cont({}, dest.size());
						}
						else
						{
							cont(code::badBufferSize, 0);
						}

						curRecv.clear();
						mOther->mWorkQueue.emplace(Op::sendComplete);
					};

					while (true)
					{
						auto op = mWorkQueue.pop();

						if (op.mType == Op::send)
						{
							assert(!curSend.mCont);

							curSend = std::move(op);

							mOther->mWorkQueue.emplace(Op::sendBegin, curSend.mData);
						}
						else if (op.mType == Op::recv)
						{
							assert(!curRecv.mCont);

							
							if (curRecv.mType == Op::sendBegin)
							{
								// there is an active send op.
								complete(curRecv.mData, op.mData, op.mCont);
							}
							else
							{
								curRecv = std::move(op);
							}
						}
						else if (op.mType == Op::sendBegin)
						{

							if (curRecv.mType == Op::recv)
							{
								// there is an active recv op
								complete(op.mData, curRecv.mData, curRecv.mCont);
							}
							else
								curRecv = std::move(op);
						}
						else if (op.mType == Op::sendComplete)
						{
							assert(curSend.mCont);
							curSend.mCont({}, curSend.mData.size());
							curSend.clear();
						}
						else
						{
							assert(op.mType == Op::stop);
							std::cout << hexPtr(this) << "exit " << std::endl;

							if (curSend.mCont)
								curSend.mCont(code::ioError, 0);

							if (curRecv.mCont)
								curRecv.mCont(code::ioError, 0);

							curSend.clear();
							curRecv.clear();
							return;
						}
					}
				});
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