
#define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING

#include "Proto.h"
#include <algorithm>
#include <numeric>
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
		{
				setName("recv_" + std::to_string(gProtoIdx++));
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

			if (mEc == code::noMessageAvailable)
			{
				sched.scheduleNext(*this);
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
		{
			setName("recv_" + std::to_string(gProtoIdx++));
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

			if (mEc == code::noMessageAvailable)
			{
				sched.scheduleNext(*this);
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
		{
			setName("send_" + std::to_string(gProtoIdx++));
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
			mEc = sched.send(*this);
			if (done())
			{
				sched.fulfillDep(*this, mEc, nullptr);
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
	class MvSendBuff : public Buffer, public ProtoBase
	{
	public:
		Container mContainer;
		error_code mEc = code::noMessageAvailable;

		MvSendBuff(Container&& t)
			:mContainer(std::move(t))
		{
			setName("send_" + std::to_string(gProtoIdx++));

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

			mEc = sched.send(*this);
			if (done())
			{
				sched.fulfillDep(*this, mEc, nullptr);
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


	error_code LocalScheduler::Sock::recv(Buffer& buff)
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

	error_code LocalScheduler::Sock::send(Buffer& buff)
	{

		auto data = buff.asSpan();
		if (data.size() == 0)
			return code::sendLengthZeroMsg;

		mSched->mBuffs[mIdx ^ 1].emplace_back(data.begin(), data.end());
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
			//std::cout << sched.mScheds[0].getDot() << std::endl;
			//std::cout << sched.mScheds[1].getDot() << std::endl;

			if (sched.mScheds[0].numRounds() != 6)
				throw std::runtime_error("num round");
			if (sched.mScheds[1].numRounds() != 6)
				throw std::runtime_error("num round");
			if (ec)
				throw std::runtime_error(ec.message());
		}


		void resultSendRecvTest()
		{
			auto proto = [](bool party) -> Proto<> {
				std::string str("hello from 0");
				//co_await Name("main");

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


		void returnValueTest()
		{
			int val = 42;
			auto proto = [val](bool party) -> Proto<int> {
				std::string str("hello from 0");
				co_return val;
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec)
				throw std::runtime_error(ec.message());

			if (*(int*)p0.mBase->getValue() != val)
				throw std::runtime_error("");
			if (*(int*)p1.mBase->getValue() != val)
				throw std::runtime_error("");
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


		Proto<> echoServer(u64 i, u64 length = 10, std::string name = "p1", bool v = false)
		{
			co_await Name(name + "_server_" + std::to_string(i) + "_" + std::to_string(length));
			auto exp = std::vector<char>(length);
			std::iota(exp.begin(), exp.end(), 0);

			if (v)
				std::cout << name << " es start " << i << " " << length << std::endl;

			if (v)
				std::cout << name << " es recv " << i << " " << length << " begin" << std::endl;
			auto msg = co_await recv<std::vector<char>>();
			if (v)
				std::cout << name << " es recv " << i << " " << length << " done" << std::endl;

			if (exp != msg)
				throw std::runtime_error("");


			if (v)
				std::cout << name << " es send " << i << " " << length << std::endl;
			co_await send(msg);

			co_await EndOfRound();

			if (i)
			{
				co_await echoServer(i - 1);
			}
		}
		Proto<> echoClient(u64 i, u64 length = 10, std::string name = "p0", bool v = false)
		{
			co_await Name(name + "_client_" + std::to_string(i) + "_" + std::to_string(length));
			if (v)
				std::cout << name << " ec start " << i << " " << length << std::endl;

			auto msg = std::vector<char>(length);
			std::iota(msg.begin(), msg.end(), 0);
			if (v)
				std::cout << name << " ec send " << i << " " << length << std::endl;
			co_await send(msg);
			co_await EndOfRound();

			if (v)
				std::cout << name << " ec recv " << i << " " << length << " begin" << std::endl;
			auto rev = co_await recv<std::vector<char>>();

			if (v)
				std::cout << name << " ec recv " << i << " " << length << " done" << std::endl;
			if (msg != rev)
			{
				throw std::runtime_error("hello world");
			}

			if (i)
			{
				co_await echoClient(i - 1);
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

					co_await echoServer(n, 10, "p1", false);
				}
				else
				{
					//std::cout << "p0 recv " << std::endl;
					co_await recv(str);
					//std::cout << " p0 recv" << std::endl;

					if (str != "hello from 0")
						throw std::runtime_error(LOCATION);

					co_await echoClient(n, 10, "p0", false);
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
			u64 n = 5;
			auto proto = [&hasEc, n](bool party) -> Proto<> {

				if (party)
				{
					std::vector<u64> buff(10);
					co_await send(buff);
					auto ec = co_await throwServer(n).wrap();

					if (ec == code::uncaughtException)
						hasEc = true;
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


		void asyncProtocolTest()
		{
			u64 n = 1;
			auto proto = [n](bool party) -> Proto<> {

				if (party)
				{
					auto name = std::string("p1");
					co_await Name(name);
					std::vector<u64> buff(10);
					//co_await send(buff);

					auto fu0 = co_await echoServer(n, 5, name, false).async();
					auto fu1 = co_await echoServer(n + 2, 6, name).async();
					auto fu2 = co_await echoServer(n, 7, name).async();
					auto fu3 = co_await echoServer(n + 7, 8, name).async();
					auto fu4 = co_await echoServer(n, 9, name).async();
					co_await echoClient(n, 10, name, false);
					//co_await send(buff);

					co_await fu0;
					co_await fu1;
					co_await fu2;
					co_await fu3;
					co_await fu4;
				}
				else
				{
					auto name = std::string("p0");
					co_await Name(name);
					std::vector<u64> buff(10);
					//co_await recv(buff);
					auto fu0 = co_await echoClient(n, 5, name, false).async();
					auto fu1 = co_await echoClient(n + 2, 6, name).async();
					auto fu2 = co_await echoClient(n, 7, name).async();
					auto fu3 = co_await echoClient(n + 7, 8, name).async();
					auto fu4 = co_await echoClient(n, 9, name).async();
					co_await echoServer(n, 10, name, false);
					//co_await recv(buff);

					co_await fu0;
					co_await fu1;
					co_await fu2;
					co_await fu3;
					co_await fu4;
				}
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);


			//std::cout << sched.mScheds[0].getDot() << std::endl;
			//std::cout << sched.mScheds[1].getDot() << std::endl;

			if (sched.mScheds[0].numRounds() != n + 1 + 8)
				throw std::runtime_error("num round");
			if (sched.mScheds[1].numRounds() != n + 1 + 7)
				throw std::runtime_error("num round");

			if (ec)
				throw std::runtime_error(ec.message());
		}

		void asyncThrowProtocolTest()
		{
			u64 n = 3;
			auto proto = [n](bool party) -> Proto<> {

				if (party)
				{
					std::vector<u64> buff(10);
					co_await send(buff);

					auto token = co_await throwServer(n).async();
					//co_await send(buff);

					co_await token;
				}
				else
				{
					std::vector<u64> buff(10);
					co_await recv(buff);
					co_await throwClient(n);
					//co_await recv(buff);

					//co_await fu;
				}
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec != code::uncaughtException)
				throw std::runtime_error("");
		}


		void endOfRoundTest()
		{

			auto recvProto = [&]() -> Proto<> {
				std::vector<u8> msg;
				co_await recv(msg);
				co_await EndOfRound();
				co_await send(msg);
			};
			auto sendProto = [&]() -> Proto<> {
				std::vector<u8> msg(10);
				co_await send(msg);
				co_await EndOfRound();
				co_await recv(msg);
			};

			auto recvProto2 = [&]() -> Proto<> {
				std::vector<u8> msg(10);
				co_await recvProto();
				co_await send(msg);
			};
			auto sendProto2 = [&]() -> Proto<> {
				std::vector<u8> msg;
				co_await sendProto();
				co_await recv(msg);
			};

			auto p0 = sendProto2();
			auto p1 = recvProto2();
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);


			if (sched.mScheds[0].numRounds() != 2)
				throw std::runtime_error("num round");
			if (sched.mScheds[1].numRounds() != 1)
				throw std::runtime_error("num round");
			if (ec)
				throw std::runtime_error(ec.message());
		}
	}
}