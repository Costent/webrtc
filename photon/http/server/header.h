#ifndef PHOTON_HTTP_SERVER_HEADER_H
#define PHOTON_HTTP_SERVER_HEADER_H

#include <string>

namespace photon {
namespace http {
namespace server {

struct header
{
  std::string name;
  std::string value;
};

} // namespace server
} // namespace http
} // namespace photon

#endif // PHOTON_HTTP_SERVER_HEADER_H
