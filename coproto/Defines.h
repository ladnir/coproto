#pragma once

#include "config.h"
#include <cstdint>
//#include <string>

#define COPRO_STRINGIZE_DETAIL(x) #x
#define COPRO_STRINGIZE(x) COPRO_STRINGIZE_DETAIL(x)
#define COPROTO_LOCATION __FILE__ ":" COPRO_STRINGIZE(__LINE__)



namespace coproto
{



#ifdef ALLOC_TEST
    void regNew_(void* ptr, std::string name);
    void regDel_(void* ptr);
    std::string regStr();
#define COPROTO_REG_NEW(p, n) regNew_(p,n)
#define COPROTO_REG_DEL(p) regDel_(p)
extern u64 mNewIdx;
#else
#define COPROTO_REG_NEW(p, n) 
#define COPROTO_REG_DEL(p)
#endif

    typedef uint64_t u64;
    typedef int64_t i64;
    typedef uint32_t u32;
    typedef int32_t i32;
    typedef uint16_t u16;
    typedef int16_t i16;
    typedef uint8_t u8;
    typedef int8_t i8;


    template<typename T>
    class ProtoV;

    //std::string hexPtr(void*);

    namespace internal
    {
        template<typename T>
        class ProtoPromise;
        const int inlineSize = 256;
    }

}