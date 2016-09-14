#ifndef PHOTON_HTTP_SERVER_RESPONSE_H
#define PHOTON_HTTP_SERVER_RESPONSE_H

#include <string>
#include <map>

#include "webrtc/base/logging.h"

#include "boost/asio.hpp"

namespace photon {
namespace http {
namespace server {

class response
{
public:
  struct http_status_code_t_ {
    const int code;
    const char *value;
  };

  static const char name_value_separator[];
  static const char http_version_[];
  static const char crlf_[];
  static const http_status_code_t_ http_status_codes_[];

  std::map<std::string, std::string> headers_;
  std::string body_;

  static const char *http_code_label(int code);
  void send_response();

public:
  int status_;
  bool response_sent_;
  boost::asio::ip::tcp::socket socket_;

  response(boost::asio::ip::tcp::socket socket_);

  void header(const std::string & key, const std::string & value);
  void status(int status);
  void text(const std::string & text);
};

} // namespace server
} // namespace http
} // namespace photon

#endif // PHOTON_HTTP_SERVER_RESPONSE_H
