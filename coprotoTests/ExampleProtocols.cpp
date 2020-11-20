#include "ExampleProtocols.h"

#include "coproto/NativeProto.h"
#include "coproto/Macros.h"

namespace examples
{

	coproto::Proto echoClient(std::string message, coproto::Socket& sock)
	{
		struct Impl : coproto::NativeProto
		{
			std::string message;
			coproto::Socket& sock;
			Impl(std::string& s, coproto::Socket& ss) :message(s), sock(ss) {}

			coproto::error_code resume() override
			{
				CP_BEGIN();
				CP_AWAIT(sock.send(std::move(message)));
				CP_AWAIT(sock.recvResize(message));
				CP_END();
				return{};
			}
		};
		return coproto::makeProto<Impl>(message, sock);
	}

	coproto::Proto echoServer(coproto::Socket& sock)
	{
		struct Impl : public coproto::NativeProto
		{
			std::string message;
			coproto::Socket& sock;
			Impl(coproto::Socket& ss)
				:sock(ss) {}

			coproto::error_code resume() override
			{
				CP_BEGIN();
				CP_AWAIT(sock.recvResize(message));
				CP_AWAIT(sock.send(message));
				CP_END();
				return{};
			}
		};
		return coproto::makeProto<Impl>(sock);
	}
}