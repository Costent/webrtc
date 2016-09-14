#include "photon/http/server/connection.h"

#include <utility>
#include <vector>

#include "photon/http/server/request_handler.h"
#include "photon/http/server/connection_manager.h"

namespace photon {
namespace http {
namespace server {

connection::connection(boost::asio::ip::tcp::socket socket,
    connection_manager& manager, request_handler handler)
  : socket_(std::move(socket)),
    connection_manager_(manager),
    request_handler_(handler)
{
}

void connection::start()
{
  do_read();
}

void connection::do_read()
{
  auto self(shared_from_this());
  socket_.async_read_some(boost::asio::buffer(buffer_),
      [this, self](boost::system::error_code ec, std::size_t bytes_transferred)
      {
        if (!ec)
        {
          request_parser::result_type result;
          std::tie(result, std::ignore) = request_parser_.parse(
              request_, buffer_.data(), buffer_.data() + bytes_transferred);

          if (result == request_parser::good)
          {

            boost::shared_ptr<response> res(new response(std::move(socket_)));
            request_handler_(std::move(request_), res);

            // do_write();
          }
          else if (result == request_parser::bad)
          {
            // reply_ = reply::stock_reply(reply::bad_request);
            // do_write();
          }
          else
          {
            do_read();
          }
        }
        else if (ec != boost::asio::error::operation_aborted)
        {
          connection_manager_.stop(shared_from_this());
        }
      });
}

} // namespace server
} // namespace http
} // namespace photon
