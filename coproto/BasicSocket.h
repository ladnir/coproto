#pragma once
#include "Defines.h"
#include "Scheduler.h"
#include "Proto.h"

namespace coproto
{

	struct IoRequest
	{
		enum Type
		{
			Send,
			Recv
		};
	};

	class BasicExecutor
	{
	public: 

		struct BasicSocket : public Socket
		{
		


		};


		Scheduler mSched;
		Proto<>* mProto;

		BasicExecutor(Proto<>& protocol)
			: mProto(&protocol)
		{
			mSched.scheduleReady(*mProto->mBase.get());
		}

		bool done()
		{
			return false;
		}

		IoRequest getIoRequest();

	};


	namespace tests
	{
		void Simple_BasicExecutor_test();
	}
}

