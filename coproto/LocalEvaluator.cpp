#include "coproto/LocalEvaluator.h"
#include "coproto/Buffers.h"
#include <cstring>
namespace coproto
{



	error_code LocalEvaluator::InterlaceSock::recv(span<u8> data)
	{
		if (mCanceled)
			return code::ioError;

		error_code ec;
		if (mEval)
		{
			ec = mEval->getError();
			if (ec)
				return ec;
		}


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

		error_code ec;
		if (mEval)
		{
			ec = mEval->getError();
			if (ec)
				return ec;
		}

		mOutbound.emplace_back(data.begin(), data.end());
		return {};
	}






	error_code LocalEvaluator::BlockingSock::recv(span<u8> data)
	{

		if (mCanceled)
			return code::ioError;

		error_code ec;
		if (mEval)
		{
			ec = mEval->getError();
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

		error_code ec;
		if (mEval)
		{
			ec = mEval->getError();
			if (ec)
				return ec;
		}

		mOther->mInbound.emplace(data.begin(), data.end());
		return {};
	}

	error_code LocalEvaluator::execute(internal::ProtoImpl& p0, internal::ProtoImpl& p1, Type type, bool print)
	{
#ifdef COPROTO_LOGGING
		if (p0.mData->mName.size() == 0)
			p0.mData->setName("main");
		if (p1.mData->mName.size() == 0)
			p1.mData->setName("main");
#endif

		mOpIdx = 0;
		//assert(p0.mData->done() == false);
		//assert(p1.mData->done() == false);

		if (type == Type::interlace)
		{


			while (!p0->done() || !p1->done())
			{

				if (!p0->done())
				{
					auto ec = p0.evaluate(print);

					sendMsgs(0, ec);

					if (print)
						std::cout << "-------------- p0 suspend --------------" << std::endl;
				}
				if (!p1->done())
				{
					auto ec = p1.evaluate(print);

					sendMsgs(1, ec);
					if (print)
						std::cout << "-------------- p1 suspend --------------" << std::endl;
				}
			}

		}
		else if (type == Type::blocking)
		{
			auto thrd = std::thread([&]() {
				p0.evaluate(print);
				});

			p1.evaluate(print);

			thrd.join();

		}
		else if (type == Type::async)
		{

			mSocketWorker.clear();

			ThreadExecutor ex;
			auto cc = [&](error_code ec) {
			};

			p0.evaluate(cc, ex, print);
			p1.evaluate(cc, ex, print);

			ex.run();

		}
		else if (type == Type::asyncThread)
		{
			mSocketWorker.clear();
			mSocketWorker.startThread();

			ThreadExecutor ex;

			auto cc = [&](error_code ec) {
			};

			p0.evaluate(cc, ex, print);
			p1.evaluate(cc, ex, print);
									   
			ex.run();
			mSocketWorker.join();

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
		auto e1 = p1->getErrorCode();

		if (e0 && e1)
		{
			if (e0 == code::ioError)
				return e1;
			else
				return e0;
		}

		if (e0)
			return e0;
		if (e1)
			return e1;
		return {};
	}

	void LocalEvaluator::AsyncSock::Worker::startThread()
	{
		if (mHasThread == false)
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

		error_code ec;
		if (mEval && mEval->mOpIdx == mEval->mErrorIdx)
		{
			ec = code::ioError;
		}


		if (ec && op.mCont)
			op.mCont(ec, 0);
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