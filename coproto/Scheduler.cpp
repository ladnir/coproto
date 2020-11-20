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
	std::string hexPtr(void* p);

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
				for (auto s : mSockets)
				{
					if (s->mSync)
					{
						if (s->mRecvBuffers.size())
							s->initRecv();
						if (s->mSendBuffers.size())
							s->initSend();
					}
				}

				if (mReady.size() == 0)
					break;
			}

			task = mReady.front();
			mReady.pop_front();
			resume(task);
		}

		if (mPrint)
			std::cout << " ------------" << hexPtr(this) << " " << std::this_thread::get_id() << " out of work" << " ------------" << std::endl;

		++mRoundIdx;

		if (done())
		{
			if (mCont)
			{
				mCont({});
				mCont = {};
			}

			mWork = {};

			if (mExecutor)
			{
				mExecutor = nullptr;
			}
		}

		mRunning = false;

	}

	bool Scheduler::done()
	{
		if (mReady.size())
			return false;

		for (auto s : mSockets)
		{
			if (s->mRecvBuffers.size() ||
				s->mSendBuffers.size())
				return false;
		}

		return true;
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
		mSockets.clear();
		mStack.clear();
		mExecutor = nullptr;

		mCont = {};

		mRoundIdx = 0;
		mRunning = false;
		mNextSlot = 1;
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