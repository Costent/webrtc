#ifndef PHOTON_HTTP_SERVER_REQUEST_HANDLER_H
#define PHOTON_HTTP_SERVER_REQUEST_HANDLER_H

#include "boost/shared_ptr.hpp"

#include "photon/http/server/response.h"
#include "photon/http/server/request.h"

namespace photon {
namespace http {
namespace server {

typedef std::function<void (request, boost::shared_ptr<response>) > request_handler;

} // namespace server
} // namespace http
} // namespace photon
#endif
