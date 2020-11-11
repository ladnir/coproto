#pragma once
#include "coproto/Defines.h"
#include "coproto/TypeTraits.h"
#include <cassert>
#include "coproto/error_code.h"
#include <iostream>
#include <memory>
#include "coproto/InlinePoly.h"
#include "coproto/Proto.h"

namespace coproto
{
	std::string hexPtr(void* p);



	struct SendProto : public Resumable
	{

		virtual SendBuffer getBuffer() = 0;
	};

	template<typename Container>
	class MoveRecvProto : public Resumable, public RecvBuffer
	{
	public:
		Container mContainer;
		error_code mEc = code::suspend;

		enum class Status
		{
			Uninit,
			Inprogress,
			Done
		};
		Status mStatus = Status::Uninit;

		MoveRecvProto()
		{
#ifdef COPROTO_LOGGING
			setName("recv_" + std::to_string(gProtoIdx++));
#endif
		}

		span<u8> asSpan(u64 size) override
		{
			internal::tryResize(size, mContainer);

			return internal::asSpan(mContainer);
		}
		error_code resume_(Scheduler& sched) override {

			if (mStatus == Status::Uninit)
			{
				mStatus = Status::Inprogress;
				sched.recv(this, getSlot(), this);
			}
			else if (mStatus == Status::Inprogress)
			{
				mStatus = Status::Done;
				if (mEc == code::suspend)
					mEc = {};
				sched.fulfillDep(*this, mEc, nullptr);
			}

			if (done())
			{
				return mEc;
			}

			return code::suspend;
		}

		bool done() override {
			return mStatus == Status::Done;
		}

		void setError(error_code ec, std::exception_ptr p) override {
			assert(ec != code::suspend);
			assert(p == nullptr && "exception_ptr not supported (MoveRecvProto)");
			mEc = ec;
		}
		std::exception_ptr getExpPtr() override {
			return nullptr;
		}

		error_code getErrorCode() override {
			return mEc;
		}
		void* getValue() override { return &mContainer; };

	};


	template<typename Container, bool allowResize = true>
	class RefRecvProto : public Resumable, public RecvBuffer
	{
	public:
		Container& mContainer;
		error_code mEc = code::suspend;
		enum class Status
		{
			Uninit,
			Inprogress,
			Done
		};
		Status mStatus = Status::Uninit;


		RefRecvProto(Container& t)
			:mContainer(t)
		{
#ifdef COPROTO_LOGGING
			setName("recv_" + std::to_string(gProtoIdx++));
#endif
		}

		span<u8> asSpan(u64 size) override
		{
			if constexpr (allowResize)
				internal::tryResize(size, mContainer);
			return internal::asSpan(mContainer);
		}

		error_code resume_(Scheduler& sched) override {


			if (mStatus == Status::Uninit)
			{
				mStatus = Status::Inprogress;
				sched.recv(this, getSlot(), this);
			}
			else if (mStatus == Status::Inprogress)
			{
				mStatus = Status::Done;
				if (mEc == code::suspend)
					mEc = {};
				sched.fulfillDep(*this, mEc, nullptr);
			}

			if (done())
			{
				return mEc;
			}

			return code::suspend;
		}
		void setError(error_code ec, std::exception_ptr p) override {
			assert(ec != code::suspend);
			assert(p == nullptr && "exception_ptr not supported (RefRecvProto)");
			mEc = ec;
		}
		std::exception_ptr getExpPtr() override {
			return nullptr;
		}


		error_code getErrorCode() override {
			return mEc;
		}

		bool done() override {
			return mStatus == Status::Done;
		}

	};

	template<typename Container>
	class RefSendProto : public SendProto
	{
	public:
		Container& mContainer;
		error_code mEc = code::suspend;

		enum class Status
		{
			Uninit,
			Inprogress,
			Done,
		};
		Status mStatus = Status::Uninit;

