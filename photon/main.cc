/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <functional>
#include <iostream>

#include "webrtc/base/physicalsocketserver.h"

#include "photon/conductor.h"
#include "photon/data_socket.h"

#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"

#include "boost/asio.hpp"
#include "boost/thread/thread.hpp"

#include "photon/http/server/server.h"

namespace boost {
  void throw_exception(std::exception const & e) {
    std::cerr << e.what() << std::endl;
  }
}

static std::string get_offer(DataSocket** socket) {
	std::string offer;
	ListeningSocket listener;
	const unsigned int port = 8888;

	if (!listener.Create()) {
		LOG(LERROR) << "Failed to create server socket";
		return "";
	}
	else if (!listener.Listen(port)) {
		LOG(LERROR) << "Failed to listen on server socket";
		return "";
	}

	LOG(INFO) << "Server listening on port " << port;

	// PeerChannel clients;
	typedef std::vector<DataSocket*> SocketArray;
	SocketArray sockets;
	bool quit = false;
	while (!quit) {
		fd_set socket_set;
		FD_ZERO(&socket_set);
		if (listener.valid())
			FD_SET(listener.socket(), &socket_set);

		for (SocketArray::iterator i = sockets.begin(); i != sockets.end(); ++i)
			FD_SET((*i)->socket(), &socket_set);

		struct timeval timeout = { 10, 0 };
		if (select(FD_SETSIZE, &socket_set, NULL, NULL, &timeout) == SOCKET_ERROR) {
			LOG(LERROR) << "select failed";
			break;
		}

		for (SocketArray::iterator i = sockets.begin(); i != sockets.end(); ++i) {
			DataSocket* s = *i;
			bool socket_done = true;
			if (FD_ISSET(s->socket(), &socket_set)) {

				if (s->OnDataAvailable(&socket_done) && s->request_received()) {
					if (s->method() == DataSocket::OPTIONS) {
						s->Send("200 OK", true, "", "", "");
					}
					else {
						*socket = s;
						offer = s->data();
						quit = true;
					}
				}
			}
			else {
				socket_done = false;
			}

			if (socket_done) {
				LOG(INFO) << "Disconnecting socket";

				assert(s->valid());  // Close must not have been called yet.
				FD_CLR(s->socket(), &socket_set);
				delete (*i);
				i = sockets.erase(i);
				if (i == sockets.end())
					break;
			}
		}

		// clients.CheckForTimeout();
		if (FD_ISSET(listener.socket(), &socket_set)) {
			DataSocket* s = listener.Accept();
			sockets.push_back(s);
		}
	}


	for (SocketArray::iterator i = sockets.begin(); i != sockets.end(); ++i) {
		if (*i != *socket)
			delete (*i);
	}
	sockets.clear();
	return offer;
}

class ConductorThr : public rtc::Runnable {
private:
    std::string offer_;
    DataSocket* peer_;
    rtc::scoped_refptr<Conductor> conductor_;
    
public:
    ConductorThr(std::string offer, DataSocket* peer) :
        offer_(offer), peer_(peer), conductor_(new rtc::RefCountedObject<Conductor>())
    {
        
    }
    
    virtual ~ConductorThr() {
        
    }
    
    bool ConnectToPeer(const std::string & offer, DataSocket* peer) {
        LOG(INFO) << "Ouech ouech la famille";
        LOG(INFO) << offer;
        conductor_->ConnectToPeer(offer_, peer_);
        return false;
    }
    
    virtual void Run(rtc::Thread* thread) {
        rtc::InitializeSSL();
    
        

        
        LOG(INFO) << "DONE" << std::endl;
        thread->ProcessMessages(rtc::ThreadManager::kForever);
        rtc::CleanupSSL();
    }
};

int amain(int argc, char* argv[]) {
	DataSocket* peer = nullptr;
	std::string offer = get_offer(&peer);
    rtc::Thread thr;
    
  ConductorThr cthr(offer, peer);
  thr.Start(&cthr);

  LOG(INFO) << "DONE" << std::endl;
  return 0;
  
  rtc::AutoThread auto_thread;
  rtc::Thread* thread = rtc::Thread::Current();


  // CustomSocketServer socket_server(thread);
  // thread->set_socketserver(&socket_server);

  rtc::InitializeSSL();

  // Must be constructed after we set the socketserver.
  rtc::scoped_refptr<Conductor> conductor(
      new rtc::RefCountedObject<Conductor>());

  conductor->ConnectToPeer(offer, peer);

  thread->Run();
  
  rtc::CleanupSSL();
  return 0;
}

class Photon {
public:
	void request_handler(const photon::http::server::request & req, photon::http::server::reply & rep) {
		std::cout << req.method << std::endl;
		std::cout << req.uri << std::endl;
		std::cout << req.body << std::endl;

		if (!req.method.compare("POST") && !req.uri.compare("/webrtc")) {
			rep = photon::http::server::reply::stock_reply(
					photon::http::server::reply::bad_request);
		} else {
			rep = photon::http::server::reply::stock_reply(
					photon::http::server::reply::not_found);
		}
	}
};

int main(int argc, char *argv[]) {
 	/*
    DataSocket* peer = nullptr;
	std::string offer = get_offer(&peer);
    rtc::Thread thr;
    
    
  ConductorThr cthr(offer, peer);
  thr.Start(&cthr);
  */

  // thr.Invoke<bool>(RTC_FROM_HERE, rtc::Bind(&ConductorThr::ConnectToPeer, &cthr, offer, peer));
  
  // Initialise the server.
  Photon p;

  photon::http::server::server s(8888, std::bind(&Photon::request_handler,
			                         &p,
						 std::placeholders::_1,
						 std::placeholders::_2));

  // Run the server until stopped.
  s.run();
  return 0;
}
