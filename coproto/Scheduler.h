#pragma once
#include "Defines.h"
#include "error_code.h"
#include <list>
#include <functional>
#include <array>

#include "Queue.h"

namespace coproto
{

	struct Continutation
	{
		using Func = std::function<void(error_code ec, u64 byte_transfered)>;
		Func mFn;
		Continutation() = default;
		Continutation(const Continutation&) = delete;
		Continutation(Continutation&&) = default;

		template<typename Fn>
			requires std::is_constructible_v<Func, Fn&&>
		Continutation(Fn&& fn)
			: mFn(std::forward<Fn>(fn))
		{}

		Continutation& operator=(Continutation&&) = default;

		explicit operator bool()
		{
			return static_cast<bool>(mFn);
		}

		void operator()(error_code ec, u64 byte_transfered) const
		{
			mFn(ec, byte_transfered);
		}
	};

	extern std::atomic<u64> gProtoIdx;

	class Resumable;
	struct SendBuffer;
	struct RecvBuffer;

	struct AsyncSocket
	{
		virtual void recv(span<u8> data, Continutation&& cont) = 0;
		virtual void send(span<u8> data, Continutation&& cont) = 0;
	};

	struct Socket
	{
		virtual error_code recv(span<u8> data) = 0;
		virtual error_code send(span<u8> data) = 0;
	};

	class Executor
	{
	public:
		virtual void dispatch(std::function<void()>&& fn) = 0;
	};

	class ThreadExecutor : public Executor
	{
	public:
		BlockingQueue<std::function<void()>> mQueue;

		void dispatch(std::function<void()>&& fn) override
		{
			mQueue.push(std::move(fn));
		}

		void run()
		{
			while (true)
			{
				auto fn = mQueue.pop();

				if (fn)
					fn();
				else
					return;
			}
		}
	};



	class Scheduler
	{
	public:
		std::list<Resumable*> mReady;

		std::unordered_map<u32, std::tuple<RecvBuffer*, Resumable*>> mRecvBuffers;

		std::list<std::tuple<SendBuffer, u32, Resumable*>> mSendBuffers;

		std::vector<Resumable*> mStack;
		
		Socket* mSock = nullptr;
		AsyncSocket* mASock = nullptr;
		Executor* mExecutor = nullptr;

		std::function<void(error_code)> mCont;

		error_code resume(Resumable* proto);

		u64 mRoundIdx = 0;
		bool mPrint = false, mLogging = false;
		bool mSuspend;


		void run();
		bool done();


		inline void dispatch(std::function<void()>&& fn)
		{
			mExecutor->dispatch(std::move(fn));
		}
		


		u32 mNextSlot = 1;
		bool mHaveHeader = false, mActiveRecv = false, mActiveSend = false;

		void initAsyncRecv();
		void asyncRecvHeader();
		void asyncRecvBody();


		void initAsyncSend();

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

		void recv(RecvBuffer* data, u32 slot, Resumable* res);
		void send_(SendBuffer&& op, u32 slot, Resumable* res);


		void cancelSendQueue(error_code ec);

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
