#pragma once

#include "coproto/config.h"


#ifdef COPROTO_CPP20
#define COPROTO_FALLTHROUGH [[fallthrough]]
#else
#define COPROTO_FALLTHROUGH
#endif


#define CP_AWAIT(X)					\
	do{{								\
		auto ec = this->await(X);			\
		if (ec)						\
		{							\
			this->mState = __LINE__;		\
			return ec;				\
		}							\
	}								\
	COPROTO_FALLTHROUGH;case __LINE__: do{}while(0);	\
	}while(0)


#define CP_AWAIT_VAL(res, X)			\
do{{										\
	auto ec = this->await(X);					\
	if (ec)								\
	{									\
		this->mState = __LINE__;				\
		return ec;						\
	}									\
}										\
COPROTO_FALLTHROUGH;case __LINE__:			\
res = std::move(*(typename decltype(X)::return_type*)(this->getAwaitReturn()));\
}while(0)

#define CP_SEND(X) CP_AWAIT(::coproto::send(X))
#define CP_RECV(X) CP_AWAIT(::coproto::recv(X))
#define CP_SEND_EC(res,X) CP_AWAIT_VAL(res, ::coproto::send(X).wrap())
#define CP_RECV_EC(res,X) CP_AWAIT_VAL(res, ::coproto::recv(X).wrap())
#define CP_END_OF_ROUND() CP_AWAIT(::coproto::EndOfRound{})		

#define CP_BEGIN()					\
	switch(mState)					\
	{								\
	case 0:							\
		do {}while(0)

#define CP_END()					\
		break;						\
	default:						\
		break;						\
	}								\
	do {}while(0)


#define CP_RETURN(x)				\
	return_value(x);				\
	return {}
