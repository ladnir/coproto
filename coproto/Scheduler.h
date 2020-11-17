#pragma once
#include "coproto/Defines.h"
#include "coproto/error_code.h"
#include "coproto/Resumable.h"
#include <list>
#include <functional>
#include <array>

#include "coproto/Queue.h"
#include <unordered_map>
#include <thread>

namespace coproto
{

	struct Continutation
	{
		using Func = std::function<void(error_code ec, u64 byte_transfered)>;
		Func mFn;
		Continutation() = default;
		Continutation(const Continutation&) = delete;
		Continutation(Continutation&&) = default;

		template<typename Fn, typename Enable_If = 
			enable_if_t<std::is_constructible<Func, Fn&&>::value>>
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


#ifdef COPROTO_LOGGING
	extern u64 gProtoIdx;
#endif

	struct AsyncSocket
	{
		virtual void recv(span<u8> data, Continutation&& cont) = 0;
		virtual void send(span<u8> data, Continutation&& cont) = 0;
		virtual void cancel() = 0;
	};

	struct Socket
	{
		virtual error_code recv(span<u8> data) = 0;
		virtual error_code send(span<u8> data) = 0;
		virtual void cancel() = 0;
	};


	class Work
	{
	public:

		struct Impl
		{
			virtual ~Impl() = default;
		};

		Work() = default;
		Work(Work&&) = default;

		Work(std::unique_ptr<Impl> i)
			:mBase(std::move(i))
		{}

		Work& operator=(Work&&) = default;

		std::unique_ptr<Impl> mBase;
	};

	class Executor
	{
	public:
		virtual ~Executor() = default;
		virtual void dispatch(std::function<void()>&& fn) = 0;

		virtual Work getWork() = 0;
	};

	class ThreadExecutor : public Executor
	{
	public:
		struct WorkImpl : public Work::Impl
		{
			ThreadExecutor* mEx;

			WorkImpl(ThreadExecutor* e)
				: mEx(e)
			{
				++mEx->mWork;
			}

			~WorkImpl()
			{
				auto p = mEx;
				mEx->mQueue.emplace([p]() {
					--p->mWork;
					});
			}
		};

		ThreadExecutor() = default;
		ThreadExecutor(ThreadExecutor&&) = delete;


		BlockingQueue<std::function<void()>> mQueue;
		std::thread::id mThreadId;
		bool mRunning = false;
		u64 mWork = 0;

		void dispatch(std::function<void()>&& fn) override
		{
			if (mThreadId == std::this_thread::get_id())
				fn();
			else
				mQueue.push(std::move(fn));
		}

		Work getWork()
		{
			return Work(make_unique<WorkImpl>(this));
		}

		//void stop()
		//{
		//	mQueue.emplace();
		//}

		void run()
		{
			mThreadId = std::this_thread::get_id();
			if (mRunning)
				throw std::runtime_error("a thread is already running");
			mRunning = true;

			while (true)
			{
				u64 size;
				auto fn = mQueue.popWithSize(size);

				if (fn)
					fn();

				if(mWork == 0 && size == 0)
				{
					mRunning = false;
					return;
				}
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
		Work mWork;

		std::function<void(error_code)> mCont;


		u64 mRoundIdx = 0;
		bool mPrint = false, mLogging = false;
		bool mRunning = false, mSentHeader = false;
		u32 mNextSlot = 1;
		bool mHaveHeader = false, mActiveRecv = false, mActiveSend = false;
		//bool mSuspend;


		void run();
		bool done();

		template<typename Fn>
		inline enable_if_t<std::is_constructible<std::function<void()>, Fn>::value>
			 dispatch(Fn&& fn)
		{
			if (mExecutor)
				mExecutor->dispatch(std::forward<Fn>(fn));
			else
				fn();
		}




		error_code resume(Resumable* proto);
		void initAsyncRecv();
		void initRecv();

		void asyncRecvHeader();
		void asyncRecvBody();


		void initAsyncSend();
		void initSend();

		u32& getSendHeaderSlot()
		{
			return ((u32*)mSendHeader.data())[1];
		}
		u32& getSendHeaderSize()
		{
			return ((u32*)mSendHeader.data())[0];
		}

		u32& getRecvHeaderSlot()
		{
			return ((u32*)mRecvHeader.data())[1];
		}
		u32& getRecvHeaderSize()
		{
			return ((u32*)mRecvHeader.data())[0];
		}

		std::array<u8, sizeof(u64)> mSendHeader, mRecvHeader;

		error_code recvHeader();

		void recv(RecvBuffer* data, u32 slot, Resumable* res);
		void send_(SendBuffer&& op, u32 slot, Resumable* res);

		void cancelRecvQueue(error_code ec);
		void cancelSendQueue(error_code ec);

		void scheduleReady(Resumable& proto);

		void addDep(Resumable& downstream, Resumable& upstream);

		void fulfillDep(Resumable& upstream, error_code ec, std::exception_ptr ptr);

		void clear();


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
