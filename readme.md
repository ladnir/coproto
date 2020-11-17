Coproto
=====

Coproto is a C++11 or C++20 "bring your own socket" protocol framework. The goal of the library to allow you to write your protocol once and run it in any evironment. In particular, protocols can be written in a synchronous manner while allowing their executions to be asynchronous. This is acheived through a "coroutine" abstraction layer which allows users to write protocol in a natural and composable manner. With C++20, users can laverage the coroutine language support to have highly readable code. To enable backwards compatibility users can also use the C++11 compatibility layer.

See the tutoritals [C++20](https://github.com/ladnir/coproto/blob/master/coprotoTests/cpp20Tutorial.cpp), [C++11](https://github.com/ladnir/coproto/blob/master/coprotoTests/cpp11Tutorial.cpp), [Custom Socket](https://github.com/ladnir/coproto/blob/master/coprotoTests/cpp20Tutorial.cpp).

C++20 Echo server example:
```cpp
Proto echoClient(std::string message) {
    co_await send(message);
    co_await recv(message);
}
Proto echoServer() {
    std::string message;
    co_await recv(message);
    co_await send(message);
}

void echoExample() {
    // lazily construct protocol objects
    Proto server = echoServer();
    Proto client = echoClient("hello world");

    // Execute the protocol. eval will run both parts.
    LocalEvaluator eval;
    eval.execute(server, client);

    // or execute a single protocol with your socket
    Socket sock = ...;
    client.evaluate(sock);
}
```

C++11 Echo server example:
```cpp
    Proto echoClient(std::string message) {
        struct Impl : NativeProto {
            std::string message;
            Impl(std::string& s) :message(s) {}
            error_code resume() override {
                CP_BEGIN();
                CP_AWAIT(send(message));
                CP_AWAIT(recv(message));
                CP_END();
                return{};
            }
        };
        return makeProto<Impl>(message);
    }

    Proto echoServer() {
        struct Impl : public NativeProto {
            std::string message;
            error_code resume() override {
                CP_BEGIN();
                CP_AWAIT(recv(message));
                CP_AWAIT(send(message));
                CP_END();
                return{};
            }
        };
        return makeProto<Impl>();
    }
```


### Unix
To build the libary with c++20 coroutine support pass `-DENABLE_CPP20=ON` and otherwise `-DENABLE_CPP20=OFF` for c++11.
```
cmake . -DENABLE_CPP20=ON
```

The unit tests can be run by calling `./bin/coprotoTests -u` and the tutorials with `./bin/coprotoTests`. 

### Windows

Open visual studio solution. Pass program argument `-u` to run tests. Define `DENABLE_CPP20` in `coproto/configDefault.h` as desired and set the c++ standard accordingly.


 ## License
 
This project has been placed in the public domain. As such, you are unrestricted in how you use it, commercial or otherwise. However, no warranty of fitness is provided. If you found this project helpful, feel free to spread the word and cite us.
 

## Help
 
Contact Peter Rindal peterrindal@gmail.com for any assistance on building or running the library.
 
