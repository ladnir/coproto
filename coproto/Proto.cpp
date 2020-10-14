
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
		error_code mEc = code::ioError;

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
			return mEc;
		}

		bool done() override {
			return !mEc;
		}

		void* getValue() override { return &mContainer; };
	};


	template<typename Container>
	class RefRecvBuff: public Buffer, public ProtoBase
	{
	public:
		Container& mContainer;
		error_code mEc = code::ioError;

		RefRecvBuff(Container& t)
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
			mEc = mSched->recv(*this);
			return mEc;
		}

		bool done() override {
			return !mEc;
		}

	};

	template<typename Container>
	class RefSendBuff : public Buffer, public ProtoBase
	{
	public:
		Container& mContainer;
		error_code mEc = code::ioError;

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
			return mEc;
		}

		bool done() override {
			return !mEc;
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
	Proto<void> recv(Container& t)
	{
		Proto<void> proto;
		proto.mBase.emplace<RefRecvBuff<Container>>(t);
		return proto;
	}


	template<typename Container>
	Proto<Container> recv()
	{
		Proto<Container> proto;
		//RecvBuff<Container> r;
		proto.mBase.emplace<RecvBuff<Container>>();
		return proto;
	}


	void coproto::LocalScheduler::Sched::runOne()
	{
		auto ec = mStack.back()->resume();


		if (!ec || ec != code::noMessageAvailable)
		{
			//if (mStack.size() > 1 &&
			//	ec)
			//{
			//	auto& child = mStack.back().mProto->mHandle.get().promise();
			//	auto& awaiter = *mStack.back().mAwaiter;

			//	if (awaiter.mReturnErrors == false)
			//	{
			//		auto& parent = mStack[mStack.size() - 2].mProto->mHandle.get().promise();
			//		parent.mExPtr = std::move(child.mExPtr);
			//		parent.setError(child.mEc);
			//	}
			//}
			mStack.pop_back();


			if (mStack.size())
			{
				runOne();
			}
		}


	}

	bool coproto::LocalScheduler::Sched::done()
	{
		return !mStack.size();
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

	void LocalScheduler::Sched::addProto(ProtoBase& proto) 
	{
		mStack.push_back(&proto);

		assert(proto.mSched == nullptr || proto.mSched == this);
		proto.mSched = this;
		//proto.mProto.mHandle.get().promise().mSched = this;

		//runOne();
	}

	error_code LocalScheduler::execute(Proto<void>& p0, Proto<void>& p1) 
	{
		mScheds[0].mIdx = 0;
		mScheds[0].mSched = this;
		mScheds[1].mIdx = 1;
		mScheds[1].mSched = this;

		mScheds[0].addProto(*p0.mBase.get());
		mScheds[1].addProto(*p1.mBase.get());

		while (
			mScheds[0].done() == false ||
			mScheds[1].done() == false)
		{

			if (mScheds[0].done() == false)
			{
				mScheds[0].runOne();
			}
			if (mScheds[1].done() == false)
			{
				mScheds[1].runOne();
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

				for (u64 i = 0; i < 1; ++i)
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
	}
}