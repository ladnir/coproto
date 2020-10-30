#include "Scheduler.h"
#include "Proto.h"
#include <sstream>

namespace coproto
{
	std::atomic<u64> gProtoIdx(0);

	void Scheduler::logEdge(ProtoBase& parent, ProtoBase& child, bool dashed)
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
	void Scheduler::logSuspend(ProtoBase& p)
	{
		if(mLogging)
			mLogs.emplace_back(Entry::Suspend, p.getName());

	}

	error_code Scheduler::resume(ProtoBase* proto)
	{
		bool eor = false;
		if (mStack.size())
		{

			auto iter = mEoRSet.find(mStack.back());
			if (iter != mEoRSet.end()) {
				eor = true;
				mEoRSet.insert(proto);
			}

			if (proto->mSlotIdx == ~0)
				proto->mSlotIdx = mStack.back()->mSlotIdx;
		}

		mStack.push_back(proto);

		if (mPrint)
			std::cout << "resume " << proto->getName() << (eor ? " eor" : "") << std::endl;
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
	void Scheduler::runRound()
	{

		ProtoBase* task = nullptr;
		mSuspend = false;



		while (!done() && mSuspend == false)
		{
			if (mReady.size() == 0)
			{

				assert(mHaveHeader == false);
				auto ec = recvHeader();
				if (!ec)
				{
					auto iter = mSlotWaiters.find(getHeaderSlot());
					if (iter != mSlotWaiters.end())
					{
						mReady.push_back(iter->second);
						mSlotWaiters.erase(iter);
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

			if (ec && ec != code::suspend)
			{
				break;
			}
		}

		if (mPrint)
			std::cout << " ------------ eor ------------- " << std::endl;

		mEoRSet.clear();
		++mRoundIdx;

	}

	bool Scheduler::done()
	{
		return mReady.size() == 0 && mSlotWaiters.size() == 0;
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
			assert(ec == code::suspend);
			return ec;
		}

		mHaveHeader = true;
		return ec;
	}

	error_code Scheduler::recv(IoProto& data)
	{
		error_code ec;

		//if (mEoRSet.find(mStack.back()) == mEoRSet.end())
		//{

		ec = recvHeader();

		if (!ec)
		{
			auto h = getHeaderSlot();
			if (data.getSlot() == h)
			{
				auto size = data.asSpan().size();
				if (size != getHeaderSize())
					ec = data.tryResize(getHeaderSize());

				if (ec)
					return ec;

				ec = mSock->recv(data.asSpan());
				assert(!ec);

				mHaveHeader = false;
			}
			else
			{
				ec = code::suspend;
				auto iter = mSlotWaiters.find(h);
				if (iter != mSlotWaiters.end())
				{
					auto proto = *iter;
					mReady.push_back(proto.second);
					mSlotWaiters.erase(iter);
				}
			}
		}


		if (ec == code::suspend)
		{
			if (mPrint)
				std::cout << " ~~ next " << data.getName() << " " << data.getSlot() << std::endl;

			auto iter = mSlotWaiters.find(data.getSlot());
			assert(iter == mSlotWaiters.end());
			mSlotWaiters.insert(std::make_pair(data.getSlot(), &data));
		}
		else
		{
			if (mPrint)
				std::cout << " ~~ recv " << data.getName() << " " << data.getSlot() << std::endl;
		}

		if (ec == code::suspend)
			logSuspend(data);

		return ec;
	}

	error_code Scheduler::send_(IoProto& data)
	{
		if (mPrint)
		{
			std::cout << " ~~ send " << data.getName() << " " << data.getSlot() << std::endl;
		}

		auto d = data.asSpan();
		if (d.size() == 0)
			return code::sendLengthZeroMsg;

		getHeaderSlot() = data.getSlot();
		getHeaderSize() = d.size();

		assert(getHeaderSlot() != ~0);

		auto ec = mSock->send(mHeader);

		assert(!ec);
		return mSock->send(d);
	}




	//void Scheduler::scheduleNext(ProtoBase& proto)
	//{


	//	mNext.push_back(&proto);

	//}

	void Scheduler::scheduleReady(ProtoBase& proto)
	{
		auto iter = std::find(mStack.begin(), mStack.end(), &proto);

		if (iter == mStack.end())
			mReady.push_back(&proto);

	}

	//error_code Scheduler::startSubproto(ProtoBase& parent, ProtoBase& sub)
	//{


	//}

	void Scheduler::addDep(ProtoBase& downstream, ProtoBase& upstream)
	{
		auto uIter = mUpstream.find(&downstream);
		if (uIter == mUpstream.end())
		{
			mUpstream.emplace(&downstream, SmallVec{ &upstream });
		}
		else
		{
			uIter->second.push_back(&upstream);
		}

		//std::cout << "add us " << hexPtr(&upstream) << " " <<upstream.getName() << std::endl;

		auto dIter = mDwstream.find(&upstream);
		if (dIter == mDwstream.end())
		{
			mDwstream.emplace(&upstream, SmallVec{ &downstream });
		}
		else
		{
			dIter->second.push_back(&downstream);
		}
	}

	void Scheduler::fulfillDep(ProtoBase& upstream, error_code ec, std::exception_ptr ptr)
	{
		assert(mUpstream.find(&upstream) == mUpstream.end() &&
			"A Proto was marked as done but it sill has dependencies");

		auto dIter = mDwstream.find(&upstream);
		if (dIter == mDwstream.end())
			return;

		bool isEoR = mEoRSet.find(&upstream) != mEoRSet.end();

		auto& downstreamProtos = dIter->second;

		// for each downstream proto
		for (auto d : downstreamProtos)
		{
			auto uIter = mUpstream.find(d);
			if (uIter == mUpstream.end()) {
				std::cout << "coproto internal error. " LOCATION << std::endl;
				throw RTE_LOC;
			}

			auto& deps = uIter->second;

			auto iter = std::find(deps.begin(), deps.end(), &upstream);
			if (iter == deps.end()) {
				std::cout << "coproto internal error. " LOCATION << std::endl;
				throw RTE_LOC;
			}

			std::swap(*iter, deps.back());
			deps.pop_back();

			if (ec)
				d->setError(ec, ptr);

			if (isEoR)
				mEoRSet.insert(d);

			logEdge(upstream, *d);
			//log(upstream.getName() + " -> " + d->getName() + ";");

			if (deps.size() == 0)
			{
				mUpstream.erase(uIter);
				scheduleReady(*d);
			}
			else
			{
				for (auto dd : deps)
				{
					std::cout << "// " << hexPtr(dd) << " " << dd->getName() << std::endl;
				}
			}
		}

		mDwstream.erase(dIter);
	}

	void Scheduler::setEndOfRound()
	{
		mEoRSet.insert(mStack.back());
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

}