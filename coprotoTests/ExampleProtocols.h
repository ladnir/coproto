#pragma once
#include "coproto/coproto.h"
#include "coproto/Socket.h"
#include <string>

namespace examples
{
	coproto::Proto echoServer(coproto::Socket& s);
	coproto::Proto echoClient(std::string msg, coproto::Socket& s);
}