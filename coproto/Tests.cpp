#define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING

#include "Tests.h"

#include "coproto.h"
#include "Buffers.h"
#include "Proto.h"

namespace coproto
{

	oc::TestCollection testCollection([](oc::TestCollection& t) {
		
		t.add("SmallBufferTest                 ", tests::SmallBufferTest);
		t.add("strSendRecvTest                 ", tests::strSendRecvTest); 
		t.add("resultSendRecvTest              ", tests::resultSendRecvTest);
		t.add("typedRecvTest                   ", tests::typedRecvTest);
		
		t.add("zeroSendRecvTest                ", tests::zeroSendRecvTest);
		t.add("badRecvSizeTest                 ", tests::badRecvSizeTest);
		t.add("zeroSendErrorCodeTest           ", tests::zeroSendErrorCodeTest);
		t.add("badRecvSizeErrorCodeTest        ", tests::badRecvSizeErrorCodeTest);
		t.add("throwsTest                      ", tests::throwsTest);

		t.add("nestedSendRecvTest              ", tests::nestedSendRecvTest);
		t.add("nestedProtocolThrowTest         ", tests::nestedProtocolThrowTest);
		t.add("nestedProtocolErrorCodeTest     ", tests::nestedProtocolErrorCodeTest);
		t.add("asyncProtocolTest               ", tests::asyncProtocolTest);
		t.add("asyncThrowProtocolTest          ", tests::asyncThrowProtocolTest);
		
		t.add("v1::intSendRecvTest             ", v1::tests::intSendRecvTest);
		t.add("v1::arraySendRecvTest           ", v1::tests::arraySendRecvTest);
		t.add("v1::basicSendRecvTest           ", v1::tests::strSendRecvTest);
		t.add("v1::resizeSendRecvTest          ", v1::tests::resizeSendRecvTest);
		t.add("v1::moveSendRecvTest            ", v1::tests::moveSendRecvTest);
		t.add("v1::typedRecvTest               ", v1::tests::typedRecvTest);
		t.add("v1::nestedProtocolTest          ", v1::tests::nestedProtocolTest);
		t.add("v1::zeroSendRecvTest            ", v1::tests::zeroSendRecvTest);
		t.add("v1::zeroSendErrorCodeTest       ", v1::tests::zeroSendErrorCodeTest);
		t.add("v1::badRecvSizeTest             ", v1::tests::badRecvSizeTest);
		t.add("v1::badRecvSizeErrorCodeTest    ", v1::tests::badRecvSizeErrorCodeTest);
		t.add("v1::throwsTest                  ", v1::tests::throwsTest);
		t.add("v1::nestedProtocolThrowTest     ", v1::tests::nestedProtocolThrowTest);
		t.add("v1::nestedProtocolErrorCodeTest ", v1::tests::nestedProtocolErrorCodeTest);

		}); 
}