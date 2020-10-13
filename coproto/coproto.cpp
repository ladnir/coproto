#include "coproto.h"
#include "Tests.h"
#include <array>
#include "cryptoTools/Common/CLP.h"

namespace coproto
{




	bool RecvAwaiter::await_ready() {
		mEc = mHandle.promise().mSched->recv(*mRecv.mStorage.get());

		// we have a result (message or error) so long as 
		// we done have a no_message_available code.
		mHasResult = mEc != code::noMessageAvailable;

		// Check if we should stop the protocol
		// and have it output an error.
		if (mEc && mHasResult && !mReturnErrors)
		{
			mHandle.promise().setError(mEc);
			//mHasResult = false;
			return false;
		}
		return mHasResult;
	}

	void RecvAwaiter::await_resume() {
		if (mHasResult == false)
		{
			await_ready();
			assert(mHasResult);
		}
	}

	bool SendAwaiter::await_ready() {
		mEc = mHandle.promise().mSched->send(*mData.mStorage.get());

		// we have a result (message or error) so long as 
		// we done have a no_message_available code.
		mHasResult = mEc != code::noMessageAvailable;

		// Check if we should stop the protocol
		// and have it output an error.
		if (mEc && !mReturnErrors)
		{
			mHandle.promise().setError(mEc);
			//mHasResult = false;
			return false;
		}

		return mHasResult;
	}

	void SendAwaiter::await_resume() {
		if (mHasResult == false)
		{
			assert(await_ready());
		}
	}

