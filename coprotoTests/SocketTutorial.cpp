#include "SocketTutorial.h"
#include "coproto/coproto.h"
#include "coproto/Tests.h"
#include <string>
#include "ExampleProtocols.h"

using namespace coproto;

namespace {
	// coproto is designed to directly support
	// a wide variety of networking environments.
	// It does not come with any propper networking
	// and instead just has several example sockets
	// which work within a single process. It is expected
	// that users of the library will provide their own
	// networking library which coproto will integrate 
	// with. 
	//
	// There are loosely three types of sockets that 
	// are supported:
	//
	// - Blocking socket: this most basic socket types
	//   will block when data is being sent or received.
	//   coproto supports this use case and will for the
	//   most part execute the protocol in a blocking 
	//   manner. Each co_await call will block until it 
	//   completes with the exception of when async() and
	//   EndOfRound{} is used in which the concurrent protocols
	//   will be interleaved as specified.
	//
	// - Nonblocking synchronous socket: This socket type
	//   has the same interface as a blocking socket
	//   but can return an error_code which tells coproto
	//   to retry the operation later. More on this type below.
	//
	// - Asynchronous socket: this socket type allows for high
	//   performance asynchronous networking where the protocol
	//   is executed via callbacks whenever some networking 
	//   operation completes. Concurrent network send and
	//   receive operations can be performed. More on this below.


	//////////////////////////////////////////////////////////////
	//                   										//
	//                   Blocking Socket						//
	//                   										//
	//////////////////////////////////////////////////////////////

	// Lets create a blocking socket. The socket interface 
	// requires us to implement three methods: send, receive, 
	// cancel. This implementation will be quite simple, we
	// will store a pointer to the other socket and push data
	// into a blocking queue. When we need to receive data we
	// will block until the queue has data.
	//
	// As a user of coproto its likely your socket has a similar
	// interface. Your task will be to implement these methods
	// in terms of your socket type as opposed to emulating it
	// as ive done here.
	struct MyBlockingSocket : public Socket
	{

		// We will take a pointer to the other socket.
		MyBlockingSocket* mOther = nullptr;

		// a flag to indicate if we have called cancel 
		// on this socket.
		error_code mErrorStatus = {};

		// a blocking queue which will hold the data 
		// that is being sent to this socket.
		BlockingQueue<std::pair<error_code, std::vector<u8>>> mInbound;

		error_code recv(span<u8> dest) override {
			// fail of this socket has been canceled.
			if (mErrorStatus)
				return mErrorStatus;

			// pop the next piece of data.
			auto op = mInbound.pop();

			error_code c = op.first;
			if (c) {
				// the other party has canceled their socket.
				cancel(code::remoteCancel);
				return mErrorStatus;
			}
			else {
				std::vector<u8>& inData = op.second;

				// is so happens that coproto always reads
				// chunks of data which are the same size as 
				// they were written.
				assert(inData.size() == dest.size());

				// copy the data 
				std::copy(inData.begin(), inData.end(), dest.begin());
				return {};
			}
		}

		error_code send(span<u8> data) override {
			// fail of this socket has been canceled.
			if (mErrorStatus)
				return mErrorStatus;

			// write the data to the other party.
			mOther->mInbound.emplace(std::make_pair(
				error_code{},
				std::vector<u8>{data.begin(), data.end()}));

			return {};
		}

		void cancel() override {
			// record that it was this end of the socket
			// which cancel was called, as opposed to mOther.
			cancel(code::cancel);
		}

		void cancel(error_code ec) {
			assert(ec);
			// record the error
			mErrorStatus = ec;

			// notify the other party.
			mOther->mInbound.emplace(std::make_pair(ec, std::vector<u8>{}));
		}

	};



	void blockingSocketExample()
	{
		std::cout << Color::Green << " ----------- blocking Socket Example ----------- " << std::endl << Color::Default;

		// If the user wishes to use a blocking socket,
		// then they will need to wrap there socket
		// type in something that implements the 
		// coproto::Socket interface. An example of a 
		// blocking socket which has this interface is
		// MyBlockingSocket shown above.

		// Lets create two of these sockets which are 
		// connected to eachother.

		MyBlockingSocket sock0;
		MyBlockingSocket sock1;

		// connect them.
		sock0.mOther = &sock1;
		sock1.mOther = &sock0;

		// These sockets are then ready to use.

		// Lets create a protocol pair to evaluate.
		Proto client = examples::echoClient("hello world");
		Proto server = examples::echoServer();

		// now we need to invoke the protocols on 
		// the provided sockets. Sinces these sockets
		// are blocking we will need to create a seperate
		// thread.

		auto thread = std::thread([&]() {

			// we invoke the protocol by calling evaluate
			// with the desired socket.
			error_code ec = client.evaluate(sock0);
			if (ec)
				std::cout << "client failed: " << ec.message() << std::endl;
			});

		// invoke the server with the other socket.
		error_code ec = server.evaluate(sock1);
		if (ec)
			std::cout << "server failed: " << ec.message() << std::endl;

		// join the other thread.
		thread.join();
	}


	//////////////////////////////////////////////////////////////
	//                   										//
	//          Non Blocking Synchronous Socket					//
	//                   										//
	//////////////////////////////////////////////////////////////

	// The second type of socket allows for the protocol to be
	// evalutated in a non-blocking manner while still not needing 
	// to deal with callbacks.
	//
	// The only difference this socket has with the blocking
	// variant is that it returns a code::suspend error_code when the
	// data is not avaliable as opposed to blocking. If you do
	// return code::suspend, then coproto will later call the
	// same function with the same buffer.
	//
	// Users may choose to implement this type of socket if
	// they might need to suspend the protocol half way through 
	// evaluating it in order to perform some other operations, e.g.
	// return to java or python and actually send the data.
	struct MyNonblockingSocket : Socket
	{
		MyNonblockingSocket* mOther = nullptr;
		error_code mErrorStatus = {};
		std::queue<std::pair<error_code, std::vector<u8>>> mInbound;

