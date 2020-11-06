#pragma once
#include "Scheduler.h"
#include "Proto.h"
#include <cassert>


#include <mutex>
#include <condition_variable>
#include <deque>

namespace coproto
{

	template <typename T>
	class queue
	{
	private:
		std::mutex              d_mutex;
		std::condition_variable d_condition;
		std::deque<T>           d_queue;
	public:
		void push(T&& value) {
			{
				std::unique_lock<std::mutex> lock(this->d_mutex);
				d_queue.emplace_front(std::move(value));
			}
			this->d_condition.notify_one();
		}

		template<typename... Args>
		void emplace(Args&&... args) {
			{
				std::unique_lock<std::mutex> lock(this->d_mutex);
				d_queue.emplace_front(std::forward<Args>(args)...);
			}
			this->d_condition.notify_one();
		}

		T pop() {
			std::unique_lock<std::mutex> lock(this->d_mutex);
			this->d_condition.wait(lock, [this] { return !this->d_queue.empty(); });
			T rc(std::move(this->d_queue.back()));
			this->d_queue.pop_back();
			return rc;
		}
	};


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
			queue<std::vector<u8>> mInbound;

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
			queue<Op> mWorkQueue;

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
		std::array<Scheduler, 2> mScheds;

		void sendMsgs(u64 sender)
		{
			assert(mSocks[sender ^ 1].mInbound.size() == 0);
			mSocks[sender ^ 1].mInbound = std::move(mSocks[sender].mOutbound);
		}

		error_code execute(Resumable& p0, Resumable& p1, Type type = Type::interlace)
		{
#ifdef COPROTO_LOGGING
			if (p0.mBase->mName.size() == 0)
				p0.mBase->setName("main");
			if (p1.mBase->mName.size() == 0)
				p1.mBase->setName("main");
#endif

			p0.mSlotIdx = 0;
			p1.mSlotIdx = 0;
			mScheds[0].scheduleReady(p0);
			mScheds[1].scheduleReady(p1);
			mScheds[0].mRoundIdx = 0;
			mScheds[1].mRoundIdx = 0;

			if (type == Type::interlace)
			{

				mScheds[0].mSock = &mSocks[0];
				mScheds[1].mSock = &mSocks[1];



				while (
					mScheds[0].done() == false ||
					mScheds[1].done() == false)
				{

					if (mScheds[0].done() == false)
					{
						mScheds[0].run();

						if (mScheds[0].done())
						{
							auto e0 = p0.getErrorCode();
							if (e0)
								return e0;
						}

						sendMsgs(0);
						if (mScheds[0].mPrint)
							std::cout << "-------------- p0 suspend --------------" << std::endl;
					}

					if (mScheds[1].done() == false)
					{
						mScheds[1].run();


						if (mScheds[1].done())
						{
							auto e1 = p1.getErrorCode();
							if (e1)
								return e1;
						}

						sendMsgs(1);

						if (mScheds[0].mPrint)
							std::cout << "-------------- p1 suspend --------------" << std::endl;
					}
				}

			}
			else if (type == Type::blocking)
			{
				mScheds[0].mSock = &mBlkSocks[0];
				mScheds[1].mSock = &mBlkSocks[1];


				mBlkSocks[0].mOther = &mBlkSocks[1];
				mBlkSocks[1].mOther = &mBlkSocks[0];

				auto thrd = std::thread([&]() {
					mScheds[0].run();

					if (p0.getErrorCode())
						mBlkSocks[1].mInbound.emplace();

					});


				mScheds[1].run();

				if (p1.getErrorCode())
					mBlkSocks[0].mInbound.emplace();

				thrd.join();

				if (p0.done() == false)
					throw std::runtime_error(COPROTO_LOCATION);
				if (p1.done() == false)
					throw std::runtime_error(COPROTO_LOCATION);

				auto e0 = p0.getErrorCode();
				if (e0)
					return e0;
				auto e1 = p1.getErrorCode();
				if (e1)
					return e1;

			}
			else
			{
				throw std::runtime_error(COPROTO_LOCATION);
			}

			return {};
		}
	};

}