	error_code LocalScheduler::Sched::recv(Buffer& buff)
	{
		error_code ec;
		if (mSched->mBuffs[mIdx].size())
		{
			auto& front = mSched->mBuffs[mIdx].front();
			if(buff.asSpan().size() != front.size())
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

	void LocalScheduler::Sched::addProto(ProtoAwaiter& proto)
	{
		mStack.push_back({ &proto.mProto, &proto });
		proto.mProto.mHandle.get().promise().mSched = this;

		runOne();
	}

	void LocalScheduler::Sched::runOne()
	{
		if (mStack.back().mProto->done() == false)
		{
			mStack.back().mProto->resume();
		}

		if (mStack.back().mProto->done())
		{
			if (mStack.size() > 1 && 
				mStack.back().mProto->getErrorCode())
			{
				auto& child = mStack.back().mProto->mHandle.get().promise();
				auto& awaiter = *mStack.back().mAwaiter;

				if (awaiter.mReturnErrors == false)
				{
					auto& parent = mStack[mStack.size() - 2].mProto->mHandle.get().promise();
					parent.mExPtr = std::move(child.mExPtr);
					parent.setError(child.mEc);
				}
			}
			mStack.pop_back();
			

			if (mStack.size())
			{
				runOne();
			}
		}

	}

	error_code LocalScheduler::execute(Proto& p0, Proto& p1)
	{
		mScheds[0].mIdx = 0; 
		mScheds[0].mSched = this;
		mScheds[1].mIdx = 1;
		mScheds[1].mSched = this;

		mScheds[0].mStack.emplace_back(&p0, nullptr);
		mScheds[1].mStack.emplace_back(&p1, nullptr);
		p0.setScheduler(mScheds[0]);
		p1.setScheduler(mScheds[1]);

		while (!p0.done() || !p1.done())
		{

			if (!p0.done())
			{
				mScheds[0].runOne();
					
					//mStack.back()->resume();
				//p0.resume();
				if (p0.getErrorCode())
					return p0.getErrorCode();

				//if (mScheds[1].mStack.back()->done())
				//{
				//	std::cout << "done 0 " << std::endl;
				//}
			}
			if (!p1.done())
			{
				mScheds[1].runOne();

				//mScheds[1].mStack.back()->resume();
				////p1.resume();
				if (p1.getErrorCode())
					return p1.getErrorCode();

				//if (mScheds[1].mStack.back()->done())
				//{
				//	std::cout << "done 1 " << std::endl;
				//}
			}
		}

		return {};
	}


	Proto ProtoPromise::get_return_object() { return Proto{ *this }; }

	ProtoAwaiter ProtoPromise::await_transform(Proto&& p)
	{
		return ProtoAwaiter(std::coroutine_handle<ProtoPromise>::from_promise(*this), std::move(p.mHandle));
	}

	EcProtoAwaiter ProtoPromise::await_transform(EcProto&& p)
	{
		return EcProtoAwaiter(std::coroutine_handle<ProtoPromise>::from_promise(*this), std::move(p));
	}















































	namespace tests
	{
		void strSendRecvTest()
		{
			auto proto = [](bool party) -> Proto{
				std::string str("hello from 0");

				for (oc::u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						co_await send(str);
						co_await receive(str);

						if (str != "hello from " + std::to_string(i * 2 + 1))
							throw std::runtime_error(LOCATION);
						str.back() += 1;
					}
					else
					{
						co_await receive(str);

						if (str != "hello from " + std::to_string(i * 2 + 0))
							throw std::runtime_error(LOCATION);

						str.back() += 1;
						co_await send(str);
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

		void arraySendRecvTest()
		{
			auto proto = [](bool party) -> Proto {
				std::array<char, 10> base;

				for (u64 i = 0; i < base.size(); ++i)
					base[i] = char(i);

				for (oc::u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						auto msg = base;
						msg.back() = char(i * 2);

						co_await send(msg);
						co_await receive(msg);

						auto exp = base;
						exp.back() = char(i * 2 + 1);

						if (msg != exp)
							throw std::runtime_error(LOCATION);
					}
					else
					{
						auto msg = base;
						co_await receive(msg);
						auto exp = base;
						exp.back() = char(i * 2);

						if (msg != exp)
							throw std::runtime_error(LOCATION);

						msg.back() = char(i * 2 + 1);
						co_await send(msg);
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


		void intSendRecvTest()
		{
			auto proto = [](bool party) -> Proto {

				for (oc::u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						auto msg = i * 2;
						co_await send(msg);
						co_await receive(msg);
					}
					else
					{
						u64 msg;
						co_await receive(msg);
						msg = i * 2 + 1;
						co_await send(msg);
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

		void resizeSendRecvTest()
		{
			auto proto = [](bool party) -> Proto {

				std::vector<u64> buff, rBuff;
				for (oc::u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						buff.resize(1 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2);
						co_await send(buff);
						co_await receive(rBuff);

						buff.resize(buff.size() + 1);
						std::fill(buff.begin(), buff.end(), i * 2+1);

						if (buff != rBuff)
							throw std::runtime_error(LOCATION);
					}
					else
					{
						co_await receive(rBuff);

						buff.resize(1 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2);

						if (buff != rBuff)
							throw std::runtime_error(LOCATION);

						buff.resize(buff.size() + 1);
						std::fill(buff.begin(), buff.end(), i * 2 + 1);
						co_await send(buff);
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

		void moveSendRecvTest()
		{
			auto proto = [](bool party) -> Proto {

				std::vector<u64> buff, rBuff;
				for (oc::u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						buff.resize(1 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2);
						co_await send(std::move(buff));
						co_await receive(rBuff);

						buff.resize(2 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2 + 1);

						if (buff != rBuff)
							throw std::runtime_error(LOCATION);
					}
					else
					{
						co_await receive(rBuff);

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

		void typedRecvTest()
		{
			auto proto = [](bool party) -> Proto {

				std::vector<u64> buff, rBuff;
				for (oc::u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						buff.resize(1 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2);
						co_await send(std::move(buff));
						rBuff = co_await receive<std::vector<u64>>();

						buff.resize(2 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2 + 1);

						if (buff != rBuff)
							throw std::runtime_error(LOCATION);
					}
					else
					{
						rBuff = co_await receiveVec<u64>();

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

		Proto echoServer(int i)
		{
			auto msg = co_await receive<std::string>();
			co_await send(std::move(msg));

			if (i)
			{
				echoServer(i - 1);
			}
		}
		Proto echoClient(int i)
		{
			auto msg = std::string("hello world");
			co_await send(msg);
			if (msg != co_await receive<std::string>())
			{
				throw std::runtime_error("hello world");
			}

			if (i)
			{
				echoClient(i - 1);
			}
		}


		void nestedProtocolTest()
		{
			auto proto = [](bool party) -> Proto {

				if (party)
				{
					std::vector<u64> buff(10);
					co_await send(buff);
					co_await echoServer(4);
				}
				else
				{
					std::vector<u64> buff(10);
					co_await receive(buff);
					co_await echoClient(4);
				}
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec)
				throw std::runtime_error("");
		}


		void zeroSendRecvTest()
		{
			auto proto = [](bool party) -> Proto {

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
			auto proto = [](bool party) -> Proto {

				std::vector<u64> buff(3);

				if (party)
				{
					co_await send(buff);
				}
				else
				{
					buff.resize(1);
					co_await receiveFixedSize(buff);
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
			auto proto = [](bool party) -> Proto {

				std::vector<u64> buff;
				auto ec = co_await send(buff).getErrorCode();

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
			auto proto = [](bool party) -> Proto {

				std::vector<u64> buff(3);

				if (party)
				{
					co_await send(buff);
				}
				else
				{
					buff.resize(1);
					auto ec = co_await receiveFixedSize(buff).getErrorCode();

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
			auto proto = [](bool party) -> Proto {
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

		Proto throwServer(int i)
		{
			auto msg = co_await receive<std::string>();
			co_await send(std::move(msg));

			if (i)
				co_await throwServer(i - 1);
			else
				throw std::runtime_error("");
		}
		Proto throwClient(int i)
		{
			auto msg = std::string("hello world");
			co_await send(msg);
			if (msg != co_await receive<std::string>())
			{
				throw std::runtime_error("hello world");
			}

			if (i)
				co_await throwClient(i - 1);
		}

		void nestedProtocolThrowTest()
		{
			
			auto proto = [](bool party) -> Proto {

				if (party)
				{
					std::vector<u64> buff(10);
					co_await send(buff);
					co_await throwServer(4);
				}
				else
				{
					std::vector<u64> buff(10);
					co_await receive(buff);
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
			auto proto = [&hasEc, n](bool party) -> Proto {

				if (party)
				{
					std::vector<u64> buff(10);
					co_await send(buff);
					auto ec = co_await throwServer(n).getErrorCode();
					if (ec)
					{
						hasEc = true;
					}
				}
				else
				{
					std::vector<u64> buff(10);
					co_await receive(buff);
					co_await throwClient(n);
				}
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			LocalScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec||!hasEc)
				throw std::runtime_error("");
		}



	}
}


int main(int argc, char** argv)
{
	oc::CLP cmd(argc, argv);

	if (cmd.isSet("u") == false)
		cmd.set("u");

	coproto::testCollection.runIf(cmd);


	return 0;
}
