#ifndef PHOTON_HTTP_SERVER_REQUEST_H
#define PHOTON_HTTP_SERVER_REQUEST_H

#include <string>
#include <vector>

#include "boost/asio.hpp"

#include "photon/http/server/header.h"

namespace photon {
namespace http {
namespace server {

/// A request received from a client.
struct request
{
  std::string method;
  std::string uri;
  std::string body;
  int http_version_major;
  int http_version_minor;
  std::vector<header> headers;
};

} // namespace server
} // namespace http
} // namespace photon

#endif // PHOTON_HTTP_SERVER_REQUEST_H
