
#define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING

#include "Proto.h"

#include <string>
#include "cryptoTools/Common/Defines.h"
namespace coproto
{



	template<typename Container>
	class RecvBuff : public Buffer, public ProtoBase
	{
	public:
		Container mContainer;
		error_code mEc = code::noMessageAvailable;

		RecvBuff()
		{}

		span<u8> asSpan() override
		{
			return internal::asSpan(mContainer);
		}

		error_code tryResize(u64 size) override
		{
			return internal::tryResize(size, mContainer);
		}

		error_code resume() override {
			assert(mSched);
			mEc = mSched->recv(*this);

			if (done())
				finalize(mEc, nullptr);

			if (mEc == code::noMessageAvailable)
			{
				mSched->scheduleNext(*this);
				return code::suspend;
			}

			return mEc;
		}

		bool done() override {
			return mEc != code::noMessageAvailable;
		}

		void setError(error_code ec, std::exception_ptr p) override {
			assert(0 && "not supported (RefSendBuff)");
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
	class RefRecvBuff : public Buffer, public ProtoBase
	{
	public:
		Container& mContainer;
		error_code mEc = code::noMessageAvailable;

		RefRecvBuff(Container& t)
			:mContainer(t)
		{}

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

		error_code resume() override {
			assert(mSched);
			mEc = mSched->recv(*this);
			if (done())
				finalize(mEc, nullptr);

			if (mEc == code::noMessageAvailable)
			{
				mSched->scheduleNext(*this);
				return code::suspend;
			}

			return mEc;
		}
		void setError(error_code ec, std::exception_ptr p) override {
			assert(0 && "not supported (RefSendBuff)");
		}
		std::exception_ptr getExpPtr() override {
			return nullptr;
		}


		error_code getErrorCode() override {
			return mEc;
		}

		bool done() override {
			return mEc != code::noMessageAvailable;
		}

	};

	template<typename Container>
	class RefSendBuff : public Buffer, public ProtoBase
	{
	public:
		Container& mContainer;
		error_code mEc = code::noMessageAvailable;

		RefSendBuff(Container& t)
			:mContainer(t)
		{}

		span<u8> asSpan() override
		{
			return internal::asSpan(mContainer);
		}

		error_code tryResize(u64 size) override
		{
			return internal::tryResize(size, mContainer);
		}


		error_code resume() override {
			assert(mSched);
			mEc = mSched->send(*this);
			if (done())
				finalize(mEc, nullptr);

			return mEc;
		}


		void setError(error_code ec, std::exception_ptr p) override {
			assert(0 && "not supported (RefSendBuff)");
		}
		std::exception_ptr getExpPtr() override {
			return nullptr;
		}

		error_code getErrorCode() override {
			return mEc;
		}

		bool done() override {
			return mEc != code::noMessageAvailable;
		}
	};


	template<typename Container>
	class MvSendBuff : public Buffer, public ProtoBase
	{
	public:
		Container mContainer;
		error_code mEc = code::noMessageAvailable;

		MvSendBuff(Container&& t)
			:mContainer(std::move(t))
		{}

		span<u8> asSpan() override
		{
			return internal::asSpan(mContainer);
		}

		error_code tryResize(u64 size) override
		{
			return internal::tryResize(size, mContainer);
		}

		error_code resume() override {
			assert(mSched);
			mEc = mSched->send(*this);
			if (done())
				finalize(mEc, nullptr);

			return mEc;
		}

		void setError(error_code ec, std::exception_ptr p) override {
			assert(0 && "not supported (RefSendBuff)");
		}
		std::exception_ptr getExpPtr() override {
			return nullptr;
		}

		error_code getErrorCode() override {
			return mEc;
		}
		bool done() override {
			return mEc != code::noMessageAvailable;
		}
	};


	template<typename Container>
	Proto<void> send(Container& t)
	{
		Proto<void> proto;
		proto.mBase.emplace<RefSendBuff<Container>>(t);
		return proto;
	}

	template<typename Container>
	Proto<void> send(Container&& t)
	{
		Proto<void> proto;
		proto.mBase.emplace<MvSendBuff<Container>>(std::forward<Container>(t));
		return proto;
	}


	template<typename Container>
	Proto<void> recv(Container& t)
	{
		Proto<void> proto;
		proto.mBase.emplace<RefRecvBuff<Container>>(t);
		return proto;
	}


	template<typename Container>
	Proto<void> recvFixedSize(Container& t)
	{
		Proto<void> proto;
		proto.mBase.emplace<RefRecvBuff<Container, false>>(t);
		return proto;
	}


	template<typename Container>
	Proto<Container> recv()
	{
		Proto<Container> proto;
		proto.mBase.emplace<RecvBuff<Container>>();
		return proto;
	}

	template<typename value_type>
	Proto<std::vector<value_type>> recvVec()
	{
		return recv<std::vector<value_type>>();
	}


	void coproto::LocalScheduler::Sched::runOne()
	{
		while (mReady.size())
		{
			auto task = mReady.front();
			mReady.pop_front();

			auto ec = task->resume();

			//if (ec && mReady.size() > 1)
			//{
			//	//auto& child = frame.mProto->mHandle.get().promise();
			//	auto& parent = *mStack[mStack.size() - 2];

			//	parent.setError(ec, std::move(frame.getExpPtr()));

			//	//if (awaiter.mReturnErrors == false)
			//	//{
			//	//	auto& parent = mStack[mStack.size() - 2].mProto->mHandle.get().promise();
			//	//	parent.mExPtr = std::move(child.mExPtr);
			//	//	parent.setError(child.mEc);
			//	//}
			//}

			//mStack.pop_back();

			//if (!ec || ec != code::suspend)
			//{
			//	if (mStack.size() > 1 &&
			//		ec)
			//	{

			//	}


			//	if (mStack.size())
			//	{
			//		runOne();
			//	}
			//}


		}

		std::swap(mReady, mNext);
	}

	bool coproto::LocalScheduler::Sched::done()
	{
		return mReady.size() == 0;
	}




	error_code LocalScheduler::Sched::recv(Buffer& buff)
	{
		error_code ec;
		if (mSched->mBuffs[mIdx].size())
		{
			auto& front = mSched->mBuffs[mIdx].front();
			if (buff.asSpan().size() != front.size())
				ec = buff.tryResize(front.size());

			if (!ec)
			{
				auto data = buff.asSpan();
				std::memcpy(data.data(), front.data(), data.size());
				mSched->mBuffs[mIdx].pop_front();
			}
		}
		else
		{
			ec = code::noMessageAvailable;
		}

		return ec;
	}

	error_code LocalScheduler::Sched::send(Buffer& buff)
	{

		auto data = buff.asSpan();
		if (data.size() == 0)
			return code::sendLengthZeroMsg;

		mSched->mBuffs[mIdx ^ 1].emplace_back(data.begin(), data.end());
		return {};
	}

	void LocalScheduler::Sched::scheduleNext(ProtoBase& proto)
	{
		mNext.push_back(&proto);

		assert(proto.mSched == nullptr || proto.mSched == this);
		proto.mSched = this;
	}

	void LocalScheduler::Sched::scheduleReady(ProtoBase& proto)
	{
		mReady.push_back(&proto);

		assert(proto.mSched == nullptr || proto.mSched == this);
		proto.mSched = this;
	}

	//void LocalScheduler::Sched::removeProto(ProtoBase& proto)
	//{
	//	assert(&proto == mStack.back());
	//	mStack.pop_back();
	//}

	error_code LocalScheduler::execute(Proto<void>& p0, Proto<void>& p1)
	{
		mScheds[0].mIdx = 0;
		mScheds[0].mSched = this;
		mScheds[1].mIdx = 1;
		mScheds[1].mSched = this;

		mScheds[0].scheduleReady(*p0.mBase.get());
		mScheds[1].scheduleReady(*p1.mBase.get());

		while (
			mScheds[0].done() == false ||
			mScheds[1].done() == false)
		{

			if (mScheds[0].done() == false)
			{
				mScheds[0].runOne();

				if (mScheds[0].done())
				{
					auto e0 = p0.mBase->getErrorCode();
					if (e0)
						return e0;
				}
			}

			if (mScheds[1].done() == false)
			{
				mScheds[1].runOne();

				if (mScheds[1].done())
				{
					auto e1 = p1.mBase->getErrorCode();
					if (e1)
						return e1;
				}
			}
		}

		return {};
	}








	namespace tests
	{

		void strSendRecvTest()
		{
			auto proto = [](bool party) -> Proto<> {
				std::string str("hello from 0");

				for (u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						co_await send(str);
						//std::cout << " p1 sent" << std::endl;
						str = co_await recv<std::string>();
						//std::cout << " p1 recv" << std::endl;

						if (str != "hello from " + std::to_string(i * 2 + 1))
							throw std::runtime_error(LOCATION);
						str.back() += 1;
					}
					else
					{
						co_await recv(str);
						//std::cout << " p0 recv" << std::endl;

						if (str != "hello from " + std::to_string(i * 2 + 0))
							throw std::runtime_error(LOCATION);

						str.back() += 1;
						co_await send(str);
						//std::cout << " p0 sent" << std::endl;

					}
				}
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec)
				throw std::runtime_error(ec.message());
		}


		void resultSendRecvTest()
		{
			auto proto = [](bool party) -> Proto<> {
				std::string str("hello from 0");

				for (u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						//std::cout << " p1 sent " << i << std::endl;
						auto ec = co_await send(str).wrap();
						//std::cout << " p1 sent " << i << " ok" << std::endl;

						//std::cout << " p1 recv " << i << std::endl;
						Result<std::string> r = co_await recv<std::string>().wrap();
						//std::cout << " p1 recv " << i << " ok " << std::endl;

						if (r.hasError())
							throw std::runtime_error(LOCATION);

						str = r.value();

						if (str != "hello from " + std::to_string(i * 2 + 1))
							throw std::runtime_error(LOCATION);
						str.back() += 1;
					}
					else
					{
						//std::cout << " p0 recv " << i << std::endl;
						co_await recv(str);
						//std::cout << " p0 recv " << i << " ok" << std::endl;

						if (str != "hello from " + std::to_string(i * 2 + 0))
							throw std::runtime_error(LOCATION);

						str.back() += 1;

						//std::cout << " p0 sent " << i << std::endl;
						co_await send(str);
						//std::cout << " p0 sent " << i << " ok" << std::endl;

					}
				}
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec)
				throw std::runtime_error(ec.message());
		}




