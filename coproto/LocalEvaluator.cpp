#include "coproto/LocalEvaluator.h"
#include "coproto/Buffers.h"

namespace coproto
{



	error_code LocalEvaluator::InterlaceSock::recv(span<u8> data)
	{
		if (mCanceled)
			return code::ioError;

		assert(mEval);
		error_code ec = mEval->getError();
		if (ec)
			return ec;

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

	error_code LocalEvaluator::InterlaceSock::send(span<u8> data)
	{

		if (mCanceled)
			return code::ioError;

		assert(mEval);
		error_code ec = mEval->getError();
		if (ec)
			return ec;

		mOutbound.emplace_back(data.begin(), data.end());
		return {};
	}






	error_code LocalEvaluator::BlockingSock::recv(span<u8> data)
	{

		if (mCanceled)
			return code::ioError;

		if (mEval)
		{
			error_code ec = mEval->getError();
			if (ec)
				return ec;
		}
		const auto vec = mInbound.pop();

		if (vec.size() == 0)
		{
			cancel();
			return code::ioError;
		}

		assert(vec.size() == data.size());

		std::copy(vec.begin(), vec.end(), data.begin());

		return {};
	}

	error_code LocalEvaluator::BlockingSock::send(span<u8> data)
	{

		if (mCanceled)
			return code::ioError;

		if (mEval)
		{
			error_code ec = mEval->getError();
			if (ec)
				return ec;
		}

		mOther->mInbound.emplace(data.begin(), data.end());
		return {};
	}

	error_code LocalEvaluator::execute(internal::ProtoImpl& p0, internal::ProtoImpl& p1, Type type)
	{
#ifdef COPROTO_LOGGING
		if (p0.mName.size() == 0)
			p0.setName("main");
		if (p1.mName.size() == 0)
			p1.setName("main");
#endif

		mOpIdx = 0;
		//p0.mSlotIdx = 0;
		//p1.mSlotIdx = 0;
		//mScheds[0].scheduleReady(p0);
		//mScheds[1].scheduleReady(p1);
		//mScheds[0].mRoundIdx = 0;
		//mScheds[1].mRoundIdx = 0;

		bool print = false;

		if (type == Type::interlace)
		{

			//mScheds[0].mSock = &mSocks[0];
			//mScheds[1].mSock = &mSocks[1];
			mSocks[0].mEval = this;
			mSocks[1].mEval = this;



			while (!p0->done() || !p1->done())
			{

				if (!p0->done())
				{
					p0.evaluate(mSocks[0]);

					sendMsgs(0);
					if (print)
						std::cout << "-------------- p0 suspend --------------" << std::endl;
				}
				if (!p1->done())
				{
					p1.evaluate(mSocks[1]);

					sendMsgs(1);
					if (print)
						std::cout << "-------------- p1 suspend --------------" << std::endl;
				}
			}

		}
		else if (type == Type::blocking)
		{
			mBlkSocks[0].mEval = this;
			mBlkSocks[1].mEval = this;
			mBlkSocks[0].mOther = &mBlkSocks[1];
			mBlkSocks[1].mOther = &mBlkSocks[0];

			auto thrd = std::thread([&]() {
				p0.evaluate(mBlkSocks[0]);
				});

			p1.evaluate(mBlkSocks[1]);

			thrd.join();

		}
		else if (type == Type::async)
		{

			auto socketWorker = AsyncSock::Worker();
			mAsyncSock[0].mIdx = 0;
			mAsyncSock[1].mIdx = 1;
			mAsyncSock[0].mWorker = &socketWorker;
			mAsyncSock[1].mWorker = &socketWorker;
			socketWorker.mEval = this;
			socketWorker.mEval = this;

			ThreadExecutor ex;
			bool done = false;
			auto cc = [&](error_code ec) {
				if (done)
					ex.stop();
				else
					done = true;
			};

			p0.evaluate(mAsyncSock[0], cc, ex);
			p1.evaluate(mAsyncSock[1], cc, ex);

			ex.run();

		}
		else if (type == Type::asyncThread)
		{

			auto socketWorker = AsyncSock::Worker();
			mAsyncSock[0].mIdx = 0;
			mAsyncSock[1].mIdx = 1;
			mAsyncSock[0].mWorker = &socketWorker;
			mAsyncSock[1].mWorker = &socketWorker;
			socketWorker.mEval = this;
			socketWorker.mEval = this;
			socketWorker.startThread();

			ThreadExecutor ex;

			std::array<bool, 2> done = { false, false };
			
			auto cc = [&](error_code ec,u64 p) {
				if (done[p^1])
					ex.stop();
				else
					done[p] = true;
			};

			p0.evaluate(mAsyncSock[0], [&](error_code ec) { cc(ec, 0); }, ex);
			p1.evaluate(mAsyncSock[1], [&](error_code ec) { cc(ec, 1); }, ex);

			ex.run();
			socketWorker.join();

		}
		else
		{
			throw std::runtime_error(COPROTO_LOCATION);
		}

		if (p0->done() == false)
			throw std::runtime_error(COPROTO_LOCATION);
		if (p1->done() == false)
			throw std::runtime_error(COPROTO_LOCATION);

		auto e0 = p0->getErrorCode();
		if (e0)
			return e0;
		auto e1 = p1->getErrorCode();
		if (e1)
			return e1;
		return {};
	}

	void LocalEvaluator::AsyncSock::Worker::startThread()
	{
		mHasThread = true;

		mThread = std::thread([this]() {

			mStopped[0] = false;
			mStopped[1] = false;
			mCanceled = false;

			while (true)
			{
				auto op = mWorkQueue.pop();

				if (op.mType == Op::join)
					return;
				else
					process(std::move(op));
			}
			});
	}

	void LocalEvaluator::AsyncSock::Worker::completeOp(u64 i)
	{
		assert(mRecv[i]);
		assert(mSend[i ^ 1]);
		// there is an active send op.
		if (mRecv[i].mData.size() == mSend[i ^ 1].mData.size())
		{
			std::copy(mSend[i ^ 1].mData.begin(), mSend[i ^ 1].mData.end(), mRecv[i].mData.begin());

			auto cont0 = std::move(mRecv[i].mCont);
			auto cont1 = std::move(mSend[i ^ 1].mCont);
			mRecv[i] = Op{};
			mSend[i ^ 1] = Op{};


			cont0({}, mRecv[i].mData.size());
			cont1({}, mRecv[i].mData.size());

		}
		else
		{
			cancel();
		}

	}

	void LocalEvaluator::AsyncSock::Worker::cancel()
	{
		for (u64 i : {0, 1})
		{

			if (mRecv[i])
			{
				auto c = std::move(mRecv[i].mCont);
				mRecv[i] = {};
				c(code::ioError, 0);
			}

			if (mSend[i])
			{
				auto c = std::move(mSend[i].mCont);
				mSend[i] = {};
				c(code::ioError, 0);
			}
		}

		mCanceled = true;
		mStopped[0] = true;
		mStopped[1] = true;
	}

	void LocalEvaluator::AsyncSock::Worker::process(Op op)
	{
		assert(mEval);
		if (mEval->mOpIdx == mEval->mErrorIdx)
		{
			if (op.mCont)
				op.mCont(code::ioError, 0);
		}
		else if (mCanceled)
		{
			if (op.mCont)
				op.mCont(code::ioError, 0);
		}
		else
		{
			auto i = op.mIdx;
			if (op.mType == Op::send)
			{

				assert(!mSend[i]);


				//std::cout << ">>> s" << i << " send " << op.mData.size() << std::endl;
				mSend[i] = std::move(op);


				if (mRecv[i ^ 1])
				{
					completeOp(i ^ 1);
				}
			}
			else if (op.mType == Op::recv)
			{
				assert(!mRecv[i]);

				//std::cout << ">>> s" << i << " recv " << op.mData.size() << std::endl;
				mRecv[i] = std::move(op);

				if (mSend[i ^ 1])
					completeOp(i);

			}
			else if (op.mType == Op::cancel)
			{
				cancel();
			}
			else if (op.mType == Op::stop)
			{
				assert(!mRecv[i]);
				assert(!mSend[i]);

				mStopped[i] = true;
			}
			else
			{
				assert(0);
			}

		}
	}

}