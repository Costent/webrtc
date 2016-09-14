#ifndef PHOTON_CAPTURER_H_
#define PHOTON_CAPTURER_H_

#include <iostream>
#include <string.h>

#include <memory>
#include <vector>

#include "webrtc/base/timeutils.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videocommon.h"
#include "webrtc/media/base/videoframe.h"
#include "webrtc/media/engine/webrtcvideoframefactory.h"

#include "webrtc/modules/desktop_capture/win/screen_capturer_win_directx.h"

namespace photon {

// Fake video capturer that allows the test to manually pump in frames.
class Capturer
    : public cricket::VideoCapturer,
      public webrtc::DesktopCapturer::Callback {
 public:
  sigslot::signal1<Capturer*> SignalDestroyed;

  Capturer();
  ~Capturer();

  void ResetSupportedFormats(const std::vector<cricket::VideoFormat>& formats);
  bool CaptureFrame();
  cricket::CaptureState Start(const cricket::VideoFormat& format) override;
  void Stop() override;
  bool IsRunning() override;
  bool IsScreencast() const override;
  bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override;
  virtual void OnCaptureResult(webrtc::DesktopCapturer::Result result,
      std::unique_ptr<webrtc::DesktopFrame> desktop_frame) override;

 private:
  bool running_;
  int64_t initial_timestamp_;
  int64_t next_timestamp_;
  webrtc::ScreenCapturerWinDirectx * capturer_;
};

}  // namespace photon

#endif  // PHOTON_CAPTURER_H_
