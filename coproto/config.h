#pragma once



#if defined(__has_include) && !defined(_MSC_VER)
	#if __has_include("coproto/configCMake.h")
		#define COPROTO_USE_CMAKE_CONFIG
	#endif
#endif

#if defined(COPROTO_USE_CMAKE_CONFIG)
	#include "coproto/configCMake.h"
#else
	#include "coproto/configDefault.h"
#endif

