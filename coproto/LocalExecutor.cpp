#include "LocalExecutor.h"


namespace coproto
{



	error_code LocalExecutor::InterlaceSock::recv(span<u8> data)
	{
		error_code ec;
		if (mInbound.size())
		{
			auto& front = mInbound.front();
			assert(data.size() == front.size());

			//if (buff.asSpan().size() != front.size())
			//	ec = buff.tryResize(front.size());

			//if (!ec)
			//{
			std::memcpy(data.data(), front.data(), data.size());
			mInbound.pop_front();
			//}
		}
		else
		{
			ec = code::suspend;
		}

		return ec;
	}

	error_code LocalExecutor::InterlaceSock::send(span<u8> data)
	{

		//auto data = buff.asSpan();
		//if (data.size() == 0)
		//	return code::sendLengthZeroMsg;

		mOutbound.emplace_back(data.begin(), data.end());
		return {};
	}






	error_code LocalExecutor::BlockingSock::recv(span<u8> data)
	{
		const auto vec = mInbound.pop();

		if (vec.size() == 0)
		{
			return code::ioError;
		}

		assert(vec.size() == data.size());

		std::copy(vec.begin(), vec.end(), data.begin());

		return {};
	}

	error_code LocalExecutor::BlockingSock::send(span<u8> data)
	{
		mOther->mInbound.emplace(data.begin(), data.end());
		return {};
	}

	error_code LocalExecutor::execute(Resumable& p0, Resumable& p1, Type type)
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
		//mScheds[0].mPrint = true;
		//mScheds[1].mPrint = true;

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
		else if (type == Type::async)
		{
			ThreadExecutor ex;
			mScheds[0].mASock = &mAsyncSock[0];
			mScheds[1].mASock = &mAsyncSock[1];

			mScheds[0].mExecutor = &ex;
			mScheds[1].mExecutor = &ex;

			mAsyncSock[0].mOther = &mAsyncSock[1];
			mAsyncSock[1].mOther = &mAsyncSock[0];

			mAsyncSock[0].startWorker();
			mAsyncSock[1].startWorker();

			mScheds[0].mPrint = true;
			mScheds[1].mPrint = true;

			ex.dispatch([&]() {
				mScheds[0].run();
				});
			ex.dispatch([&]() {
				mScheds[1].run();
				});

			mScheds[0].mCont = [&](error_code ec) {
				mAsyncSock[0].mWorkQueue.emplace(AsyncSock::Op::stop);
			};
			mScheds[1].mCont = [&](error_code ec) {
				mAsyncSock[1].mWorkQueue.emplace(AsyncSock::Op::stop);
			};


			std::cout << "executor " << std::this_thread::get_id() << std::endl;
			ex.run();


			mAsyncSock[0].mWorker.join();
			mAsyncSock[1].mWorker.join();

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

}