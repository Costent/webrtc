var peerConnection = new RTCPeerConnection({
  iceServers:[{
    urls: ['stun:stun.l.google.com:19302']
  }]
}, {
  optional: [{
    DtlsSrtpKeyAgreement: true
  }]
});

peerConnection.onicecandidate = function(event) {
  var self = this;

  if (event.candidate === null) {
    var sdp = peerConnection.localDescription.sdp;

    var r = new XMLHttpRequest();
    r.open('POST', '/webrtc', true);
    r.onreadystatechange = function () {
      if (r.readyState !== 4 || r.status !== 200) {
        return;
      }

      var answer = r.responseText;

      peerConnection.setRemoteDescription(new RTCSessionDescription({
        type: 'answer',
        sdp: answer
      }));
    };
    r.send(sdp);
  }
};

peerConnection.onaddstream = function(event) {
  var url = URL.createObjectURL(event.stream);
  var vid = document.createElement('video');
  vid.src = url;
  vid.autoplay = true;

  document.body.appendChild(vid);
};

var dataChannel = peerConnection.createDataChannel('userinput', {
  ordered: true,
  reliable: true
});

dataChannel.onopen = function() {
  console.log('data channel open');
  window.dc = dataChannel;
};

peerConnection.createOffer(function(offer) {
  peerConnection.setLocalDescription(offer);
}, function(err) {
  console.error(err);
}, {
  offerToReceiveAudio: true,
  offerToReceiveVideo: true
});