		RefSendProto(Container& t)
			:mContainer(t)
		{
#ifdef COPROTO_LOGGING
			setName("send_" + std::to_string(gProtoIdx++));
#endif
		}

		SendBuffer getBuffer()
		{
			SendBuffer ret;
			ret.mStorage.emplace<RefSendBuffer<Container>>(mContainer);
			return ret;
		}

		error_code resume_(Scheduler& sched) override
		{
			if (mStatus == Status::Uninit)
			{
				if (mContainer.size() == 0)
				{
					mEc = code::sendLengthZeroMsg;
					mStatus = Status::Done;
					sched.fulfillDep(*this, mEc, nullptr);
				}
				else
				{
					mStatus = Status::Inprogress;
					sched.send_(getBuffer(), getSlot(), this);
				}
			}
			else if (mStatus == Status::Inprogress)
			{
				mStatus = Status::Done;
				if (mEc == code::suspend)
					mEc = {};
				sched.fulfillDep(*this, mEc, nullptr);
			}

			if (done())
			{
				return mEc;
			}

			return code::suspend;
		}


		void setError(error_code ec, std::exception_ptr p) override {
			assert(ec != code::suspend);
			assert(p == nullptr);
			mEc = ec;
		}
		std::exception_ptr getExpPtr() override {
			return nullptr;
		}

		error_code getErrorCode() override {
			return mEc;
		}

		bool done() override {
			return mStatus == Status::Done;
		}
	};


	template<typename Container>
	class MvSendProto : public SendProto
	{
	public:
		Container mContainer;
		error_code mEc = code::suspend;

		MvSendProto(Container&& t)
			:mContainer(std::move(t))
		{
#ifdef COPROTO_LOGGING
			setName("send_" + std::to_string(gProtoIdx++));
#endif
		}


		SendBuffer getBuffer()
		{
			SendBuffer ret;
			ret.mStorage.emplace<MvSendBuffer<Container>>(std::move(mContainer));
			return ret;
		}

		error_code resume_(Scheduler& sched) override {


			if (mContainer.size() == 0)
			{
				mEc = code::sendLengthZeroMsg;
				sched.fulfillDep(*this, mEc, nullptr);
			}
			else
			{
				sched.send_(getBuffer(), getSlot(), nullptr);
				mEc = code::success;
				sched.fulfillDep(*this, mEc, nullptr);
			}
			return mEc;
		}

		void setError(error_code ec, std::exception_ptr p) override {
			assert(0 && "not supported (RefSendProto)");
		}
		std::exception_ptr getExpPtr() override {
			return nullptr;
		}

		error_code getErrorCode() override {
			return mEc;
		}
		bool done() override {
			return mEc != code::suspend;
		}
	};


	template<typename Container>
	ProtoV<void> send(Container& t)
	{
		ProtoV<void> proto;
		proto.mBase.emplace<RefSendProto<Container>>(t);
		return proto;
	}

	template<typename Container>
	ProtoV<void> send(Container&& t)
	{
		ProtoV<void> proto;
		proto.mBase.emplace<MvSendProto<Container>>(std::forward<Container>(t));
		return proto;
	}


	template<typename Container>
	ProtoV<void> recv(Container& t)
	{
		ProtoV<void> proto;
		proto.mBase.emplace<RefRecvProto<Container>>(t);
		return proto;
	}


	template<typename Container>
	ProtoV<void> recvFixedSize(Container& t)
	{
		ProtoV<void> proto;
		proto.mBase.emplace<RefRecvProto<Container, false>>(t);
		return proto;
	}


	template<typename Container>
	ProtoV<Container> recv()
	{
		ProtoV<Container> proto;
		proto.mBase.emplace<MoveRecvProto<Container>>();
		return proto;
	}

	template<typename value_type>
	ProtoV<std::vector<value_type>> recvVec()
	{
		return recv<std::vector<value_type>>();
	}





}
