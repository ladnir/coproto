#include "Scheduler.h"
#include "Proto.h"
#include <sstream>

namespace coproto
{
	std::atomic<u64> gProtoIdx(0);
	void Scheduler::runOne()
	{
		ProtoBase* task = nullptr;
		while (mReady.size())
		{
			task = mReady.front();

			mReady.pop_front();

			//log("resume " + task->getName() + ";");
			auto ec = task->resume(*this);

		}
		std::swap(mReady, mNext);
	}

	bool Scheduler::done()
	{
		return mReady.size() == 0;
	}




	void Scheduler::scheduleNext(ProtoBase& proto)
	{

		auto name = proto.getName();
		log(name + "[shape=Mdiamond];");

		mNext.push_back(&proto);

	}

	void Scheduler::scheduleReady(ProtoBase& proto)
	{
		mReady.push_back(&proto);

	}

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

		// for each downstream proto
		for (auto d : dIter->second)
		{
			auto uIter = mUpstream.find(d);
			if (uIter == mUpstream.end()) {
				std::cout << "coproto internal error. " LOCATION << std::endl;
				throw RTE_LOC;
			}
			auto iter = std::find(uIter->second.begin(), uIter->second.end(), &upstream);
			if (iter == uIter->second.end()) {
				std::cout << "coproto internal error. " LOCATION << std::endl;
				throw RTE_LOC;
			}

			std::swap(*iter, uIter->second.back());
			uIter->second.pop_back();

			if(ec)
				uIter->first->setError(ec, ptr);

			log(upstream.getName() + " -> " + d->getName() + ";");

			if (uIter->second.size() == 0)
			{
				mUpstream.erase(uIter);
				scheduleReady(*d);
			}
		}

		mDwstream.erase(dIter);
	}

	void Scheduler::setEndOfRound()
	{
	}

	std::string Scheduler::getDot() const
	{
		std::stringstream ss;
		
		ss << "digraph G {\n   rankdir = TD;\n start -> ";

		for (auto l : mLogs)
			ss << l << std::endl;

		ss << "}\n";

		//std::cout << ss.str() << std::endl;

		return ss.str();
	}

}