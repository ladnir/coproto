#define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING

#include "Tests.h"

#include "coproto.h"
#include "Buffers.h"

namespace coproto
{

	oc::TestCollection testCollection([](oc::TestCollection& t) {
		
		t.add("SmallBufferTest             ", tests::SmallBufferTest);
		t.add("intSendRecvTest             ", tests::intSendRecvTest);
		t.add("arraySendRecvTest           ", tests::arraySendRecvTest);
		t.add("basicSendRecvTest           ", tests::strSendRecvTest);
		t.add("resizeSendRecvTest          ", tests::resizeSendRecvTest);
		t.add("moveSendRecvTest            ", tests::moveSendRecvTest);
		t.add("typedRecvTest               ", tests::typedRecvTest);
		t.add("nestedProtocolTest          ", tests::nestedProtocolTest);

		t.add("zeroSendRecvTest            ", tests::zeroSendRecvTest);
		t.add("zeroSendErrorCodeTest       ", tests::zeroSendErrorCodeTest);
		t.add("badRecvSizeTest             ", tests::badRecvSizeTest);
		t.add("badRecvSizeErrorCodeTest    ", tests::badRecvSizeErrorCodeTest);
		t.add("throwsTest                  ", tests::throwsTest);

		t.add("nestedProtocolThrowTest     ", tests::nestedProtocolThrowTest);

		});
}