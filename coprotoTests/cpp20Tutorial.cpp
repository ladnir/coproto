#include "cpp20Tutorial.h"
#include "coproto/coproto.h"
#include "coproto/Tests.h"
#include <string>

using namespace coproto;

#ifdef COPROTO_CPP20


Proto echoClient(std::string message, Socket& sock)
{

	std::cout << "echo client sending: " << message << std::endl;

	// send the size of the message and 
	// the message itself.
	co_await sock.send(message.size());
	co_await sock.send(message);

	// wait for the server to respond.
	co_await sock.recv(message);
	std::cout << "echo client received: " << message << std::endl;
}

Proto echoServer(Socket& sock)
{
	// receive the size.
	size_t size;
	co_await sock.recv(size);


	std::string message;
	message.resize(size);

	// the size of the received message must match.
	// if not as error will occur.
	co_await sock.recv(message);

	std::cout << "echo server received: " << message << std::endl;

	// send the result back.
	co_await sock.send(message);
}

void echoExample()
{
	std::cout << Color::Green << " ----------- echoExample ----------- " << std::endl << Color::Default;
	LocalEvaluator eval;
	auto sockets = eval.getSocketPair();

	// coproto is lazy in that when you 
	// construct a protocol nothing actually 
	// happens apart from caputuring the 
	// arguments.
	auto server = echoServer(sockets[0]);
	auto client = echoClient("hello world", sockets[1]);

	// to actually execute the protocol,
	// the user must invoke them in some way.
	// One easy way is with LocalEvaluator
	// which will runs both sides of the protocol.
	eval.execute(server, client);
}

Proto resizeServer(Socket& sock)
{
	// no need to sent the size.
	// Containers can be dynamicaly resized.
	std::string message;
	co_await sock.recvResize(message);

	// or have the container returned.
	auto message2 = co_await sock.recv<std::string>();

	std::cout << "echo server received: " << message << " " << message2<< std::endl;

	// moving the send message in improves efficency
	// by allowinng the protocol proceed immediately
	// while not moving results in blocking until
	// all data has been sent.
	co_await sock.send(std::move(message));
	co_await sock.send(std::move(message2));
}

Proto resizeClient(std::string s0, std::string s1, Socket& sock)
{
	co_await sock.send(s0);
	co_await sock.send(s1);

	co_await sock.recv(s0);
	co_await sock.recv(s1);
}

void resizeExample()
{
	std::cout << Color::Green << " ----------- resizeExample ----------- " << std::endl << Color::Default;
	LocalEvaluator eval;
	auto sockets = eval.getSocketPair();

	auto server = resizeServer(sockets[0]);
	auto client = resizeClient("hello", "world", sockets[1]);

	eval.execute(server, client);
}

Proto errorServer(Socket& sock)
{
	for(u64 i = 0 ;; ++i)
	{
		bool doThrow;
		co_await sock.recv(doThrow);
		co_await sock.send(doThrow);

		if (doThrow)
		{
			std::cout << "errorServer throwing at " << i << std::endl;
			throw std::runtime_error("doThrow");
		}
	}
}

Proto errorClient(u64 t, Socket& sock)
{
	for (u64 i = 0; ; ++i)
	{
		bool doThrow = i == t;
		co_await sock.send(doThrow);
		co_await sock.recv(doThrow);
	}
}

void errorExample()
{
	std::cout << Color::Green << " ----------- errorExample ----------- " << std::endl << Color::Default;
	LocalEvaluator eval;
	auto sockets = eval.getSocketPair();

	auto server = errorServer(sockets[0]);
	auto client = errorClient(6, sockets[1]);

	error_code ec = eval.execute(server, client);

	if (ec)
		std::cout << "LocalEvaluator returned: " << ec.message() << std::endl;

}

Proto wrapServer(Socket& sock)
{
	for (u64 i = 0;; ++i)
	{
		bool doThrow;
		co_await sock.recv(doThrow);
		co_await sock.send(doThrow);

		if (doThrow)
		{
			std::cout << "errorServer throwing at " << i << std::endl;
			throw std::runtime_error("doThrow");
		}
	}
}

Proto wrapClient(u64 t, Socket& sock)
{
	for (u64 i = 0; ; ++i)
	{
		bool doThrow = i == t;
		co_await sock.send(doThrow);

		auto ec = co_await sock.recv(doThrow).wrap();

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
	LocalEvaluator eval;
	auto sockets = eval.getSocketPair();

	auto server = wrapServer(sockets[0]);
	auto client = wrapClient(6, sockets[1]);

	error_code ec = eval.execute(server, client);

	if (ec)
		std::cout << "LocalEvaluator returned: " << ec.message() << std::endl;
}


Proto subprotoServer(Socket& sock)
{
	std::vector<char> msg(10);
	co_await sock.recv(msg);
	co_await sock.send(msg);

	std::cout << "subprotoServer calling echoClient()" << std::endl;

	std::string str(msg.begin(), msg.end());
	co_await echoClient(str, sock);
}

Proto subprotoClient(u64 t, Socket& sock)
{
	std::vector<char> msg(10);
	for (u64 i = 0; i < msg.size(); ++i)
		msg[i] = 'a' + i;

	co_await sock.send(msg);
	co_await sock.recv(msg);

	std::cout << "subprotoClient calling echoServer()" << std::endl;

	co_await echoServer(sock);
}

void subprotoExample()
{
	std::cout << Color::Green << " ----------- subprotoExample ----------- " << std::endl << Color::Default;
	LocalEvaluator eval;
	auto sockets = eval.getSocketPair();

	auto server = subprotoServer(sockets[0]);
	auto client = subprotoClient(6, sockets[1]);

	error_code ec = eval.execute(server, client);

	if (ec)
		std::cout << "LocalEvaluator returned: " << ec.message() << std::endl;
}

Proto asyncServer(Socket& sock)
{
	u64 i;
	co_await sock.recv(i);
	--i;
	co_await sock.send(i);

	if (i)
	{
		// run two instances of the protocol in parallel.
		// these two recursive calls will be interleaved.
		auto future0 = co_await asyncServer(sock).async();
		auto future1 = co_await asyncServer(sock).async();

		co_await future0;
		co_await future1;
	}
}

Proto asyncClient(u64 i, Socket	& sock)
{
	std::cout << "asyncClient(" << i << ")"<< std::endl;
	co_await sock.send(i);

	// awaiting on EndOfRound tells the scheduler to
	// pause the current protocol if there are other
	// protocols which are ready. This allows us to control
	//exactly how the rounds of protocols are composed.
	co_await EndOfRound{};

	co_await sock.recv(i);

	if (i)
	{
		// run two instances of the protocol in parallel.
		auto future0 = co_await asyncClient(i, sock).async();
		auto future1 = co_await asyncClient(i, sock).async();

		co_await future0;
		co_await future1;
	}
}

void asyncExample()
{
	std::cout << Color::Green << " ----------- asyncExample ----------- " << std::endl << Color::Default;
	LocalEvaluator eval;
	auto sockets = eval.getSocketPair();
	
	auto server = asyncServer(sockets[0]);
	auto client = asyncClient(4, sockets[1]);

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
#else

void cpp20Tutorial()
{}

#endif
