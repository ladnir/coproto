#include "cpp20Tutorial.h"
#include "coproto/coproto.h"
#include "coproto/Tests.h"
#include <string>

using namespace coproto;

Proto echoClient(std::string message)
{

	std::cout << "echo client sending: " << message << std::endl;

	// send the size of the message and 
	// the message itself.
	co_await send(message.size());
	co_await send(message);

	// wait for the server to respond.
	co_await recv(message);
	std::cout << "echo client received: " << message << std::endl;
}

Proto echoServer()
{
	// receive the size.
	size_t size;
	co_await recv(size);


	std::string message;
	message.resize(size);

	// the size of the received message must match.
	// if not as error will occur.
	co_await recv(message);

	std::cout << "echo server received: " << message << std::endl;

	// send the result back.
	co_await send(message);
}

void echoExample()
{
	std::cout << Color::Green << " ----------- echoExample ----------- " << std::endl << Color::Default;
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

Proto resizeServer()
{
	// no need to sent the size.
	// Containers can be dynamicaly resized.
	std::string message;
	co_await recvResize(message);

	// or have the container returned.
	auto message2 = co_await recv<std::string>();

	std::cout << "echo server received: " << message << " " << message2<< std::endl;

	// moving the send message in improves efficency
	// by allowinng the protocol proceed immediately
	// while not moving results in blocking until
	// all data has been sent.
	co_await send(std::move(message));
	co_await send(std::move(message2));
}

Proto resizeClient(std::string s0, std::string s1)
{
	co_await send(s0);
	co_await send(s1);

	co_await recv(s0);
	co_await recv(s1);
}

void resizeExample()
{
	std::cout << Color::Green << " ----------- resizeExample ----------- " << std::endl << Color::Default;
	auto server = resizeServer();
	auto client = resizeClient("hello", "world");

	LocalEvaluator eval;
	eval.execute(server, client);
}

Proto errorServer()
{
	for(u64 i = 0 ;; ++i)
	{
		bool doThrow;
		co_await recv(doThrow);
		co_await send(doThrow);

		if (doThrow)
		{
			std::cout << "errorServer throwing at " << i << std::endl;
			throw std::runtime_error("doThrow");
		}
	}
}

Proto errorClient(u64 t)
{
	for (u64 i = 0; ; ++i)
	{
		bool doThrow = i == t;
		co_await send(doThrow);
		co_await recv(doThrow);
	}
}

void errorExample()
{
	std::cout << Color::Green << " ----------- errorExample ----------- " << std::endl << Color::Default;
	auto server = errorServer();
	auto client = errorClient(6);

	LocalEvaluator eval;
	error_code ec = eval.execute(server, client);

	if (ec)
		std::cout << "LocalEvaluator returned: " << ec.message() << std::endl;

}

Proto wrapServer()
{
	for (u64 i = 0;; ++i)
	{
		bool doThrow;
		co_await recv(doThrow);
		co_await send(doThrow);

		if (doThrow)
		{
			std::cout << "errorServer throwing at " << i << std::endl;
			throw std::runtime_error("doThrow");
		}
	}
}

Proto wrapClient(u64 t)
{
	for (u64 i = 0; ; ++i)
	{
		bool doThrow = i == t;
		co_await send(doThrow);

		auto ec = co_await recv(doThrow).wrap();

		if (ec)
		{
			std::cout << "recv(...) returned ec = " << ec.message() << std::endl;
			co_return;
		}
	}
}

void wrapExample()
{
	std::cout << Color::Green << " ----------- wrapExample ----------- " << std::endl << Color::Default;
	auto server = wrapServer();
	auto client = wrapClient(6);

	LocalEvaluator eval;
	error_code ec = eval.execute(server, client);

	if (ec)
		std::cout << "LocalEvaluator returned: " << ec.message() << std::endl;
}


Proto subprotoServer()
{
	std::vector<char> msg(10);
	co_await recv(msg);
	co_await send(msg);

	std::cout << "subprotoServer calling echoClient()" << std::endl;

	std::string str(msg.begin(), msg.end());
	co_await echoClient(str);
}

Proto subprotoClient(u64 t)
{
	std::vector<char> msg(10);
	for (u64 i = 0; i < msg.size(); ++i)
		msg[i] = 'a' + i;

	co_await send(msg);
	co_await recv(msg);

	std::cout << "subprotoClient calling echoServer()" << std::endl;

	co_await echoServer();
}

void subprotoExample()
{
	std::cout << Color::Green << " ----------- subprotoExample ----------- " << std::endl << Color::Default;
	auto server = subprotoServer();
	auto client = subprotoClient(6);

	LocalEvaluator eval;
	error_code ec = eval.execute(server, client);

	if (ec)
		std::cout << "LocalEvaluator returned: " << ec.message() << std::endl;
}

Proto asyncServer()
{
	u64 i;
	co_await recv(i);
	--i;
	co_await send(i);

	if (i)
	{
		// run two instances of the protocol in parallel.
		// these two recursive calls will be interleaved.
		auto future0 = co_await asyncServer().async();
		auto future1 = co_await asyncServer().async();

		co_await future0;
		co_await future1;
	}
}

Proto asyncClient(u64 i)
{
	std::cout << "asyncClient(" << i << ")"<< std::endl;
	co_await send(i);

	// awaiting on EndOfRound tells the scheduler to
	// pause the current protocol if there are other
	// protocols which are ready. This allows us to control
	//exactly how the rounds of protocols are composed.
	co_await EndOfRound{};

	co_await recv(i);

	if (i)
	{
		// run two instances of the protocol in parallel.
		auto future0 = co_await asyncClient(i).async();
		auto future1 = co_await asyncClient(i).async();

		co_await future0;
		co_await future1;
	}
}

void asyncExample()
{
	std::cout << Color::Green << " ----------- asyncExample ----------- " << std::endl << Color::Default;
	auto server = asyncServer();
	auto client = asyncClient(4);

	LocalEvaluator eval;
	error_code ec = eval.execute(server, client);

	if (ec)
		std::cout << "LocalEvaluator returned: " << ec.message() << std::endl;
}



void cpp20Tutorial()
{
	echoExample();
	resizeExample();
	errorExample();
	wrapExample();
	subprotoExample();
	asyncExample();
}
