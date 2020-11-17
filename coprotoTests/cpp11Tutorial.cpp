#include "cpp11Tutorial.h"
#include "cpp20Tutorial.h"
#include "coproto/coproto.h"
#include "coproto/Tests.h"
#include "coproto/Macros.h"
#include <string>

using namespace coproto;
namespace {

	Proto echoClient(std::string message)
	{
		// we have to implement each protocol
		// as a custom struct which inherits
		// from NativeProto.
		struct Impl : NativeProto
		{
			// capture the parameters in the constructor.
			std::string message;
			Impl(std::string& s)
				:message(s)
			{}

			// implement the protocol.
			error_code resume() override
			{
				// first we have to start with the CP_BEGIN();
				//  macro. We will discuss what this does later.
				CP_BEGIN();
				std::cout << "echo client sending: " << message << std::endl;

				// send the size of the message and the message 
				// itself. Instead of using the c++20 co_await keyword
				// we will use the CP_AWAIT macro to acheive a similar
				// result. This will pause the funcation at this point
				// until the operation has completed.
				CP_AWAIT(send(message.size()));
				CP_AWAIT(send(message));

				// wait for the server to respond.
				CP_AWAIT(recv(message));
				std::cout << "echo client received: " << message << std::endl;

				// finally, we end the coroutine with CP_END
				// and must return an error_code.
				CP_END();
				return{};
			}
		};

		// construct an instance of this protocol and return it.
		return makeProto<Impl>(message);
	}

	Proto echoServer()
	{
		struct Impl : public NativeProto
		{
			// all local variables which have a lifetime
			// which crosses over and CP_AWAIT call must
			// be stored as a member variable.
			size_t size;
			std::string message;

			error_code resume() override
			{
				CP_BEGIN();

				CP_AWAIT(recv(size));

				message.resize(size);

				// the size of the received message must match.
				// if not as error will occur.
				CP_AWAIT(recv(message));

				std::cout << "echo server received: " << message << std::endl;

				// send the result back.
				CP_AWAIT(send(message));

				CP_END();
				return{};
			}
		};
		return makeProto<Impl>();
	}

	void echoExample()
	{
		std::cout << Color::Green << " ----------- echoExample 11 ----------- " << std::endl << Color::Default;
		// coproto is lazy in that when you 
		// construct a protocol nothing actually 
		// happens apart from caputuring the 
		// arguments.
		auto server = echoServer();
		auto client = echoClient("hello world");

		// to actually execute the protocol,
		// the user must invoke them in some way.
		// One easy way is with LocalEvaluator
		// which will runs both sides of the protocol.
		LocalEvaluator eval;
		eval.execute(server, client);
	}


	Proto macroClient(std::string message)
	{
		struct Impl : NativeProto
		{
			std::string message;
			Impl(std::string& s)
				:message(s)
			{}

			error_code resume() override
			{

				// the CP_BEGIN(); simply starts a switch statement 
				// and jumpts to the index specified by _resume_idx_,
				// a member variable of NativeProto.
				switch (this->_resume_idx_)
				{
				case 0:

					std::cout << "macro client sending: " << message << std::endl;

					// the statement CP_AWAIT(send(message.size()));
					// effectively expands to the following
					{
						Proto proto = send(message.size());
						error_code ec = this->await(std::move(proto));
						if (ec)
						{
							this->_resume_idx_ = 1;
							return ec;
						}
					}
				case 1:

					// what the code above does is run the protocol
					// returned by the send(...). If it completes immediately
					// then the error_code returns success and the function 
					// continues to execute. Otherwise, the _resume_idx_ is
					// set to be the next case statement and the function
					// returns. When ever this subprotocol completes, ie the 
					// send operation, then this resume function will be called 
					// again. The initial switch statement will then jump up 
					// back to where we left off.

					// It is possible to implement your protocol in some other way.
					// For example, without the switch statement. There are basically
					// three special error_codes that control how the protocol is called.

					// if this protocol return the coproto::code::suspend error_code,
					// then the protocol will not be called again unwil some other
					// process schedules it. This is exactly what the this->await(...)
					// funtion takes care for use. 

					// if this protocol return a success error_code, eg {}, then it is
					// considered completed and dependent protocols whcih are awaiting
					// on this one will be scheduled.

					// if this protocol returns any other error_code, this the protocol
					// is considered completed with an error. dependent protocols might be 
					// called or canceled depending on how they are configured.

					// we will not expand the next calls since they are the same.
					CP_AWAIT(send(message));
					CP_AWAIT(recv(message));

					// finally, the CP_END(); closes the switch statement.
				default:
					break;
				}
				return{};
			}
		};

		// construct an instance of this protocol and return it.
		return makeProto<Impl>(message);
	}

	void macroExample()
	{
		std::cout << Color::Green << " ----------- macroExample 11 ----------- " << std::endl << Color::Default;

		auto server = echoServer();
		auto client = macroClient("hello world");

		LocalEvaluator eval;
		eval.execute(server, client);
	}


}


void cpp11Tutorial()
{
	echoExample();
	macroExample();
}
