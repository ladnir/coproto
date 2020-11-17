#pragma once
#include "coproto/coproto.h"
#include <string>

namespace examples
{
	coproto::Proto echoServer();
	coproto::Proto echoClient(std::string msg);
}