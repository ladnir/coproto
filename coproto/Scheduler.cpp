#include "coproto/Scheduler.h"
#include "coproto/Proto.h"
#include <sstream>
#include "coproto/Buffers.h"

namespace coproto
{
	//std::atomic<u64> gProtoIdx(0);
#ifdef COPROTO_LOGGING
	u64  gProtoIdx(0);
#endif

	error_code Scheduler::resume(Resumable* proto)
	{
		//bool eor = false;
		if (mStack.size())
		{

			if (proto->mSlotIdx == ~u32(0))
				proto->mSlotIdx = mStack.back()->mSlotIdx;
		}

		mStack.push_back(proto);

#ifdef COPROTO_LOGGING
		if (mPrint)
			std::cout << "resume " << proto->getName() << std::endl;
#endif

		assert(proto->mUpstream.size() == 0);

		auto ec = proto->resume_(*this);
		mStack.pop_back();
		return ec;
	}

	void Scheduler::run()
	{

		Resumable* task = nullptr;
		if (mRunning)
			return;

		mRunning = true;
		//mSuspend = false;


		while (!done())
		{
			if (mReady.size() == 0)
			{
				if (mASock)
				{
					break;
				}
				else
				{
					if(mRecvBuffers.size())
						initRecv();
					if (mSendBuffers.size())
						initSend();
					if (mReady.size() == 0)
						break;
				}
			}

			task = mReady.front();
			mReady.pop_front();
			resume(task);
		}

		if (mPrint)
			std::cout << " ------------" << hexPtr(this) << " " << std::this_thread::get_id() << " out of work" << " ------------" << std::endl;

		//mEoRSet.clear();
		++mRoundIdx;

		if (done())
		{
			if (mCont)
			{
				mCont({});
				mCont = {};
			}

			if (mExecutor)
			{
				mExecutor = nullptr;
			}
		}

		mRunning = false;

	}

	bool Scheduler::done()
	{
		return mReady.size() == 0 &&
			mRecvBuffers.size() == 0 &&
			mSendBuffers.size() == 0;
	}

	void Scheduler::initAsyncRecv()
	{
		assert(mActiveRecv == false && mRecvBuffers.size() > 0);
		mActiveRecv = true;

		if (mHaveHeader)
		{
			if (mPrint)
				std::cout << hexPtr(this) << " recved header and resuming..." << std::endl;

			asyncRecvBody();
		}
		else
		{
			asyncRecvHeader();
		}
	}

