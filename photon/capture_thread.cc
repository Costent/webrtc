#include "photon/capture_thread.h"
#include "photon/capturer.h"

namespace photon {

CaptureThread::CaptureThread(Capturer *capturer)
    : capturer_(capturer),
      finished_(false),
      running_(false) {}

CaptureThread::~CaptureThread() {}

void CaptureThread::Run() {
  int start, end;
  running_ = true;
  for (;;) {
    start = rtc::TimeNanos();
    capturer_->CaptureFrame();
    end = rtc::TimeNanos();
    SleepMs(10);
  }
  running_ = false;
  finished_ = true;
}

void CaptureThread::OnMessage(rtc::Message * msg) {}

bool CaptureThread::Finished() const {
  return finished_;
}

bool CaptureThread::Running() const {
  return running_;
}

}  // namespace photon
