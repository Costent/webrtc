#ifndef PHOTON_HTTP_SERVER_CONNECTION_H
#define PHOTON_HTTP_SERVER_CONNECTION_H

#include <array>
#include <memory>

#include "boost/shared_ptr.hpp"
#include "boost/asio.hpp"

#include "photon/http/server/request.h"
#include "photon/http/server/request_parser.h"
#include "photon/http/server/request_handler.h"

namespace photon {
namespace http {
namespace server {

class connection_manager;

/// Represents a single connection from a client.
class connection
  : public std::enable_shared_from_this<connection>
{
public:
  connection(const connection&) = delete;
  connection& operator=(const connection&) = delete;

  /// Construct a connection with the given socket.
  explicit connection(boost::asio::ip::tcp::socket socket,
      connection_manager& manager, request_handler handler);

  /// Start the first asynchronous operation for the connection.
  void start();

private:
  /// Perform an asynchronous read operation.
  void do_read();

  /// Socket for the connection.
  boost::asio::ip::tcp::socket socket_;

  /// The manager for this connection.
  connection_manager& connection_manager_;

  /// The handler used to process the incoming request.
  request_handler request_handler_;

  /// The incoming request.
  request request_;

  /// Buffer for incoming data.
  std::array<char, 8192> buffer_;

  /// The parser for the incoming request.
  request_parser request_parser_;

};

typedef std::shared_ptr<connection> connection_ptr;

} // namespace server
} // namespace http
} // namespace photon

#endif // PHOTON_HTTP_SERVER_CONNECTION_H
