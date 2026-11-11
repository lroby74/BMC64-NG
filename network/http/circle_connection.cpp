#include "circle_connection.h"

#include <errno.h>
namespace bmx {
namespace network {
namespace http {
namespace {
io::IoResult Error() { return {io::IoStatus::Error, 0, EINVAL}; }
}
CircleConnection::CircleConnection(CNetSubSystem* network, const tls::ClientOptions* options)
    : network_(network) {
    if (options && options->authorities && options->authority_count &&
        options->authority_count <= 16) {
        options_ = *options;
        for (size_t i = 0; i < options->authority_count; ++i)
            authorities_[i] = options->authorities[i];
        options_.authorities = authorities_;
        has_options_ = true;
    }
}
CircleConnection::~CircleConnection() { Close(); }
void CircleConnection::Close() {
    delete plain_;
    plain_ = 0;
    delete secure_;
    secure_ = 0;
}
io::IoResult CircleConnection::Begin(const Url& url, const io::OperationContext& context) {
    Close();
    io::IoResult checked = io::CheckOperation(context);
    if (checked.status != io::IoStatus::Progress) return checked;
    if (url.secure) {
        if (!has_options_) return Error();
        secure_ = tls::CreateNetworkTlsStream(network_);
        return secure_ ? secure_->Begin(url.host, url.port, options_, context) : Error();
    }
    plain_ = io::CreateNetworkTcpStream();
    return plain_ ? plain_->BeginConnect(url.host, url.port, context) : Error();
}
io::IoResult CircleConnection::PollConnect(const io::OperationContext& context) {
    return secure_  ? secure_->PollConnect(context)
           : plain_ ? plain_->PollConnect(context)
                    : Error();
}
io::IoResult CircleConnection::ReadSome(void* data, size_t size,
                                        const io::OperationContext& context) {
    return secure_  ? secure_->ReadSome(data, size, context)
           : plain_ ? plain_->ReadSome(data, size, context)
                    : Error();
}
io::IoResult CircleConnection::WriteSome(const void* data, size_t size,
                                         const io::OperationContext& context) {
    return secure_  ? secure_->WriteSome(data, size, context)
           : plain_ ? plain_->WriteSome(data, size, context)
                    : Error();
}
}
}
}
