#pragma once
#include "Defines.h"
#include "error_code.h"
#include "Buffers.h"
#include <list>
//#include "boost/container/small_vector.hpp"
#include <functional>
#include <array>
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

	class Resumable;
	class IoProto;

	struct AsyncSocket
	{
		virtual error_code recv(BufferInterface& data, Continutation&& cont) = 0;
		virtual error_code send(BufferInterface& data, Continutation&& cont) = 0;
	};

	struct Socket
	{
		virtual error_code recv(span<u8> data) = 0;
		virtual error_code send(span<u8> data) = 0;
	};

	class Scheduler
	{
	public:
		//using SmallVec = boost::container::small_vector<Resumable*, 4>;
		std::list<Resumable*> mReady;

		//std::unordered_map<Resumable*, SmallVec> mUpstream, mDwstream;
		//tsl::robin_map<Resumable*, SmallVec> mUpstream, mDwstream;
		std::unordered_map<u32, Resumable*> mSlotWaiters;

		std::vector<Resumable*> mStack;
		
		Socket* mSock;
		AsyncSocket* mASock;

		error_code resume(Resumable* proto);

		u64 mRoundIdx = 0;
		bool mPrint = false, mLogging = false;
		bool mSuspend;


		void runRound();
		bool done();

		
		u32 mNextSlot = 1;
		bool mHaveHeader = false;

		u32& getHeaderSlot()
		{
			return ((u32*)mHeader.data())[1];
		}
		u32& getHeaderSize()
		{
			return ((u32*)mHeader.data())[0];
		}

		std::array<u8, sizeof(u64)> mHeader;

		error_code recvHeader();

		error_code recv(IoProto& data);
		error_code send_(IoProto& data);

		void scheduleReady(Resumable& proto);

		void addDep(Resumable& downstream, Resumable& upstream);

		void fulfillDep(Resumable& upstream, error_code ec, std::exception_ptr ptr);

		//void setEndOfRound();


		u64 numRounds()
		{
			return mRoundIdx;
		}


#ifdef COPROTO_LOGGING
		void logEdge(Resumable& parent, Resumable& child, bool dashed = false);
		void logEdge(std::string p, std::string c, bool dashed = false);

		void logProto(std::string name, u64 protoIdx, std::string label, u64 resumeCount);

		void logSuspend(Resumable& p);
		std::string getDot()const;

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
				, mChild(std::move(c))
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
#endif
	};
}
