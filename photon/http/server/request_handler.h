#ifndef PHOTON_HTTP_SERVER_REQUEST_HANDLER_H
#define PHOTON_HTTP_SERVER_REQUEST_HANDLER_H

namespace photon {
namespace http {
namespace server {

struct request;
struct reply;

typedef std::function<void (const request&, reply&) > request_handler;

} // namespace server
} // namespace http
} // namespace photon
#endif
