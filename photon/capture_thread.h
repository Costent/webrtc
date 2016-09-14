#ifndef PHOTON_CAPTURE_THREAD_H_
#define PHOTON_CAPTURE_THREAD_H_

#include "webrtc/base/thread.h"

#include "photon/capturer.h"

namespace photon {

class CaptureThread : public rtc::AutoThread,
                      public rtc::MessageHandler {
 private:
  Capturer * capturer_;
  bool finished_;
  bool running_;

 public:
  CaptureThread(Capturer *capturer);
  virtual ~CaptureThread();

 protected:
  virtual void Run();

 public:
  virtual void OnMessage(rtc::Message * msg);
  bool Finished() const;
  bool Running() const;

 private:
  void operator=(const CaptureThread &) = delete;
  CaptureThread(const CaptureThread &) = delete;

};

}  // namespace photon

#endif  // PHOTON_CAPTURE_THREAD_H_
