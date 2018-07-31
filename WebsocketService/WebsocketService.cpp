/**
* Copyright 2018 Logimic,s.r.o.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#define IWebsocketService_EXPORTS

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

#include "WebsocketService.h"
#include "Trace.h"
#include <libwebsockets.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <cstring>
#include <queue>
#include <vector>

#include "shape__WebsocketService.hxx"

TRC_INIT_MODULE(shape::WebsocketService);

/* LWS_LIBRARY_VERSION_NUMBER looks like 1005001 for e.g. version 1.5.1 */
#define LWS_LIBRARY_VERSION_NUMBER (LWS_LIBRARY_VERSION_MAJOR*1000000)+(LWS_LIBRARY_VERSION_MINOR*1000)+LWS_LIBRARY_VERSION_PATCH

/*
//starting websocket server
//message handler processing remote control messages
lws.registerMessageHandler([&](const std::string msg) { messageHandler(msg); });
lws.run();

void messageHandler(const std::string& msg)
{
}
*/
namespace shape {
  const int BUFSIZE = 64 * 1024;

  struct lws_context *context = nullptr;

  class WebsocketService::Imp
  {
  public:
    static Imp& get()
    {
      static Imp s;
      return s;
    }

    void sendMessage(const std::vector<uint8_t> & msg)
    {
      if (m_runThd) {
        std::unique_lock<std::mutex> lck(m_connectionMutex);
        if (m_wsi) {
          m_msgQueue.push(msg);
          lws_callback_on_writable(m_wsi);
          lws_cancel_service_pt(m_wsi);
        }
      }
      else {
        TRC_WARNING("Websocket is not started" << PAR(m_port));
      }
    }

    void startThd()
    {
      if (!m_runThd) {
        m_runThd = true;
        m_thd = std::thread([this]() { this->runThd(); });
      }
    }

    void stopThd()
    {
      if (m_runThd) {
        m_runThd = false;
        if (context) lws_cancel_service(context);
        if (m_thd.joinable()) {
          std::cout << "Joining LwsServer thread ..." << std::endl;
          m_thd.join();
          std::cout << "LwsServer thread joined" << std::endl;
        }
      }
    }

    void registerMessageHandler(MessageHandlerFunc messageHandlerFunc)
    {
      m_messageHandlerFunc = messageHandlerFunc;
    }

    void unregisterMessageHandler()
    {
      m_messageHandlerFunc = nullptr;
    }

    void handleMsg(const std::vector<uint8_t> & msg)
    {
      if (m_messageHandlerFunc) {
        m_messageHandlerFunc(msg);
      }
      else {
        TRC_WARNING("Message handler is not registered");
      }
    }

    void setWsi(lws * wsi)
    {
      std::unique_lock<std::mutex> lck(m_connectionMutex);
      m_wsi = wsi;
    }

    ~Imp()
    {
      delete[] m_buf;
    }

    int getPort() const
    {
      return m_port;
    }

    void setPort(int port)
    {
      m_port = port;
    }

    void activate(const shape::Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "WebsocketService instance activate" << std::endl <<
        "******************************"
      );

      // server url will be http://localhost:<port> default port: 1338
      props->getMemberAsInt("WebsocketPort", m_port);
      props->getMemberAsBool("AutoStart", m_autoStart);
      TRC_INFORMATION(PAR(m_port) << PAR(m_autoStart));
      
      if (m_autoStart) {
        startThd();
      }

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "WebsocketService instance deactivate" << std::endl <<
        "******************************"
      );

      stopThd();

