#pragma once


#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING

#include <cstdint>
#include <span>
#include <string>

#define COPRO_STRINGIZE_DETAIL(x) #x
#define COPRO_STRINGIZE(x) COPRO_STRINGIZE_DETAIL(x)
#define COPROTO_LOCATION __FILE__ ":" COPRO_STRINGIZE(__LINE__)

#define COPROTO_LOGGING
#include <atomic>


namespace coproto
{

    inline std::atomic<int> gNewDel_ = 0;

    void regNew(void* ptr, std::string name);
    void regDel(void* ptr);
    std::string regStr();


    typedef uint64_t u64;
    typedef int64_t i64;
    typedef uint32_t u32;
    typedef int32_t i32;
    typedef uint16_t u16;
    typedef int16_t i16;
    typedef uint8_t u8;
    typedef int8_t i8;

    template<typename T> using span = std::span<T>;


    template<typename T>
    class ProtoV;

    std::string hexPtr(void*);

    namespace internal
    {
        template<typename T>
        class ProtoPromise;
        const int inlineSize = 256;
    }

}