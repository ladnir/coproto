#pragma once


#include  <system_error>



namespace coproto
{
	enum class code
	{
		//The io operation completed successfully;
		success = 0,
		//The io operation failed and the caller should abort;
		ioError,
		// message has yet to arrive
		//noMessageAvailable,
		// bad buffer size. The reciever's buffer size does not match the number of bytes sent.
		badBufferSize,
		// data was received over the connection and the receiver buffer was resized. However, the receive buffer
		// value_type is not a multiple of the number of bytes received. The receive buffer is incorrect.
		bufferResizeNotMultipleOfValueType,
		// resizing the buffer failed for an unkown reason.
		bufferResizedFailed,
		// the numberleying buffer does not support resizing
		noResizeSupport,
		// The protocol has been suspended for some reason.

		//endOfRound,

		suspend,
		//endOfRoundFlag,
		// sending a zero length message is not allowed.
		sendLengthZeroMsg,
		// One of the other parties was caught cheating
		secuirtyViolation,
		//An error occured while paring one of the protocol messages.
		parsingError,
		//An error occured durring the protocol such as a messages being of the wrong size.";
		protocolError,
		//An uncaught exception was thrown during the protocol. ";
		uncaughtException
	};
}
namespace std {
	template <>
	struct is_error_code_enum<coproto::code> : true_type {};
}

namespace coproto
{
	using error_code = std::error_code;


	static const std::error_category& coproto_cat = error_code{ code{} }.category();

	error_code make_error_code(code e);

}