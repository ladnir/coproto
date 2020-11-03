#include "LocalExecutor.h"


namespace coproto
{



	error_code LocalExecutor::InterlaceSock::recv(span<u8> data)
	{
		error_code ec;
		if (mInbound.size())
		{
			auto& front = mInbound.front();
			assert(data.size() == front.size());

			//if (buff.asSpan().size() != front.size())
			//	ec = buff.tryResize(front.size());

			//if (!ec)
			//{
			std::memcpy(data.data(), front.data(), data.size());
			mInbound.pop_front();
			//}
		}
		else
		{
			ec = code::suspend;
		}

		return ec;
	}

	error_code LocalExecutor::InterlaceSock::send(span<u8> data)
	{

		//auto data = buff.asSpan();
		//if (data.size() == 0)
		//	return code::sendLengthZeroMsg;

		mOutbound.emplace_back(data.begin(), data.end());
		return {};
	}






	error_code LocalExecutor::BlockingSock::recv(span<u8> data)
	{
		const auto vec = mInbound.pop();

		if (vec.size() == 0)
		{
			return code::ioError;
		}

		assert(vec.size() == data.size());

		std::copy(vec.begin(), vec.end(), data.begin());

		return {};
	}

	error_code LocalExecutor::BlockingSock::send(span<u8> data)
	{
		mOther->mInbound.emplace(data.begin(), data.end());
		return {};
	}

}