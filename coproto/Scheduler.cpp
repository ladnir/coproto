#include "Scheduler.h"
#include "Proto.h"
#include <sstream>
#include "Buffers.h"

namespace coproto
{
	std::atomic<u64> gProtoIdx(0);


	error_code Scheduler::resume(Resumable* proto)
	{
		//bool eor = false;
		if (mStack.size())
		{

			if (proto->mSlotIdx == ~0)
				proto->mSlotIdx = mStack.back()->mSlotIdx;
		}

		mStack.push_back(proto);

#ifdef COPROTO_LOGGING
		if (mPrint)
			std::cout << "resume " << proto->getName() << std::endl;
#endif
		auto ec = proto->resume_(*this);
		mStack.pop_back();
		return ec;
	}
	//void Scheduler::runOne()
	//{
	//	auto task = mReady.front();
	//	resume(task);
	//	mReady.pop_front();
	//}
	void Scheduler::run()
	{

		Resumable* task = nullptr;
		mSuspend = false;



		while (!done() && mSuspend == false)
		{
			if (mReady.size() == 0)
			{

				assert(mHaveHeader == false);
				auto ec = recvHeader();
				if (!ec)
				{
					auto iter = mRecvers.find(getHeaderSlot());
					if (iter != mRecvers.end())
					{
						mReady.push_back(iter->second);
						mRecvers.erase(iter);
					}
				}
				else
				{

					break;
				}
			}

			task = mReady.front();
			mReady.pop_front();
			auto ec = resume(task);

			//if (ec && ec != code::suspend)
			//{
			//	break;
			//}
		}

		if (mPrint)
			std::cout << " ------------ eor ------------- " << std::endl;

		//mEoRSet.clear();
		++mRoundIdx;

		if (done() && mCont)
		{
			mCont({});
		}

	}

	bool Scheduler::done()
	{
		return mReady.size() == 0 && mRecvers.size() == 0;
	}

	void Scheduler::initAsyncRecv()
	{
		assert(mActiveRecv == false && mRecvers.size() > 0);
		mActiveRecv = true;

		if (mHaveHeader)
		{
			asyncRecvBody();
		}
		else
		{
			asyncRecvHeader();
		}
	}

	void Scheduler::asyncRecvHeader()
	{
		assert(mHaveHeader == false);

		mASock->recv(mHeader, [this](error_code ec, u64 bt) {
			if (ec)
			{
				assert(bt == mHeader.size());
				assert(0);
			}

			mHaveHeader = true;

			asyncRecvBody();
			});

	}

	void Scheduler::asyncRecvBody()
	{
		assert(mHaveHeader);

		dispatch([this]() {

			auto h = getHeaderSlot();
			auto iter = mRecvers.find(h);
			if (iter != mRecvers.end())
			{
				auto& proto = *iter->second;
				auto data = proto.asSpan(getHeaderSize());

				if (data.size() != getHeaderSize())
				{
					assert(0);
				}

				mASock->recv(data, [this, data](error_code ec, u64 bt) {

					assert(!ec && bt == data.size());

					dispatch([this]() {

						auto h = getHeaderSlot();
						auto iter = mRecvers.find(h);
						auto& proto = *iter->second;
						mRecvers.erase(iter);

						mReady.push_back(&proto);

						mHaveHeader = false;
						mActiveRecv = false;

						if (mRecvers.size())
						{
							initAsyncRecv();
						}

						run();

						});
					});
			}
			});


	}


	error_code Scheduler::recvHeader()
	{
		if (mHaveHeader)
		{
			return {};
		}

		auto ec = mSock->recv(mHeader);
		if (ec)
		{
			mSuspend = true;
			return ec;
		}

		mHaveHeader = true;
		return ec;
	}

	error_code Scheduler::recv(RecvProto& data)
	{
		error_code ec;


		if (mASock)
		{

			mRecvers.insert(std::make_pair(data.getSlot(), &data));

			if (mActiveRecv == false)
			{
				initAsyncRecv();
			}
			ec = code::suspend;

		}
		else
		{

			ec = recvHeader();

			if (!ec)
			{
				auto h = getHeaderSlot();
				if (data.getSlot() == h)
				{
					auto d = data.asSpan(getHeaderSize());
					if (d.size() != getHeaderSize())
						return code::badBufferSize;

					ec = mSock->recv(d);
					assert(!ec);

					mHaveHeader = false;
				}
				else
				{
					ec = code::suspend;
					auto iter = mRecvers.find(h);
					if (iter != mRecvers.end())
					{
						auto proto = *iter;
						mReady.push_back(proto.second);
						mRecvers.erase(iter);
					}
				}
			}


			if (ec == code::suspend)
			{
#ifdef COPROTO_LOGGING
				if (mPrint)
					std::cout << " ~~ next " << data.getName() << " " << data.getSlot() << std::endl;
				logSuspend(data);
#endif
				auto iter = mRecvers.find(data.getSlot());
				assert(iter == mRecvers.end());
				mRecvers.insert(std::make_pair(data.getSlot(), &data));
			}
			else
			{
#ifdef COPROTO_LOGGING
				if (mPrint)
					std::cout << " ~~ recv " << data.getName() << " " << data.getSlot() << std::endl;
#endif
			}
		}

		return ec;
	}

