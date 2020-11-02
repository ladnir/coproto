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
				return code::bufferResizeNotMultipleOfValueType;
			try {
				container.resize(size / sizeof(typename Container::value_type));
				return {};
			}
			catch (...)
			{
				return code::bufferResizedFailed;
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

	struct BufferInterface
	{
		virtual span<u8> asSpan() = 0;
		virtual error_code tryResize(u64 size) = 0;
	};


	struct Buffer
	{
		internal::InlinePoly<BufferInterface, sizeof(u64) * 4> mStorage;

		span<u8> asSpan() {
			return mStorage->asSpan();
		}
		error_code tryResize(u64 size) {
			return mStorage->tryResize(size);
		}
	};




	class IoProto : public Resumable, public BufferInterface
	{};



	template<typename Container>
	class RecvProto : public IoProto
	{
	public:
		Container mContainer;
		error_code mEc = code::suspend;

		RecvProto()
		{
#ifdef COPROTO_LOGGING
			setName("recv_" + std::to_string(gProtoIdx++));
#endif
		}

		span<u8> asSpan() override
		{
			return internal::asSpan(mContainer);
		}

		error_code tryResize(u64 size) override
		{
			return internal::tryResize(size, mContainer);
		}

		error_code resume_(Scheduler& sched) override {
			mEc = sched.recv(*this);

			if (done())
			{
				sched.fulfillDep(*this, mEc, nullptr);
			}

			return mEc;
		}

		bool done() override {
			return mEc != code::suspend;
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
		void* getValue() override { return &mContainer; };
	};


	template<typename Container, bool allowResize = true>
	class RefRecvProto : public IoProto
	{
	public:
		Container& mContainer;
		error_code mEc = code::suspend;

		RefRecvProto(Container& t)
			:mContainer(t)
		{
#ifdef COPROTO_LOGGING
			setName("recv_" + std::to_string(gProtoIdx++));
#endif
		}

		span<u8> asSpan() override
		{
			return internal::asSpan(mContainer);
		}

		error_code tryResize(u64 size) override
		{
			if constexpr (allowResize)
				return internal::tryResize(size, mContainer);
			else
				return code::noResizeSupport;
		}

		error_code resume_(Scheduler& sched) override {

			mEc = sched.recv(*this);
			if (done())
			{
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
	class RefSendProto : public IoProto
	{
	public:
		Container& mContainer;
		error_code mEc = code::suspend;

		RefSendProto(Container& t)
			:mContainer(t)
		{
#ifdef COPROTO_LOGGING
			setName("send_" + std::to_string(gProtoIdx++));
#endif
		}

		span<u8> asSpan() override
		{
			return internal::asSpan(mContainer);
		}

		error_code tryResize(u64 size) override
		{
			return internal::tryResize(size, mContainer);
		}


		error_code resume_(Scheduler& sched) override {
			mEc = sched.send_(*this);

			assert(mEc != code::suspend);

			if (done())
			{
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
	class MvSendProto : public IoProto
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

		span<u8> asSpan() override
		{
			return internal::asSpan(mContainer);
		}

		error_code tryResize(u64 size) override
		{
			return internal::tryResize(size, mContainer);
		}

		error_code resume_(Scheduler& sched) override {

			mEc = sched.send_(*this);
			assert(mEc != code::suspend);
			if (done())
			{
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
		proto.mBase.emplace<RecvProto<Container>>();
		return proto;
	}

	template<typename value_type>
	Proto<std::vector<value_type>> recvVec()
	{
		return recv<std::vector<value_type>>();
	}





}
