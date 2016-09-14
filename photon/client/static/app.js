'use strict';

/* jshint browser: true */
/* globals PeerConnectionManager */


function main_rtc(server_address) {
  var vidContainer = document.getElementById('vid-container');
  vidContainer.style.display = "initial";
  vidContainer.addEventListener('dblclick', function() {
    if (vidContainer.mozRequestFullScreen) {
      vidContainer.mozRequestFullScreen();
    } else if (vidContainer.webkitRequestFullscreen) {
      vidContainer.webkitRequestFullscreen();
    }
  }, false);

  var video = document.getElementById('video');
  new PeerConnectionManager(video, 'http://' + server_address + '/webrtc');
}

main_rtc("localhost:8080");
