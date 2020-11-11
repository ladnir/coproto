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
		// bad buffer size. The reciever's buffer size does not match the number of bytes sent.
		badBufferSize,
		// The operation has been suspended.
		suspend,
		// sending a zero length message is not allowed.
		sendLengthZeroMsg,
		// The requested operation for not support async sockets.
		noAsyncSupport,
		//An uncaught exception was thrown during the protocol. ";
		uncaughtException,
		invalidArguments,
		endOfRound
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