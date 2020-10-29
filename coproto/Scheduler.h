#pragma once
#include "Defines.h"
#include "error_code.h"
#include "Buffers.h"
#include <list>
#include "boost/container/small_vector.hpp"
#include <unordered_map>
#include <unordered_set>
#include <functional>
namespace coproto
{

	struct Continutation
	{
		std::function<void(error_code ec, u64 byte_transfered)> mFn;
		Continutation() = default;
		Continutation(const Continutation&) = default;
		Continutation(Continutation&&) = default;



		void operator()(error_code ec, u64 byte_transfered) const
		{
			mFn(ec, byte_transfered);
		}
	};

	extern std::atomic<u64> gProtoIdx;

	class ProtoBase;
	class IoProto;

	struct AsyncSocket
	{
		virtual error_code recv(BufferInterface& data, Continutation&& cont) = 0;
		virtual error_code send(BufferInterface& data, Continutation&& cont) = 0;
	};

	struct Socket
	{
		virtual error_code recv(BufferInterface& data) = 0;
		virtual error_code send(BufferInterface& data) = 0;
	};

	class Scheduler
	{
	public:
		using SmallVec = boost::container::small_vector<ProtoBase*, 4>;
		std::list<ProtoBase*> mReady, mNext;
		std::unordered_map<ProtoBase*, SmallVec> mUpstream, mDwstream;


		std::unordered_set<ProtoBase*> mEoRSet;

		std::vector<ProtoBase*> mStack;
		
		Socket* mSock;
		AsyncSocket* mASock;

		error_code resume(ProtoBase* proto);

		u64 mRoundIdx = 0;
		bool mPrint = false;
		bool mSuspend;
		//u64 mIsSuspended = false;

		struct Entry
		{
			enum Type
			{
				Edge,
				Subgraph,
				Suspend
			};
			Type mType;

			Entry(Type t, std::string&& p, std::string&& c)
				:mParent(std::move(p))
				,mChild(std::move(c))
			{
				mType = t;
			}

			Entry(Type t, std::string&& proto)
				:mParent(std::move(proto))
			{
				mType = t;
			}
			std::string mParent, mChild;
			bool mBidirectional = false;
			bool mDashed = false;


			std::string str();

		};
		struct pair_hash
		{
			template <class T1, class T2>
			std::size_t operator() (const std::pair<T1, T2>& pair) const
			{
				return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
			}
		};

		std::unordered_map<std::pair<std::string, std::string>, u64, pair_hash> mEdgeSet;
		std::vector<Entry> mLogs;
		//void log(std::string l)
		//{
		//	if (mPrint)
		//		std::cout << l << std::endl;
		//	mLogs.push_back(l);
		//}

		void logEdge(ProtoBase& parent, ProtoBase& child, bool dashed = false);
		void logEdge(std::string p, std::string c, bool dashed = false);

		void logProto(std::string name, u64 protoIdx, std::string label, u64 resumeCount);

		void logSuspend(ProtoBase& p);

		//void runOne();
		void runRound();
		bool done();

		//template<typename RecvProto>
		error_code recv(IoProto& data);
		error_code send_(IoProto& data);


		//void scheduleNext(ProtoBase& proto);
		void scheduleReady(ProtoBase& proto);

		//error_code startSubproto(ProtoBase& downstream, ProtoBase& upstream);

		void addDep(ProtoBase& downstream, ProtoBase& upstream);

		void fulfillDep(ProtoBase& upstream, error_code ec, std::exception_ptr ptr);

		void setEndOfRound();


		//std::unordered_map
		u64 numRounds()
		{
			return mRoundIdx;
		}
		std::string getDot()const;
	};
}
