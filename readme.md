#Coproto
=====

Coproto is a c++11 or c++20 protocol framework based on coroutines. The framework supports a variety of socket types, e.g. blocking or asynchronous, and allows users to write their protocol once and have it optimally evaluated regardless. See the tuoritals in `coprotoTests/`.

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

    // or execute a single protocol as
    Socket sock = ...;
    client.evaluate(sock);
}
```

C++11 Echo server example:
```cpp
    coproto::Proto echoClient(std::string message) {
        struct Impl : coproto::NativeProto {
            std::string message;
            Impl(std::string& s) :message(s) {}

            coproto::error_code resume() override {
                CP_BEGIN();
                CP_AWAIT(coproto::send(std::move(message)));
                CP_AWAIT(coproto::recvResize(message));
                CP_END();
                return{};
            }
        };
        return coproto::makeProto<Impl>(message);
    }

    coproto::Proto echoServer() {
        struct Impl : public coproto::NativeProto {
            std::string message;
            coproto::error_code resume() override {
                CP_BEGIN();
                CP_AWAIT(coproto::recvResize(message));
                CP_AWAIT(coproto::send(message));
                CP_END();
                return{};
            }
        };
        return coproto::makeProto<Impl>();
    }
```


### Unix
To build the libary with c++ coroutine support pass `-DENABLE_CPP20=ON` and otherwise `-DENABLE_CPP20=OFF` for c++11.
```
cmake . -DENABLE_CPP20=ON
```

The unit tests can be run by calling `./bin/coprotoTests -u` and the tutorials with `./bin/coprotoTests`. 

### Windows

Open visual studio solution. Pass program argument `-u` to run tests. Define `DENABLE_CPP20` in `coproto/configDefault.h` if desired and set the c++ standard accordingly.


 ## License
 
This project has been placed in the public domain. As such, you are unrestricted in how you use it, commercial or otherwise. However, no warranty of fitness is provided. If you found this project helpful, feel free to spread the word and cite us.
 

## Help
 
Contact Peter Rindal peterrindal@gmail.com for any assistance on building or running the library.
 
