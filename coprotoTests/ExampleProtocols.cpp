#include "ExampleProtocols.h"

#include "coproto/Macros.h"

namespace examples
{

	coproto::Proto echoClient(std::string message)
	{
		struct Impl : coproto::NativeProto
		{
			std::string message;
			Impl(std::string& s) :message(s) {}

			coproto::error_code resume() override
			{
				CP_BEGIN();
				CP_AWAIT(coproto::send(std::move(message)));
				CP_AWAIT(coproto::recvResize(message));
				CP_END();
				return{};
			}
		};
		return coproto::makeProto<Impl>(message);
	}

	coproto::Proto echoServer()
	{
		struct Impl : public coproto::NativeProto
		{
			std::string message;
			coproto::error_code resume() override
			{
				CP_BEGIN();
				CP_AWAIT(coproto::recvResize(message));
				CP_AWAIT(coproto::send(message));
				CP_END();
				return{};
			}
		};
		return coproto::makeProto<Impl>();
	}
}