		error_code recv(span<u8> dest) override {
			if (mErrorStatus)
				return mErrorStatus;

			// if we dont have data, then return the suspend code.
			if (mInbound.size() == 0)
				return code::suspend;

			// pop the next piece of data.
			auto op = std::move(mInbound.front());
			mInbound.pop();

			error_code c = op.first;
			if (c) {
				cancel(code::remoteCancel);
				return mErrorStatus;
			}
			else {
				std::vector<u8>& inData = op.second;
				std::copy(inData.begin(), inData.end(), dest.begin());
				return {};
			}
		}

		error_code send(span<u8> data) override {
			if (mErrorStatus)
				return mErrorStatus;

			mOther->mInbound.emplace(std::make_pair(
				error_code{},
				std::vector<u8>{data.begin(), data.end()}));
			return {};
		}

		void cancel() override {
			cancel(code::cancel);
		}

		void cancel(error_code ec) {
			mErrorStatus = ec;
			mOther->mInbound.emplace(std::make_pair(ec, std::vector<u8>{}));
		}
	};



	void nonblockingSocketExample()
	{
		std::cout << Color::Green << " ----------- non blocking Socket Example ----------- " << std::endl << Color::Default;

		MyNonblockingSocket sock0;
		MyNonblockingSocket sock1;
		sock0.mOther = &sock1;
		sock1.mOther = &sock0;

		// These sockets are then ready to use.

		// Lets create a protocol pair to evaluate.
		Proto client = examples::echoClient("hello world");
		Proto server = examples::echoServer();

		// now, since our sockets dont block we do
		// not need to create a thread. Instead we
		// will take turns calling each protocol
		// until they complete.

		error_code
			clientEC = code::suspend,
			serverEC = code::suspend;

		while (
			clientEC == code::suspend ||
			serverEC == code::suspend)
		{
			if (clientEC == code::suspend)
				clientEC = client.evaluate(sock0);
			if (serverEC == code::suspend)
				serverEC = server.evaluate(sock1);
		}

		// we can now check if either failed.
		if (clientEC)
			std::cout << "client failed: " << clientEC.message() << std::endl;
		if (serverEC)
			std::cout << "server failed: " << serverEC.message() << std::endl;
	}


	//////////////////////////////////////////////////////////////
	//                   										//
	//                     Aynchronous Socket					//
	//                   										//
	//////////////////////////////////////////////////////////////
	
	// Asynchronous sockets can allow for greater performance
	// but come at the expense of increased complexity. In order 
	// to get the best of both world we provide a seperate interface
	// for asynchronous sockets.

	// Implementing an async socket is a bit more complicated 
	// as as such we will just review the interface and not provide
	// an exactual example implmentation. See LocalEvaluator for an 
	// example.

	struct MyAsyncSocket : public AsyncSocket
	{
		// when data should be received the recv(...) function
		// will be called. When this operation completes, the 
		// Continutation function object should be called
		// with an error_code and the number of bytes that has 
		// been transmitted. If an error occurs, then the error_code
		// should reflect this. Only one recv operation will be 
		// performed at a time. However, send and recv operations
		// will be performed concurrently.
		void recv(span<u8> data, Continutation&& cont) {};

		// Same situation as recv(...).
		void send(span<u8> data, Continutation&& cont) {};

		// If something in the protocol goes wrong, cancel 
		// might be called. The socket should stop doing any
		// current send/recv operations and call the continuation.
		void cancel() {};
	};

	void asyncSocketExample()
	{
		std::cout << Color::Green << " ----------- Async Socket Example ----------- " << std::endl << Color::Default;

		// in this example we will use the async socket provided
		// by the LocalEvaluator.
		using ASocket = LocalEvaluator::AsyncSock;

		ASocket sock0;
		ASocket sock1;

		// setup the async sockets. ioWorker stores state for the
		// various async operations.
		ASocket::Worker ioWorker;
		sock0.mIdx = 0;
		sock1.mIdx = 1;
		sock0.mWorker = &ioWorker;
		sock1.mWorker = &ioWorker;

		// These sockets are then ready to use.

		// Lets create a protocol pair to evaluate.
		Proto client = examples::echoClient("hello world");
		Proto server = examples::echoServer();

		// One addition parameter async sockets requires is an executor.
		// An executor controls where the protocol is computed. Since
		// its async it need not be on the current thread. One requirement
		// of the executor is that only one operation is performed at any given
		// time.

		// In this example we will use the ThreadExecutor. This executor
		// is implemented by having one thread execute functions that are
		// pushed to a work queue. However, other libraries have other
		// methods of acheiving a similar result such as boost::asio::strand.
		ThreadExecutor executor;

		client.evaluate(sock0, [&](error_code ec) {
			// when the async protocol completes, this function will be called
			std::cout << "client finished with ec = " << ec.message() << std::endl;

			}, executor);

		server.evaluate(sock1, [&](error_code ec) {
			// when the async protocol completes, this function will be called
			std::cout << "server finished with ec = " << ec.message() << std::endl;

			}, executor);

		// However, despite calling evaluate, no work has actually 
		// been performed. To Acheive this, we have to give the 
		// executor a thread to run on. This could be the current
		// thread or some other. Lets use the current thread.

		executor.run();

		std::cout << "the executor is done. " << std::endl;

	}
}

void SocketTutorial()
{

	blockingSocketExample();
	nonblockingSocketExample();
	asyncSocketExample();
}
