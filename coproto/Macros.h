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
			this->_resume_idx_ = __LINE__;		\
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
		this->_resume_idx_ = __LINE__;				\
		return ec;						\
	}									\
}										\
COPROTO_FALLTHROUGH;case __LINE__:			\
res = std::move(*(typename decltype(X)::return_type*)(this->getAwaitReturn()));\
}while(0)

#define CP_SEND(s, X) CP_AWAIT(s.send(X))
#define CP_RECV(s, X) CP_AWAIT(s.recv(X))
#define CP_SEND_EC(res,s, X) CP_AWAIT_VAL(res, s.send(X).wrap())
#define CP_RECV_EC(res,s, X) CP_AWAIT_VAL(res, s.recv(X).wrap())
#define CP_END_OF_ROUND() CP_AWAIT(::coproto::EndOfRound{})		

#define CP_BEGIN()					\
	switch(this->_resume_idx_)					\
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
