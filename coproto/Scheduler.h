#pragma once
#include "Defines.h"
#include "error_code.h"
#include "Buffers.h"
#include <list>
#include "boost/container/small_vector.hpp"
#include <unordered_map>
namespace coproto
{
	//struct ProtoAwaiter;
	//struct EcProtoAwaiter;
	extern std::atomic<u64> gProtoIdx;

	class ProtoBase;
	class Scheduler
	{
	public:
		using SmallVec = boost::container::small_vector<ProtoBase*, 4>;
		std::list<ProtoBase*> mReady, mNext;

		std::unordered_map<ProtoBase*, SmallVec> mUpstream, mDwstream;

		bool mPrint = false;
		u64 mIdx = 0;
		std::vector<std::string> mLogs;
		void log(std::string l)
		{
			if (mPrint)
				std::cout << l << std::endl;
			mLogs.push_back(l);
		}

		bool mEndOfRoundFlag = false;

		void runOne();
		bool done();




		virtual error_code recv(Buffer& data) = 0;
		virtual error_code send(Buffer& data) = 0;

		void scheduleNext(ProtoBase& proto);
		void scheduleReady(ProtoBase& proto);

		void addDep(ProtoBase& downstream, ProtoBase& upstream);

		void fulfillDep(ProtoBase& upstream, error_code ec, std::exception_ptr ptr);

		void setEndOfRound();


		//std::unordered_map

		std::string getDot()const;
	};
}
