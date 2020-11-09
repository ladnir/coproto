#pragma once
#include "Defines.h"
#include "TypeTraits.h"
#include <cassert>
#include "error_code.h"
#include <iostream>
#include <memory>
#include "InlinePoly.h"
#include "Proto.h"

namespace coproto
{
	std::string hexPtr(void* p);
	namespace internal
	{

		template<typename Container>
		typename std::enable_if<is_resizable_trivial_container_v<Container>, error_code>::type
			tryResize(u64 size, Container& container)
		{
			if (size % sizeof(typename Container::value_type))
				return code::badBufferSize;
			try {
				container.resize(size / sizeof(typename Container::value_type));
				return {};
			}
			catch (...)
			{
				return code::badBufferSize;
			}
		}

		template<typename Container>
		typename std::enable_if<!is_resizable_trivial_container_v<Container>, error_code>::type
			tryResize(u64 size, Container& container)
		{
			return code::noResizeSupport;
		}

		template<typename Container>
		typename std::enable_if<is_trivial_container_v<Container>, span<u8>>::type
			asSpan(Container& container)
		{
			return span<u8>((u8*)container.data(), container.size() * sizeof(typename Container::value_type));
		}

		template<typename ValueType>
		typename std::enable_if<std::is_trivial_v<ValueType>, span<u8>>::type
			asSpan(ValueType& container)
		{
			return span<u8>((u8*)&container, sizeof(ValueType));
		}
	}


	struct SendOp
	{
		virtual ~SendOp() {}
		virtual span<u8> asSpan() = 0;

	};

	template<typename Container>
	struct RefSendBuffer : public SendOp
	{
		Container& mCont;

		RefSendBuffer(Container& c)
			: mCont(c)
		{}

		RefSendBuffer(RefSendBuffer&&) = default;

		span<u8> asSpan() override
		{
			return internal::asSpan(mCont);
		}

	}; 


	template<typename Container>
	struct MvSendBuffer : public SendOp
	{
		Container mCont;

		MvSendBuffer(Container&& c)
			:mCont(std::forward<Container>(c))
		{}

		span<u8> asSpan() override
		{
			return internal::asSpan(mCont);
		}

	};


	struct SendBuffer
	{
		internal::InlinePoly<SendOp, sizeof(u64) * 8> mStorage;

		span<u8> asSpan() {
			return mStorage->asSpan();
		}
	};


	struct RecvBuffer 
	{
		virtual span<u8> asSpan(u64 resize) = 0;
	};


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
			else if(mStatus == Status::Inprogress)
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
	Proto<void> send(Container& t)
	{
		Proto<void> proto;
		proto.mBase.emplace<RefSendProto<Container>>(t);
		return proto;
	}

	template<typename Container>
	Proto<void> send(Container&& t)
	{
		Proto<void> proto;
		proto.mBase.emplace<MvSendProto<Container>>(std::forward<Container>(t));
		return proto;
	}


	template<typename Container>
	Proto<void> recv(Container& t)
	{
		Proto<void> proto;
		proto.mBase.emplace<RefRecvProto<Container>>(t);
		return proto;
	}


	template<typename Container>
	Proto<void> recvFixedSize(Container& t)
	{
		Proto<void> proto;
		proto.mBase.emplace<RefRecvProto<Container, false>>(t);
		return proto;
	}


	template<typename Container>
	Proto<Container> recv()
	{
		Proto<Container> proto;
		proto.mBase.emplace<MoveRecvProto<Container>>();
		return proto;
	}

	template<typename value_type>
	Proto<std::vector<value_type>> recvVec()
	{
		return recv<std::vector<value_type>>();
	}





}
