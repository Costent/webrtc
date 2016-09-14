'use strict';

/* jshint browser: true */
/* globals RTCPeerConnection, RTCSessionDescription */

/*
 * KeyboardManager
 */
var KeyboardManager;

(function() {
  KeyboardManager = function(element, rtcChannel) {
    this._element = element;
    this._rtcChannel = rtcChannel;

    var keyevent = function(event) {
      event.preventDefault();
      console.log(event);
      rtcChannel.send(((event.type === 'keydown') ? 'D':'U') + String.fromCharCode(event.keyCode));
    };

    element.addEventListener('keydown', keyevent);
    element.addEventListener('keyup', keyevent);
  };
})();

/*
 * MouseManager
 */
var MouseManager;

(function() {
  MouseManager = function(element, rtcChannel) {
    this._element = element;
    this._rtcChannel = rtcChannel;

    var mouseButtonEvent = function(event) {
      event.preventDefault();

      var mess = 'C';

      if (event.type === 'mousedown') {
        mess += 'D';
      } else {
        mess += 'U';
      }

      if (event.button === 0) { // Left button
        mess += 'L';
      } else { // Right button
        mess += 'R';
      }

      rtcChannel.send(mess);
    };

    element.addEventListener('contextmenu', function(event) {
      event.preventDefault();
    });

    element.addEventListener('mousedown', mouseButtonEvent);
    element.addEventListener('mouseup', mouseButtonEvent);
    element.addEventListener('mousewheel', function(event) {
      event.preventDefault();
      rtcChannel.send('W' + event.wheelDelta);
    });

    element.addEventListener('mousemove', this._OnMove.bind(this));
  };

  MouseManager.prototype._OnMove = function(event) {
    var x = event.offsetX * 0xFFFF / this._element.clientWidth;
    var y = event.offsetY * 0xFFFF / this._element.clientHeight;

    this._rtcChannel.send('M' + Math.floor(x) + '|' + Math.floor(y));
  };
})();


var PeerConnectionManager;

(function() {
  PeerConnectionManager = function(videoElement, serverAddress) {
    this._videoElement = videoElement;
    this._serverAddress = serverAddress;

    var peerConnection = new RTCPeerConnection({
      iceServers:[{
        urls: ['stun:stun.l.google.com:19302']
      }]
    }, {
      optional: [{
        DtlsSrtpKeyAgreement: true
      }]
    });

    this._peerConnection = peerConnection;

    peerConnection.onicecandidate = this._OnICECandidate.bind(this);
    peerConnection.onaddstream = this._OnAddStream.bind(this);

    var dataChannel = peerConnection.createDataChannel('userinput', {
      ordered: true,
      reliable: true
    });
    this._dataChannel = dataChannel;

    dataChannel.onopen = this._OnDataChannelOpen.bind(this);

    peerConnection.createOffer(function(offer) {
      peerConnection.setLocalDescription(offer);
    }, function(err) {
      console.error(err);
    }, {
      offerToReceiveAudio: true,
      offerToReceiveVideo: true
    });
  };

  PeerConnectionManager.prototype._OnDataChannelOpen = function() {
    new MouseManager(this._videoElement, this._dataChannel);
    new KeyboardManager(document.body, this._dataChannel);
  };

  PeerConnectionManager.prototype._OnICECandidate = function(event) {
    var self = this;

    if (event.candidate === null) {
      var sdp = this._peerConnection.localDescription.sdp;

      console.log(sdp);

      var req = new XMLHttpRequest();
	  
      req.open('POST', this._serverAddress, true);
	  
      req.onreadystatechange = function () {
        if (req.readyState !== 4 || req.status !== 200) {
          return;
        }
        var answer = req.responseText;
        self._peerConnection.setRemoteDescription(new RTCSessionDescription({
          type: 'answer',
          sdp: answer
        }));
      };

      req.send(sdp);
    }
  };

  PeerConnectionManager.prototype._OnAddStream = function(event) {
    var url = URL.createObjectURL(event.stream);
    this._videoElement.src = url;
  };
})();
