#include "coproto.h"
#include "Tests.h"
#include <array>

namespace coproto
{




	bool RecvAwaiter::await_ready() {
		mEc = mHandle.promise().mSched->recv(mHandle.promise().mId, *mRecv.mStorage.get());

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
		mEc = mHandle.promise().mSched->send(mHandle.promise().mId, *mData.mStorage.get());

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

	error_code InterlaceScheduler::recv(oc::u64 id, Buffer& buff)
	{
		error_code ec;
		if (mBuffs[id].size())
		{
			auto& front = mBuffs[id].front();
			if(buff.asSpan().size() != front.size())
				ec = buff.tryResize(front.size());

			if (!ec)
			{
				auto data = buff.asSpan();
				std::memcpy(data.data(), front.data(), data.size());
				mBuffs[id].pop_front();
			}
		}
		else
		{
			ec = code::noMessageAvailable;
		}

		return ec;
	}

	error_code InterlaceScheduler::send(oc::u64 id, Buffer& buff)
	{
		auto data = buff.asSpan();
		if (data.size() == 0)
			return code::sendLengthZeroMsg;

		mBuffs[id ^ 1].emplace_back(data.begin(), data.end());
		return {};
	}

	error_code InterlaceScheduler::execute(Proto& p0, Proto& p1)
	{
		p0.setScheduler(*this, 0);
		p1.setScheduler(*this, 1);

		while (!p0.done() || !p1.done())
		{

			if (!p0.done())
			{
				p0.resume();
				if (p0.getErrorCode())
					return p0.getErrorCode();
			}
			if (!p1.done())
			{
				p1.resume();
				if (p1.getErrorCode())
					return p1.getErrorCode();
			}
		}

		return {};
	}


	Proto ProtoPromise::get_return_object() { return Proto{ *this }; }

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
			InterlaceScheduler sched;
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
			InterlaceScheduler sched;
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
			InterlaceScheduler sched;
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
			InterlaceScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (ec)
				throw std::runtime_error(ec.message());
		}


		void zeroSendRecvTest()
		{
			auto proto = [](bool party) -> Proto {

				std::vector<u64> buff;
				co_await send(buff);
			};
			auto p0 = proto(0);
			auto p1 = proto(1);
			InterlaceScheduler sched;
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
			InterlaceScheduler sched;
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
			InterlaceScheduler sched;
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
			InterlaceScheduler sched;
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
			InterlaceScheduler sched;
			auto ec = sched.execute(p0, p1);
			if (!ec)
				throw std::runtime_error("");
		}

	}
}


int main()
{
	
	coproto::testCollection.runAll();


	return 0;
}