      TRC_FUNCTION_LEAVE("")
    }

  private:
    Imp()
    {
      m_buf = shape_new unsigned char[BUFSIZE];
    }

    std::mutex m_connectionMutex;
    std::queue<std::vector<uint8_t>> m_msgQueue;

    bool m_runThd = false;
    std::thread m_thd;
    unsigned char* m_buf = nullptr;
    unsigned m_bufSize = 0;

    MessageHandlerFunc m_messageHandlerFunc;

    lws * m_wsi = nullptr;

    // default server url will be http://localhost:1338
    int m_port = 1338;
    bool m_autoStart = true;

    void runThd()
    {
      struct lws_context_creation_info info;
      //struct lws_vhost *vhost;
      char interface_name[128] = "";
      //unsigned int ms, oldms = 0;
      const char *iface = NULL;
      char cert_path[1024] = "";
      char key_path[1024] = "";
      char ca_path[1024] = "";
      int uid = -1, gid = -1;
      int use_ssl = 0;
      int pp_secs = 0;
      int opts = 0;
      int n = 0;

      struct lws_protocols protocols[] = {

        /* first protocol must always be HTTP handler */
        {
          "http-only",   // name
          callback_http, // callback
          0              // per_session_data_size
        },
        {
          NULL, // protocol name - very important!
          callback_cobalt,   // callback
          128, /* rx buf size must be >= permessage-deflate rx size
                               * dumb-increment only sends very small packets, so we set
                               * this accordingly.  If your protocol will send bigger
                               * things, adjust this to match */
        },
        {
          "iqrf", // protocol name - very important!
          callback_cobalt,   // callback
          128, /* rx buf size must be >= permessage-deflate rx size
               * dumb-increment only sends very small packets, so we set
               * this accordingly.  If your protocol will send bigger
               * things, adjust this to match */
        },
        {
          "cobalt", // protocol name - very important!
          callback_cobalt,   // callback
          128, /* rx buf size must be >= permessage-deflate rx size
               * dumb-increment only sends very small packets, so we set
               * this accordingly.  If your protocol will send bigger
               * things, adjust this to match */
        },
        {
          NULL, NULL, 0,   /* End of list */
        }
      };

      /*
      * take care to zero down the info struct, he contains random garbaage
      * from the stack otherwise
      */
      memset(&info, 0, sizeof info);
      info.port = m_port;

      //TODO not all set explicitly, just used nulled info
      info.protocols = protocols;
      info.uid = uid;
      info.gid = gid;

      /* tell the library what debug level to emit and to send it to syslog */
      lws_set_log_level(LLL_INFO, lwsl_emit_syslog);

      lwsl_notice("libwebsockets server\n");

      context = lws_create_context(&info);

      if (context == NULL) {
        lwsl_err("libwebsocket init failed\n");
        lws_context_destroy(context);
        return;
      }

      std::cout << "starting server..." << std::endl;

      while (m_runThd) {

        lws_service(context, 10000);
      }

      lws_context_destroy(context);
    }

    //static part
    //------------------------------

    static int callback_http(
      struct lws *wsi,
      enum lws_callback_reasons reason,
      void *user,
      void *in,
      size_t len)
    {
      //return 0;
      callback_cobalt(wsi, reason, user, in, len);
      return 0;
    }

    static int callback_cobalt(
      struct lws *wsi,
      enum lws_callback_reasons reason,
      void *user,
      void *in,
      size_t len)
    {

      switch (reason) {
      case LWS_CALLBACK_ESTABLISHED: // just log message that someone is connecting
        lwsl_notice("callback_zep connection established\n");
        Imp::get().setWsi(wsi);
        lws_callback_on_writable(wsi);
        break;
      case LWS_CALLBACK_CLOSED: // just log message that someone is connecting
        lwsl_notice("callback_zep connection closed\n");
        Imp::get().setWsi(nullptr);
        break;
      case LWS_CALLBACK_RECEIVE:
        Imp::get().handleMsg(std::vector<uint8_t>((uint8_t*)in, (uint8_t*)in + len));
        break;
      case LWS_CALLBACK_SERVER_WRITEABLE:
        if (Imp::get().sendMsgOnWritable()) {
          lws_callback_on_writable(wsi);
        }
        break;
      default:
        break;
      }

      return 0;
    }

    //get all cached msgs to send if any
    bool sendMsgOnWritable()
    {
      std::unique_lock<std::mutex> lck(m_connectionMutex);
      if (!m_msgQueue.empty()) {

        const std::vector<uint8_t> msg = m_msgQueue.front();

        const unsigned MINSZ = LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING;

        //possibly realocate
        if (nullptr == m_buf || m_bufSize < MINSZ + msg.size()) {
          m_bufSize = MINSZ + msg.size();
          delete[] m_buf;
          m_buf = shape_new unsigned char[MINSZ + msg.size()];
        }

        std::memcpy(&m_buf[LWS_SEND_BUFFER_PRE_PADDING], msg.data(), msg.size());

        if (m_wsi) {
          int written = lws_write(m_wsi, &m_buf[LWS_SEND_BUFFER_PRE_PADDING], msg.size(), LWS_WRITE_TEXT);
          if (written != msg.size()) {
            TRC_WARNING(PAR(msg.size() << PAR(written)));
          }
        }

        m_msgQueue.pop();
        if (m_msgQueue.empty()) {
          return false;
        }
        else {
          return true;
        }
      }
      return false;
    }

  };

  ///////////////////////////////////////
  WebsocketService::WebsocketService()
  {
  }

  WebsocketService::~WebsocketService()
  {
  }

  void WebsocketService::sendMessage(const std::vector<uint8_t> & msg)
  {
    Imp::get().sendMessage(msg);
  }

  void WebsocketService::start()
  {
    Imp::get().startThd();
  }

  void WebsocketService::stop()
  {
    Imp::get().stopThd();
  }

  void WebsocketService::registerMessageHandler(MessageHandlerFunc messageHandlerFunc)
  {
    Imp::get().registerMessageHandler(messageHandlerFunc);
  }

  void WebsocketService::unregisterMessageHandler()
  {
    Imp::get().unregisterMessageHandler();
  }

  void WebsocketService::activate(const shape::Properties *props)
  {
    Imp::get().activate(props);
  }

  void WebsocketService::deactivate()
  {
    Imp::get().deactivate();
  }

  void WebsocketService::modify(const shape::Properties *props)
  {
  }

  void WebsocketService::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void WebsocketService::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }


}
