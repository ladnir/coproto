#pragma once
#define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING

#include <cstdint>
#include <cryptoTools/Common/Defines.h>

namespace coproto
{

    typedef uint64_t u64;
    typedef int64_t i64;
    typedef uint32_t u32;
    typedef int32_t i32;
    typedef uint16_t u16;
    typedef int16_t i16;
    typedef uint8_t u8;
    typedef int8_t i8;

    template<typename Interface> using span = gsl::span<Interface>;

}