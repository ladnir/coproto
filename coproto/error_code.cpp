

#include "error_code.h"


namespace { // anonymous namespace

    struct coprotoCodeCategory : std::error_category
    {
        const char* name() const noexcept override
        {
            return "hydra_error_code";
        }

        std::string message(int err) const override
        {
            switch (static_cast<coproto::code>(err))
            {
            case coproto::code::success:
                return "The io operation completed successfully";
            case coproto::code::ioError:
                return "The io operation failed and the caller should abort";
            //case coproto::code::endOfRound:
            //    return "This protocol has reached the end of the round and is being suspended";
            case coproto::code::suspend:
                return "The protocol has been suspended for some reason.";
            case coproto::code::badBufferSize:
                return "Bad buffer size. The reciever's buffer size does not match the number of bytes sent.";
            case coproto::code::bufferResizeNotMultipleOfValueType:
                return  "Data was received over the connection and the receiver buffer was resized. However, the receiver buffer "
                    "value_type is not a multiple of the number of bytes received. The receive buffer is incorrect.";
            case coproto::code::bufferResizedFailed:
                return "Bad buffer size. The reciever's buffer size does not match the number of bytes sent. Resizing the buffer failed for an unkown reason.";
            case coproto::code::noResizeSupport:
                return "Bad buffer size. The reciever's buffer size does not match the number of bytes sent. The numberleying buffer does not support resizing.";
            case coproto::code::sendLengthZeroMsg:
                return "Sending a zero length message is not allowed.";
            case coproto::code::secuirtyViolation:
                return "One of the other parties was caught cheating.";
            case coproto::code::parsingError:
                return "An error occured while paring one of the protocol messages.";
            case coproto::code::protocolError:
                return "An error occured durring the protocol such as a messages being of the wrong size.";
            case coproto::code::uncaughtException:
                return "An uncaught exception was thrown during the protocol. It can be obtained by calling getException() on the protocol object.";
            default:
                return "unknown error_code of type coproto::code";
            }
        }
    };
    const coprotoCodeCategory theCoprotoCodeCategory{};
} // anonymous namespace

namespace coproto
{
    error_code make_error_code(code e)
    {
        auto ee = static_cast<int>(e);
        return { ee, theCoprotoCodeCategory };
    }
}