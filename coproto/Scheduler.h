#pragma once
#include "error_code.h"
#include "Buffers.h"

namespace coproto
{
	//struct ProtoAwaiter;
	//struct EcProtoAwaiter;
	class ProtoBase;
	class Scheduler
	{
	public:
		virtual error_code recv(Buffer& data) = 0;
		virtual error_code send(Buffer& data) = 0;

		virtual void addProto(ProtoBase& proto) = 0;
		virtual void removeProto(ProtoBase& proto) = 0;
	};



}
