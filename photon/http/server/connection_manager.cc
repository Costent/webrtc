#include "photon/http/server/connection_manager.h"

namespace photon {
namespace http {
namespace server {

connection_manager::connection_manager()
{
}

void connection_manager::start(connection_ptr c)
{
  connections_.insert(c);
  c->start();
}

void connection_manager::stop(connection_ptr c)
{
  connections_.erase(c);
}

} // namespace server
} // namespace http
} // namespace photon
