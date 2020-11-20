#include "Socket.h"

namespace coproto
{
	std::string hexPtr(void*);


	void SockScheduler::initAsyncRecv()
	{
		assert(mActiveRecv == false && mRecvBuffers.size() > 0);
		mActiveRecv = true;

		if (mHaveHeader)
		{
			if (mSched->mPrint)
				std::cout << hexPtr(this) << " recved header and resuming..." << std::endl;

			asyncRecvBody();
		}
		else
		{
			asyncRecvHeader();
		}
	}

	void SockScheduler::initRecv()
	{
		error_code ec;
		ec = recvHeader();

		if (!ec)
		{
			auto h = getRecvHeaderSlot();
			auto iter = mRecvBuffers.find(h);

			if (iter != mRecvBuffers.end())
			{
				auto& data = std::get<0>(iter->second);

				auto d = data->asSpan(getRecvHeaderSize());
				if (d.size() != getRecvHeaderSize())
				{
					ec = code::badBufferSize;
					if (mSched->mPrint)
						std::cout << ec.message()
						<< "\nBytes received = " << getRecvHeaderSize()
						<< "\nSize of buffer = " << d.size() << std::endl;
				}
				else
				{
					try
					{
						ec = mSync->recv(d);
					}
					catch (...)
					{
						ec = code::ioError;
					}
				}

				if (ec == code::suspend)
					return;

				mHaveHeader = false;

				if (ec)
				{
					cancelRecvQueue(ec);
				}
				else
				{
					auto res = std::get<1>(iter->second);
					mRecvBuffers.erase(iter);
					mSched->resume(res);
				}
			}
		}
		else  if (ec != code::suspend)
		{
			cancelRecvQueue(ec);
		}
	}

	void SockScheduler::asyncRecvHeader()
	{
		assert(mHaveHeader == false);

		//if (mPrint)
		//	std::cout << hexPtr(this) << " recving header "  << std::endl;

		mAsync->recv(mRecvHeader, [this](error_code ec, u64 bt) {
			mSched->dispatch([this, ec, bt]() {

				mHaveHeader = true;
				if (ec)
					cancelRecvQueue(ec);
				else
					asyncRecvBody();

				});
			});

	}

	void SockScheduler::asyncRecvBody()
	{

		mSched->dispatch([this]() {
			assert(mHaveHeader);

			auto h = getRecvHeaderSlot();
			auto iter = mRecvBuffers.find(h);
			if (iter != mRecvBuffers.end())
			{
				auto data = std::get<0>(iter->second)->asSpan(getRecvHeaderSize());

				if (data.size() == getRecvHeaderSize())
				{
					mAsync->recv(data, [this](error_code ec, u64 bt) {
						mSched->dispatch([this, ec, bt]() {


							auto h = getRecvHeaderSlot();
							auto iter = mRecvBuffers.find(h);
							auto proto = std::get<1>(iter->second);

							//if (mPrint)
							//	std::cout << hexPtr(this) << " recved body ~~ " << proto->getName() << std::endl;

							if (!ec)
							{
								mRecvBuffers.erase(iter);

								mSched->mReady.push_back(proto);

								mHaveHeader = false;
								mActiveRecv = false;

								if (mRecvBuffers.size())
								{
									initAsyncRecv();
								}

								mSched->run();
							}
							else
							{
								cancelRecvQueue(ec);
							}

							});
						});
				}
				else
				{

					auto ec = code::badBufferSize;
					auto proto = std::get<1>(iter->second);
					mRecvBuffers.erase(iter);
					proto->setError(ec, nullptr);
					mSched->mReady.push_back(proto);
					//mHaveHeader = false;

					cancelRecvQueue(code::ioError);
				}
			}
			else
			{
				mActiveRecv = false;
				if (mSched->mPrint)
					std::cout << hexPtr(this) << " recved header but suspend... " << h << std::endl;
			}

			});


	}


	error_code SockScheduler::recvHeader()
	{
		if (mHaveHeader)
			return {};

		try
		{
			auto ec = mSync->recv(mRecvHeader);
			if (ec)
				return ec;
		}
		catch (...)
		{
			return code::ioError;
		}

		mHaveHeader = true;
		return {};

	}

	void SockScheduler::recv(Scheduler& sched, RecvBuffer* data, u32 slot, Resumable* res)
	{
		//if(mPrint)
		//	std::cout << " @@@@@@@@@ " << hexPtr(this) << " recv "<< res->getName() << " on " << slot << std::endl;
		if (mSched != &sched)
		{
			mSched = &sched;
			assert(std::find(mSched->mSockets.begin(), mSched->mSockets.end(), this) == mSched->mSockets.end());
			mSched->mSockets.push_back(this);
		}

		assert(mRecvBuffers.find(slot) == mRecvBuffers.end());
		mRecvBuffers.insert(std::make_pair(slot, std::make_tuple(data, res)));

		if (mAsync)
		{
			if (mActiveRecv == false)
			{
				initAsyncRecv();
			}
		}
		else
		{
			initRecv();
		}
	}

