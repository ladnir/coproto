#include "LocalScheduler.h"


namespace coproto
{



	error_code LocalScheduler::Sock::recv(span<u8> data)
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

	error_code LocalScheduler::Sock::send(span<u8> data)
	{

		//auto data = buff.asSpan();
		//if (data.size() == 0)
		//	return code::sendLengthZeroMsg;

		mOutbound.emplace_back(data.begin(), data.end());
		return {};
	}






}