		void typedRecvTest()
		{
			auto proto = [](bool party) -> Proto<> {

				std::vector<u64> buff, rBuff;
				for (oc::u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						buff.resize(1 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2);
						co_await send(std::move(buff));
						rBuff = co_await recv<std::vector<u64>>();

						buff.resize(2 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2 + 1);

						if (buff != rBuff)
							throw std::runtime_error(LOCATION);
					}
					else
					{
						rBuff = co_await recvVec<u64>();

						buff.resize(1 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2);

						if (buff != rBuff)
							throw std::runtime_error(LOCATION);

						buff.resize(buff.size() + 1);
						std::fill(buff.begin(), buff.end(), i * 2 + 1);
						co_await send(std::move(buff));
					}
				}
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec)
				throw std::runtime_error(ec.message());
		}



		void zeroSendRecvTest()
		{
			auto proto = [](bool party) -> Proto<> {

				std::vector<u64> buff;
				co_await send(buff);
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (!ec)
				throw std::runtime_error("");
		}


		void badRecvSizeTest()
		{
			auto proto = [](bool party) -> Proto<> {

				std::vector<u64> buff(3);

				if (party)
				{
					co_await send(buff);
				}
				else
				{
					buff.resize(1);
					co_await recvFixedSize(buff);
				}
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec != code::noResizeSupport)
				throw std::runtime_error(ec.message());
		}


		void zeroSendErrorCodeTest()
		{
			auto proto = [](bool party) -> Proto<> {

				std::vector<u64> buff;
				auto ec = co_await send(buff).wrap();

				if (ec != code::sendLengthZeroMsg)
					throw std::runtime_error("");
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec)
				throw std::runtime_error("");
		}


		void badRecvSizeErrorCodeTest()
		{
			auto proto = [](bool party) -> Proto<> {

				std::vector<u64> buff(3);

				if (party)
				{
					co_await send(buff);
				}
				else
				{
					buff.resize(1);
					auto ec = co_await recvFixedSize(buff).wrap();

					if (ec != code::noResizeSupport)
						throw std::runtime_error("");
				}
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec)
				throw std::runtime_error("");
		}

		void throwsTest()
		{
			auto proto = [](bool party) -> Proto<> {
				throw std::runtime_error("");

				co_return;
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (!ec)
				throw std::runtime_error("");
		}


		Proto<> echoServer(u64 i)
		{
			//std::cout << "p1 echo recv " << i << std::endl;
			auto msg = co_await recv<std::string>();
			//std::cout << "p1 echo send " << i << std::endl;
			co_await send(msg);

			if (i)
			{
				echoServer(i - 1);
			}
		}
		Proto<> echoClient(u64 i)
		{
			auto msg = std::string("hello world");
			//std::cout << "p0 echo send " << i << std::endl;
			co_await send(msg);
			//std::cout << "p0 echo recv " << i << std::endl;
			if (msg != co_await recv<std::string>())
			{
				throw std::runtime_error("hello world");
			}

			if (i)
			{
				echoClient(i - 1);
			}
		}


		void nestedSendRecvTest()
		{
			auto proto = [](bool party) -> Proto<> {
				std::string str("hello from 0");
				u64 n = 5;
				if (party)
				{
					//std::cout << "p1 send " << std::endl;
					auto ec = co_await send(std::move(str)).wrap();
					if (ec)
						throw std::runtime_error(LOCATION);

					co_await echoServer(n);
				}
				else
				{
					//std::cout << "p0 recv " << std::endl;
					co_await recv(str);
					//std::cout << " p0 recv" << std::endl;

					if (str != "hello from 0")
						throw std::runtime_error(LOCATION);

					co_await echoClient(n);
					//std::cout << " p0 sent" << std::endl;

				}
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec)
				throw std::runtime_error(ec.message());
		}


		Proto<> throwServer(u64 i)
		{
			auto msg = co_await recv<std::string>();
			co_await send((msg));

			if (i)
				co_await throwServer(i - 1);
			else
				throw std::runtime_error("");
		}
		Proto<> throwClient(u64 i)
		{
			auto msg = std::string("hello world");
			co_await send(msg);
			if (msg != co_await recv<std::string>())
			{
				throw std::runtime_error("hello world");
			}

			if (i)
				co_await throwClient(i - 1);
		}

		void nestedProtocolThrowTest()
		{

			auto proto = [](bool party) -> Proto<> {

				if (party)
				{
					std::vector<u64> buff(10);
					co_await send(buff);
					co_await throwServer(4);
				}
				else
				{
					std::vector<u64> buff(10);
					co_await recv(buff);
					co_await throwClient(4);
				}
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (!ec)
				throw std::runtime_error("");
		}


		void nestedProtocolErrorCodeTest()
		{
			bool hasEc = false;
			u64 n = 0;
			auto proto = [&hasEc, n](bool party) -> Proto<> {

				if (party)
				{
					std::vector<u64> buff(10);
					co_await send(buff);
					auto ec = co_await throwServer(n).wrap();
					if (ec)
					{
						hasEc = true;
					}
				}
				else
				{
					std::vector<u64> buff(10);
					co_await recv(buff);
					co_await throwClient(n);
				}
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec || !hasEc)
				throw std::runtime_error("");
		}

	}
}