	void Scheduler::send_(SendBuffer&& op, u64 slot,  Resumable* res)
	{
#ifdef COPROTO_LOGGING
		if (mPrint)
			std::cout << " ~~ send " << data.getName() << " " << data.getSlot() << std::endl;
#endif
		assert(slot != ~0);

		if(mSock)
		{
			auto data = op.asSpan();
			if (data.size() == 0)
				assert(0);
				//res->setError(code::sendLengthZeroMsg, nullptr);

			getHeaderSlot() = slot;
			getHeaderSize() = data.size();

			assert(getHeaderSlot() != ~0);

			auto ec = mSock->send(mHeader);
			assert(!ec && "Not impl");

			ec = mSock->send(data);
			assert(!ec && "Not impl");
			

			if (res)
			{
				res->setError(ec, nullptr);
				resume(res);
			}
		}
		else
		{
			mSendBuffers.emplace_back(std::move(op), slot, res);

			if (mActiveSend == false)
				initAsyncSend();
		}
	}

	void Scheduler::initAsyncSend()
	{
		assert(mActiveSend == false && mSendBuffers.size());
		mActiveSend = true;

		auto data = std::get<0>(mSendBuffers.front()).asSpan();

		getHeaderSlot() = std::get<1>(mSendBuffers.front());
		getHeaderSize() = data.size();


		mASock->send(mHeader, [this, data](error_code ec, u64 bt) {
			
			if (!ec)
			{
				mASock->send(data, [this](error_code ec, u64 bt) {
					if (!ec)
					{
						auto res = std::get<2>(mSendBuffers.front());
						if (res)
						{
							resume(res);
						}
						mSendBuffers.pop_front();
						mActiveSend = false;

						if(mSendBuffers.size())
							initAsyncSend();
					}
					else
					{
						cancelSendQueue(ec);
					}
				});
			}
			else
			{
				cancelSendQueue(ec);
			}
		});
	}


	void Scheduler::cancelSendQueue(error_code ec)
	{
		assert(ec && ec != code::suspend);

		while (mSendBuffers.size())
		{
			auto res = std::get<2>(mSendBuffers.front());
			if (res)
			{
				res->setError(ec, nullptr);
				resume(res);
			}

			mSendBuffers.pop_front();
		}
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
		//auto uIter = mUpstream.find(&downstream);

		//if (uIter == mUpstream.end())
		//{
		//	mUpstream.emplace(&downstream, SmallVec{ &upstream });
		//}
		//else
		//{
		//	uIter.value().push_back(&upstream);
		//}

		////std::cout << "add us " << hexPtr(&upstream) << " " <<upstream.getName() << std::endl;

		//auto dIter = mDwstream.find(&upstream);
		//if (dIter == mDwstream.end())
		//{
		//	mDwstream.emplace(&upstream, SmallVec{ &downstream });
		//}
		//else
		//{
		//	dIter.value().push_back(&downstream);
		//}
	}

	void Scheduler::fulfillDep(Resumable& upstream, error_code ec, std::exception_ptr ptr)
	{

		assert(upstream.mUpstream.size() == 0);
		//assert(mUpstream.find(&upstream) == mUpstream.end() &&
		//	"A Proto was marked as done but it sill has dependencies");

		//auto dIter = mDwstream.find(&upstream);
		//if (dIter == mDwstream.end())
		//	return;


		auto& downstreamProtos = upstream.mDwstream;

		// for each downstream proto
		for (auto d : downstreamProtos)
		{
			//auto uIter = mUpstream.find(d);
			//if (uIter == mUpstream.end()) {
			//	std::cout << "coproto internal error. " COPROTO_LOCATION << std::endl;
			//	throw RTE_LOC;
			//}

			auto& deps = d->mUpstream;

			auto iter = std::find(deps.begin(), deps.end(), &upstream);
			if (iter == deps.end()) {
				std::cout << "coproto internal error. " COPROTO_LOCATION << std::endl;
				throw std::runtime_error(COPROTO_LOCATION);
			}


			std::swap(*iter, deps.back());
			deps.pop_back();

			if (ec)
				d->setError(ec, ptr);

			//if (isEoR)
			//	mEoRSet.insert(d);

#ifdef COPROTO_LOGGING
			logEdge(upstream, *d);
#endif

			if (deps.size() == 0)
			{
				//mUpstream.erase(uIter);
				scheduleReady(*d);
			}

		}

		//mDwstream.erase(dIter);
	}

	//void Scheduler::setEndOfRound()
	//{
	//	mEoRSet.insert(mStack.back());
	//}


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