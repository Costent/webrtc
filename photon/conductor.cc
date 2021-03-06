#include <iostream>

#include <memory>
#include <utility>
#include <vector>

#include "webrtc/api/test/fakeconstraints.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"

#include "photon/conductor.h"
#include "photon/defaults.h"

#if defined(WEBRTC_WIN)
#include "photon/capture_thread.h"
#include "photon/capturer.h"
#else
#include "webrtc/media/base/fakevideocapturer.h"
#endif

class DummySetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  static DummySetSessionDescriptionObserver* Create() {
    return
      new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
  }

  virtual void OnSuccess() {
    LOG(INFO) << __FUNCTION__;
  }

  virtual void OnFailure(const std::string& error) {
    LOG(INFO) << __FUNCTION__ << " " << error;
  }

 protected:
  DummySetSessionDescriptionObserver() {}
  ~DummySetSessionDescriptionObserver() {}
};

Conductor::Conductor(const std::string & offer,
  boost::shared_ptr<photon::http::server::response> res)
    : offer_(offer),
      response_(res) {}

Conductor::~Conductor() {
  ASSERT(peer_connection_.get() == NULL);
}

bool Conductor::connection_active() const {
  return peer_connection_.get() != NULL;
}

void Conductor::Close() {
  DeletePeerConnection();
}

bool Conductor::InitializePeerConnection() {
  ASSERT(peer_connection_factory_.get() == NULL);
  ASSERT(peer_connection_.get() == NULL);

  peer_connection_factory_  = webrtc::CreatePeerConnectionFactory();

  if (!peer_connection_factory_.get()) {
    LOG(LERROR) << "Failed to initialize PeerConnectionFactory";
    DeletePeerConnection();
    return false;
  }

  if (!CreatePeerConnection()) {
    LOG(LERROR) << "CreatePeerConnection failed";
    DeletePeerConnection();
  }
  AddStreams();
  return peer_connection_.get() != NULL;
}

bool Conductor::ReinitializePeerConnectionForLoopback() {
  return false;
}

bool Conductor::CreatePeerConnection() {
  ASSERT(peer_connection_factory_.get() != NULL);
  ASSERT(peer_connection_.get() == NULL);

  webrtc::PeerConnectionInterface::RTCConfiguration config;
  webrtc::PeerConnectionInterface::IceServer server;
  server.uri = "stun:stun.l.google.com:19302";
  config.servers.push_back(server);

  webrtc::FakeConstraints constraints;
  constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
      "true");

  peer_connection_ = peer_connection_factory_->CreatePeerConnection(
      config, &constraints, NULL, NULL, this);
  return peer_connection_.get() != NULL;
}

void Conductor::DeletePeerConnection() {
  peer_connection_ = NULL;
  active_streams_.clear();
  peer_connection_factory_ = NULL;
}

//
// DataChannelObserver implementation.
//

// The data channel state have changed.
void Conductor::OnStateChange() {

}

//  A data buffer was successfully received.
void Conductor::OnMessage(const webrtc::DataBuffer& buffer) {
#if defined(WEBRTC_WIN)
  const rtc::CopyOnWriteBuffer * d = &buffer.data;
  const char * buff = d->data<char>();
  std::string mess(buff, d->size());
  int x, y;

  switch (buff[0]) {
    // Mouse Move
    case 'M':
      x = std::stoi(mess.substr(1, mess.find('|') - 1));
      y = std::stoi(mess.substr(mess.find('|') + 1));
      input_manager_.MouseMove(x, y);
      break;

      // Mouse Button
    case 'C':
      input_manager_.MouseButtonEvent(buff[2], buff[1]);
      break;
    case 'W': // Mouse Wheel
      x = std::stoi(mess.substr(1));
      input_manager_.MouseWheelEvent(x);
      break;
      // Keyboard
    case 'D':
      input_manager_.KeyDown(buff[1]);
      break;
    case 'U':
      input_manager_.KeyUp(buff[1]);
      break;
  }
#endif
}

//
// PeerConnectionObserver implementation.
//

// Called when a remote stream is added
void Conductor::OnAddStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
  LOG(INFO) << __FUNCTION__ << " " << stream->label();
}

void Conductor::OnRemoveStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
  LOG(INFO) << __FUNCTION__ << " " << stream->label();
}

void Conductor::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
  LOG(INFO) << __FUNCTION__;

  channel->RegisterObserver(this);
}

void Conductor::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {

  if (new_state == webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringComplete) {
    const webrtc::SessionDescriptionInterface* desc = peer_connection_->local_description();

    std::string sdp;
    desc->ToString(&sdp);

    response_->status(200);
    response_->text(sdp);
  }
}

void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
  LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();
}

void Conductor::ConnectToPeer() {
  ASSERT(!offer_.empty());

  if (!peer_connection_.get()) {
    if (!InitializePeerConnection()) {
      LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
      return;
    }
  }

  webrtc::SdpParseError error;
  webrtc::SessionDescriptionInterface* session_description(
      webrtc::CreateSessionDescription("offer", offer_, &error));
  if (!session_description) {
    LOG(WARNING) << "Can't parse received session description message. "
      << "SdpParseError was: " << error.description;
    return;
  }
  peer_connection_->SetRemoteDescription(
      DummySetSessionDescriptionObserver::Create(), session_description);
  peer_connection_->CreateAnswer(this, NULL);
}

cricket::VideoCapturer* Conductor::OpenVideoCaptureDevice() {
#if defined(WEBRTC_WIN)
  photon::Capturer * capturer = new photon::Capturer();

  photon::CaptureThread * thr = new photon::CaptureThread(capturer);
  thr->Start();
  return capturer;
#else
  return new cricket::FakeVideoCapturer;
#endif
}

void Conductor::AddStreams() {
  if (active_streams_.find(kStreamLabel) != active_streams_.end())
    return;  // Already added.

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      peer_connection_factory_->CreateAudioTrack(
        kAudioLabel, peer_connection_factory_->CreateAudioSource(NULL)));

  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
      peer_connection_factory_->CreateVideoTrack(
        kVideoLabel,
        peer_connection_factory_->CreateVideoSource(OpenVideoCaptureDevice(),
          NULL)));
  rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
    peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);

  stream->AddTrack(audio_track);
  stream->AddTrack(video_track);
  if (!peer_connection_->AddStream(stream)) {
    LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
  }
  typedef std::pair<std::string,
          rtc::scoped_refptr<webrtc::MediaStreamInterface> >
            MediaStreamPair;
  active_streams_.insert(MediaStreamPair(stream->label(), stream));
}

void Conductor::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
  peer_connection_->SetLocalDescription(
      DummySetSessionDescriptionObserver::Create(), desc);

  std::string sdp;
  desc->ToString(&sdp);
}

void Conductor::OnFailure(const std::string& error) {
  LOG(LERROR) << error;
}
