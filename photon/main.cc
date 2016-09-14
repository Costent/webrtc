#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"

#include "boost/asio.hpp"
#include "boost/thread/thread.hpp"

#include "photon/conductor.h"
#include "photon/http/server/server.h"

namespace boost {
  void throw_exception(std::exception const & e) {
    std::cerr << e.what() << std::endl;
  }
}

class ConductorThr
  : public rtc::Runnable {

private:
  std::string offer_;
  rtc::scoped_refptr<Conductor> conductor_;
  rtc::Thread thr_;

public:
  ConductorThr()
  {
    thr_.Start(this);
  }

  virtual ~ConductorThr() {

  }

  void ConnectToPeer(const std::string & offer, boost::shared_ptr<photon::http::server::response> res) {
        conductor_ = new rtc::RefCountedObject<Conductor>(offer, res);

        thr_.Invoke<void>(RTC_FROM_HERE, rtc::Bind(&Conductor::ConnectToPeer, conductor_));
  }

  virtual void Run(rtc::Thread* thread) {
    thr_.ProcessMessages(rtc::ThreadManager::kForever);
  }
};

class Photon {

private:
  ConductorThr conductor_thr_;

public:
  void request_handler(photon::http::server::request req, boost::shared_ptr<photon::http::server::response> res) {
    if (!req.method.compare("POST") && !req.uri.compare("/webrtc")) {

      conductor_thr_.ConnectToPeer(req.body, res);
    } else {
      res->status(404);
      res->text("Not Found");
    }
  }
};

int main(int argc, char *argv[]) {
  rtc::InitializeSSL();

  Photon p;

  photon::http::server::server s(8888,
      std::bind(&Photon::request_handler,
                &p,
                std::placeholders::_1,
                std::placeholders::_2));

  s.run();
  rtc::CleanupSSL();

  return 0;
}