	void Scheduler::initRecv()
	{
		error_code ec;
		ec = recvHeader();

		if (!ec)
		{
			auto h = getRecvHeaderSlot();
			auto iter = mRecvBuffers.find(h);
			
			if(iter != mRecvBuffers.end())
			{
				auto& data = std::get<0>(iter->second);

				auto d = data->asSpan(getRecvHeaderSize());
				if (d.size() != getRecvHeaderSize())
					ec = code::badBufferSize;
				else
				{
					try
					{
						ec = mSock->recv(d);
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
					resume(res);
				}
			}
		}
		else  if (ec != code::suspend)
		{
			cancelRecvQueue(ec);
		}
	}

	void Scheduler::asyncRecvHeader()
	{
		assert(mHaveHeader == false);

		//if (mPrint)
		//	std::cout << hexPtr(this) << " recving header "  << std::endl;

		mASock->recv(mRecvHeader, [this](error_code ec, u64 bt) {
			dispatch([this, ec, bt]() {

				mHaveHeader = true;
				if (ec)
					cancelRecvQueue(ec);
				else
					asyncRecvBody();

				});
			});

	}

	void Scheduler::asyncRecvBody()
	{

		dispatch([this]() {
			assert(mHaveHeader);

			auto h = getRecvHeaderSlot();
			auto iter = mRecvBuffers.find(h);
			if (iter != mRecvBuffers.end())
			{
				auto data = std::get<0>(iter->second)->asSpan(getRecvHeaderSize());

				if (data.size() == getRecvHeaderSize())
				{
					mASock->recv(data, [this](error_code ec, u64 bt) {
						dispatch([this, ec, bt]() {


							auto h = getRecvHeaderSlot();
							auto iter = mRecvBuffers.find(h);
							auto proto = std::get<1>(iter->second);

							//if (mPrint)
							//	std::cout << hexPtr(this) << " recved body ~~ " << proto->getName() << std::endl;

							if (!ec)
							{
								mRecvBuffers.erase(iter);

								mReady.push_back(proto);

								mHaveHeader = false;
								mActiveRecv = false;

								if (mRecvBuffers.size())
								{
									initAsyncRecv();
								}

								run();
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
					mReady.push_back(proto);
					//mHaveHeader = false;

					cancelRecvQueue(code::ioError);
				}
			}
			else
			{
				mActiveRecv = false;
				if (mPrint)
					std::cout << hexPtr(this) << " recved header but suspend... " << h << std::endl;
			}

			});


	}


	error_code Scheduler::recvHeader()
	{
		if (mHaveHeader)
			return {};

		try
		{
			auto ec = mSock->recv(mRecvHeader);
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

	void Scheduler::recv(RecvBuffer* data, u32 slot, Resumable* res)
	{
		//if(mPrint)
		//	std::cout << " @@@@@@@@@ " << hexPtr(this) << " recv "<< res->getName() << " on " << slot << std::endl;

		assert(mRecvBuffers.find(slot) == mRecvBuffers.end());
		mRecvBuffers.insert(std::make_pair(slot, std::make_tuple(data, res)));

		if (mASock)
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

	void Scheduler::send_(SendBuffer&& op, u32 slot, Resumable* res)
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
		assert(slot != ~u32(0));
		mSendBuffers.emplace_back(std::move(op), slot, res);

		if (mSock)
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

	void Scheduler::initAsyncSend()
	{
		assert(mActiveSend == false && mSendBuffers.size());
		mActiveSend = true;

		auto data = std::get<0>(mSendBuffers.front()).asSpan();

		assert(data.size() != 0);
		assert(data.size() < std::numeric_limits<u32>::max());

		getSendHeaderSlot() = std::get<1>(mSendBuffers.front());
		getSendHeaderSize() = static_cast<u32>(data.size());

		if (mPrint)
			std::cout << "******** " << hexPtr(this) << " init send on " << getSendHeaderSlot() << std::endl;

		//if (mPrint)
		//	std::cout << hexPtr(this) << " " << std::this_thread::get_id() << " sending header" << std::endl;
		mASock->send(mSendHeader, [this, data](error_code ec, u64 bt) {


			//dispatch([this]() {
			//	if (mPrint)
			//		std::cout << hexPtr(this) << " " << std::this_thread::get_id() << " sent   header" << std::endl;
			//	});

			if (!ec)
			{
				mASock->send(data, [this](error_code ec, u64 bt) {

					dispatch([this, ec, bt]() {

						auto res = std::get<2>(mSendBuffers.front());
						//if (mPrint && res)
						//	std::cout << hexPtr(this) << " sent   body ~~ " << res->getName() << std::endl;

						if (!ec)
						{
							if (res)
							{
								mReady.push_back(res);
							}
							mSendBuffers.pop_front();
							mActiveSend = false;

							if (mSendBuffers.size())
								initAsyncSend();

							run();
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
				dispatch([this, ec, bt]() {
					cancelSendQueue(ec);
					});
			}
			});
	}

	void coproto::Scheduler::initSend()
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
					ec = mSock->send(mSendHeader);
				}catch(...)
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
				ec = mSock->send(data);
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
					resume(res);

				mSendBuffers.pop_front();
			}
		}
	}


	void coproto::Scheduler::cancelRecvQueue(error_code ec)
	{

		if (mPrint)
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
				mReady.push_back(res);
			}

			mRecvBuffers.erase(iter);
		}

		mActiveRecv = false;
		mHaveHeader = false;

		if (mASock)
			mASock->cancel();
		else
			mSock->cancel();

		run();
	}

	void Scheduler::cancelSendQueue(error_code ec)
	{
		if (mPrint)
			std::cout << hexPtr(this) << "  cancelSendQueue(" << ec.message() << ") " << std::endl;
		assert(ec && ec != code::suspend);

		while (mSendBuffers.size())
		{
			auto res = std::get<2>(mSendBuffers.front());
			if (res)
			{
				res->setError(ec, nullptr);
				mReady.push_back(res);
			}

			mSendBuffers.pop_front();
		}

		mActiveSend = false;

		if (mASock)
			mASock->cancel();
		else
			mSock->cancel();

		run();
	}

	void Scheduler::scheduleReady(Resumable& proto)
	{
		auto iter = std::find(mStack.begin(), mStack.end(), &proto);

		if (iter == mStack.end())
			mReady.push_back(&proto);

	}


	void Scheduler::addDep(Resumable& downstream, Resumable& upstream)
	{
		downstream.mUpstream.push_back(&upstream);
		upstream.mDwstream.push_back(&downstream);
	}

	void Scheduler::fulfillDep(Resumable& upstream, error_code ec, std::exception_ptr ptr)
	{
		assert(!ec || ec != code::suspend);
		assert(upstream.mUpstream.size() == 0);

		auto& downstreamProtos = upstream.mDwstream;

		// for each downstream proto
		for (auto d : downstreamProtos)
		{

			auto& deps = d->mUpstream;

			auto iter = std::find(deps.begin(), deps.end(), &upstream);
			assert(iter != deps.end() && "coproto internal error");

			std::swap(*iter, deps.back());
			deps.pop_back();

			if (ec)
				d->setError(ec, ptr);

#ifdef COPROTO_LOGGING
			logEdge(upstream, *d);
#endif
			if (deps.size() == 0)
			{
				scheduleReady(*d);
		}
	}
	}

	void Scheduler::clear()
	{
		assert(done());

		mReady.clear();
		mRecvBuffers.clear();
		mSendBuffers.clear();
		mStack.clear();
		mSock = nullptr;
		mASock = nullptr;
		mExecutor = nullptr;

		mCont = {};

		mRoundIdx = 0;
		mRunning = false;
		mSentHeader = false;
		mNextSlot = 1;
		mHaveHeader = false;
		mActiveRecv = false;
		mActiveSend = false;
	}

#ifdef COPROTO_LOGGING
	void Scheduler::logEdge(Resumable& parent, Resumable& child, bool dashed)
	{
		if (mLogging)
			logEdge(parent.getName(), child.getName());
	}
	void Scheduler::logEdge(std::string pp, std::string cc, bool dashed)
	{
		if (mLogging)
		{

			auto cp = std::make_pair(std::move(cc), std::move(pp));
			auto iter = mEdgeSet.find(cp);

			if (iter != mEdgeSet.end())
			{
				mLogs[iter->second].mBidirectional = true;
				mEdgeSet.erase(iter);
			}
			else
			{

				std::swap(cp.first, cp.second);
				mEdgeSet.insert(std::make_pair(cp, mLogs.size()));
				auto& p = cp.first;
				auto& c = cp.second;
				mLogs.emplace_back(Entry::Edge, std::move(p), std::move(c));
				mLogs.back().mDashed = dashed;
			}
		}
	}
	void Scheduler::logProto(std::string name, u64 protoIdx, std::string label, u64 resumeCount)
	{
		if (mLogging)
		{

			if (label.size() == 0)
				label = name;

			std::stringstream ss;
			ss << "    subgraph cluster_" << std::to_string(protoIdx) << " { \n"
				<< "        style = filled;\n"
				<< "        color = lightgrey;\n"
				<< "        node[style = filled, color = white];\n"
				<< "        label = \"" << label << "\";\n"
				<< "        " << name << "_" << 0;

			for (u64 i = 1; i <= resumeCount; ++i)
				ss << " -> " << name << "_" << i;
			if (resumeCount > 1)
				ss << "[style=invis]";
			ss << ";\n    }";

			mLogs.emplace_back(Entry::Subgraph, ss.str());

		}
	}
	void Scheduler::logSuspend(Resumable& p)
	{
		if (mLogging)
			mLogs.emplace_back(Entry::Suspend, p.getName());

	}

	std::string Scheduler::getDot() const
	{
		std::stringstream ss;

		ss << "digraph G {\n   rankdir = TD;\n start [style=filled;color=green;];\n start -> ";

		for (auto l : mLogs)
		{

			ss << l.str() << std::endl;
		}

		ss << "}\n";

		//std::cout << ss.str() << std::endl;

		return ss.str();
	}

	std::string Scheduler::Entry::str()
	{
		std::stringstream ss;

		if (mType == Type::Edge)
		{
			ss << mParent << " -> " << mChild << "[";

			if (mBidirectional)
			{
				ss << " dir=\"both\";";
			}
			if (mDashed)
			{
				ss << " style=dashed;";

			}

			ss << "];\n";

		}
		else if (mType == Type::Subgraph)
		{
			ss << mParent;
		}
		else if (mType == Type::Suspend)
		{
			ss << mParent << "[shape=Mdiamond]";
		}
		else
		{
			assert(0);
		}

		return ss.str();
	}

#endif
}