	void SockScheduler::send_(Scheduler& sched, SendBuffer&& op, u32 slot, Resumable* res)
	{
#ifdef COPROTO_LOGGING
		if (mPrint)
		{
			if (res)
				std::cout << " @@@@@@@@@ " << hexPtr(this) << " send " << res->getName() << " " << slot << std::endl;
			else
				std::cout << " @@@@@@@@@ " << hexPtr(this) << " send _____________ " << slot << std::endl;

		}
#endif
		if (mSched != &sched)
		{
			mSched = &sched;
			assert(std::find(mSched->mSockets.begin(), mSched->mSockets.end(), this) == mSched->mSockets.end());
			mSched->mSockets.push_back(this);
		}

		assert(slot != ~u32(0));
		mSendBuffers.emplace_back(std::move(op), slot, res);

		if (mSync)
		{
			if (mSendBuffers.size() == 1)
				initSend();
		}
		else
		{

			if (mActiveSend == false)
				initAsyncSend();
		}
	}

	void SockScheduler::initAsyncSend()
	{
		assert(mActiveSend == false && mSendBuffers.size());
		mActiveSend = true;

		auto data = std::get<0>(mSendBuffers.front()).asSpan();

		assert(data.size() != 0);
		assert(data.size() < std::numeric_limits<u32>::max());

		getSendHeaderSlot() = std::get<1>(mSendBuffers.front());
		getSendHeaderSize() = static_cast<u32>(data.size());

		if (mSched->mPrint)
			std::cout << "******** " << hexPtr(this) << " init send on " << getSendHeaderSlot() << std::endl;

		//if (mPrint)
		//	std::cout << hexPtr(this) << " " << std::this_thread::get_id() << " sending header" << std::endl;
		mAsync->send(mSendHeader, [this, data](error_code ec, u64 bt) {


			//dispatch([this]() {
			//	if (mPrint)
			//		std::cout << hexPtr(this) << " " << std::this_thread::get_id() << " sent   header" << std::endl;
			//	});

			if (!ec)
			{
				mAsync->send(data, [this](error_code ec, u64 bt) {

					mSched->dispatch([this, ec, bt]() {

						auto res = std::get<2>(mSendBuffers.front());
						//if (mPrint && res)
						//	std::cout << hexPtr(this) << " sent   body ~~ " << res->getName() << std::endl;

						if (!ec)
						{
							if (res)
							{
								mSched->mReady.push_back(res);
							}
							mSendBuffers.pop_front();
							mActiveSend = false;

							if (mSendBuffers.size())
								initAsyncSend();

							mSched->run();
						}
						else
						{
							cancelSendQueue(ec);
						}
						});
					});
			}
			else
			{
				mSched->dispatch([this, ec, bt]() {
					cancelSendQueue(ec);
					});
			}
			});
	}

	void SockScheduler::initSend()
	{
		while (mSendBuffers.size())
		{


			auto& op = std::get<0>(mSendBuffers.front());
			auto& slot = std::get<1>(mSendBuffers.front());
			auto& res = std::get<2>(mSendBuffers.front());

			auto data = op.asSpan();

			if (mSentHeader == false)
			{


				assert(data.size() != 0);
				assert(data.size() < std::numeric_limits<u32>::max());

				getSendHeaderSlot() = slot;
				getSendHeaderSize() = static_cast<u32>(data.size());

				error_code ec;
				try {
					ec = mSync->send(mSendHeader);
				}
				catch (...)
				{
					ec = code::ioError;
				}

				if (ec == code::suspend)
					return;
				else if (ec)
				{
					cancelSendQueue(ec);
					return;
				}

				mSentHeader = true;
			}


			error_code ec;
			try {
				ec = mSync->send(data);
			}
			catch (...)
			{
				ec = code::ioError;
			}

			if (ec == code::suspend)
			{
				return;
			}
			else if (ec)
			{
				cancelSendQueue(ec);
			}
			else
			{
				mSentHeader = false;
				if (res)
					mSched->resume(res);

				mSendBuffers.pop_front();
			}
		}
	}


	void SockScheduler::cancelRecvQueue(error_code ec)
	{

		if (mSched->mPrint)
			std::cout << hexPtr(this) << "  cancelRecvQueue(" << ec.message() << ") " << std::endl;
		assert(ec && ec != code::suspend);

		while (mRecvBuffers.size())
		{
			auto iter = mRecvBuffers.begin();
			auto& op = *iter;

			auto res = std::get<1>(op.second);
			if (res)
			{
				res->setError(ec, nullptr);
				mSched->resume(res);
				//mReady.push_back(res);
			}

			mRecvBuffers.erase(iter);
		}

		mActiveRecv = false;
		mHaveHeader = false;

		if (mAsync)
			mAsync->cancel();
		else
			mSync->cancel();

		mSched->run();
	}

	void SockScheduler::cancelSendQueue(error_code ec)
	{
		if (mSched->mPrint)
			std::cout << hexPtr(this) << "  cancelSendQueue(" << ec.message() << ") " << std::endl;
		assert(ec && ec != code::suspend);

		while (mSendBuffers.size())
		{
			auto res = std::get<2>(mSendBuffers.front());
			if (res)
			{
				res->setError(ec, nullptr);
				mSched->resume(res);
				//mReady.push_back(res);
			}

			mSendBuffers.pop_front();
		}

		mActiveSend = false;

		if (mAsync)
			mAsync->cancel();
		else
			mSync->cancel();

		mSched->run();
	}


}
