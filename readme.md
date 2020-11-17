#Coproto
=====

Coproto is a c++11 or c++20 protocol framework based on coroutines. The framework supports a variety of socket types, e.g. blocking or asynchronous, and allows users to write their protocol once and have it optimally evaluated regardless. See the tuoritals in `coprotoTests/`.

  
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
 
