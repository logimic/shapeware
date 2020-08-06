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

#include "WebsocketCppService.h"
#include "Trace.h"
#include <thread>
#include <mutex>
#include <map>

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_INTERNAL_

//#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>

#include "shape__WebsocketCppService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::WebsocketCppService);

namespace shape {

  typedef websocketpp::connection_hdl connection_hdl;
  //typedef websocketpp::server<websocketpp::config::asio> WsServer;
  typedef websocketpp::server<websocketpp::config::asio_tls> WsServer;

  // pull out the type of messages sent by our config
  typedef websocketpp::config::asio::message_type::ptr message_ptr;
  typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

  class LogStream : public std::streambuf {
  private:
    std::string buffer;

  protected:
    int overflow(int ch) override {
      buffer.push_back((char)ch);
      if (ch == '\n') {
        TRC_INFORMATION("Websocketpp: " << buffer);
        buffer.clear();
      }
      return ch;
    }
  };

  class WebsocketCppService::Imp
  {
  private:
    WsServer m_server;
    int m_port = 1338;

    std::mutex m_mux;
    std::map<connection_hdl, std::string, std::owner_less<connection_hdl>> m_connectionsStrMap;

    bool m_autoStart = true;
    bool m_acceptOnlyLocalhost = false;
    bool m_runThd = false;
    std::thread m_thd;

    MessageHandlerFunc m_messageHandlerFunc;
    MessageStrHandlerFunc m_messageStrHandlerFunc;
    OpenHandlerFunc m_openHandlerFunc;
    CloseHandlerFunc m_closeHandlerFunc;

    LogStream m_wsLoger;
    std::ostream m_wsLogerOs;

    // lock mux before
    bool getHndl(const std::string& connId, connection_hdl& hdl)
    {
      for (auto it : m_connectionsStrMap) {
        if (it.second == connId) {
          hdl = it.first;
          return true;
        }
      }
      return false;
    }

    // lock mux before
    bool getConnId(connection_hdl hdl, std::string& connId)
    {
      auto found = m_connectionsStrMap.find(hdl);
      if (found != m_connectionsStrMap.end()) {
        connId = found->second;
        return true;
      }
      return false;
    }

    void on_message(connection_hdl hdl, WsServer::message_ptr msg)
    {
      TRC_FUNCTION_ENTER("");

      std::string connId;
      bool found = false;
      {
        std::unique_lock<std::mutex> lock(m_mux);
        found = getConnId(hdl, connId);
      }

      if (found) {
        TRC_DEBUG("Found: " << PAR(connId));;
        found = false;

        if (m_messageStrHandlerFunc) {
          m_messageStrHandlerFunc(msg->get_payload(), connId);
          found = true;
        }

        if (m_messageHandlerFunc) {
          uint8_t* buf = (uint8_t*)msg->get_payload().data();
          std::vector<uint8_t> vmsg(buf, buf + msg->get_payload().size());
          m_messageHandlerFunc(vmsg, connId);
          found = true;
        }

        if (!found) {
          TRC_WARNING("Handler is not registered");
        }

      }
      else {
        TRC_WARNING("Cannot find matching connection");
      }
      TRC_FUNCTION_LEAVE("");
    }

    bool on_validate(connection_hdl hdl)
    {
      //TODO on_connection can be use instead, however we're ready for authentication by a token
      TRC_FUNCTION_ENTER("");
      bool valid = true;

      //websocketpp::server<websocketpp::config::asio>::connection_ptr con = m_server.get_con_from_hdl(hdl);
      WsServer::connection_ptr con = m_server.get_con_from_hdl(hdl);

      //TODO provision id
      std::ostringstream os;
      os << con->get_handle().lock().get();
      std::string connId = os.str();

      websocketpp::uri_ptr uri = con->get_uri();
      std::string query = uri->get_query(); // returns empty string if no query string set.
      std::string host = uri->get_host();

      if (m_acceptOnlyLocalhost) {
        if (host == "localhost" || host == "127.0.0.1" || host == "[::1]") {
          valid = true;
        }
        else {
          valid = false;
          TRC_INFORMATION("Connection refused: " << PAR(connId) << PAR(host));;
        }
      }

      if (valid) {
        TRC_INFORMATION("Connected: " << PAR(connId) << PAR(host));;

        if (!query.empty()) {
          // Split the query parameter string here, if desired.
          // We assume we extracted a string called 'id' here.
        }
        else {
          // Reject if no query parameter provided, for example.
          //return false;
        }

        {
          std::unique_lock<std::mutex> lock(m_mux);
          m_connectionsStrMap.insert(std::make_pair(hdl, connId));
        }

        if (m_openHandlerFunc) {
          m_openHandlerFunc(connId);
        }
        else {
          TRC_WARNING("Message handler is not registered");
        }
      }
      TRC_FUNCTION_LEAVE(PAR(valid));
      return valid;
    }

    void on_fail(connection_hdl hdl)
    {
      TRC_FUNCTION_ENTER("");
      //websocketpp::server<websocketpp::config::asio>::connection_ptr con = m_server.get_con_from_hdl(hdl);
      WsServer::connection_ptr con = m_server.get_con_from_hdl(hdl);
      websocketpp::lib::error_code ec = con->get_ec();
      TRC_WARNING("Error: " << NAME_PAR(hdl, hdl.lock().get()) << " " << ec.message());
      TRC_FUNCTION_LEAVE("");
    }

    void sendClose(const std::string& connId)
    {
      TRC_FUNCTION_ENTER(PAR(connId));

      connection_hdl hdl;

      {
        std::unique_lock<std::mutex> lock(m_mux);
        if (getHndl(connId, hdl)) {
          m_connectionsStrMap.erase(hdl);
        }
      }

      std::string data = "Terminating connection...";
      websocketpp::lib::error_code ec;
      m_server.close(hdl, websocketpp::close::status::normal, data, ec); // send close message.
      if (ec) { // we got an error
        TRC_WARNING("Error: " << PAR(connId) << " " << ec.message());
      }

      TRC_FUNCTION_LEAVE("");
    }

    void on_close(connection_hdl hdl)
    {
      TRC_FUNCTION_ENTER("");
      std::string connId;
      bool found = false;
      {
        std::unique_lock<std::mutex> lock(m_mux);
        found = getConnId(hdl, connId);
        m_connectionsStrMap.erase(hdl);
      }

      if (found) {
        TRC_DEBUG("Found: " << PAR(connId));;

        if (m_closeHandlerFunc) {
          m_closeHandlerFunc(connId);
        }
        else {
          TRC_WARNING("Message handler is not registered");
        }
      }
      TRC_FUNCTION_LEAVE("");
    }
    ///////////////////////////////
    //?????????????????????????????
    void on_http(connection_hdl hdl) {
      WsServer::connection_ptr con = m_server.get_con_from_hdl(hdl);

      con->set_body("Hello World!");
      con->set_status(websocketpp::http::status_code::ok);
    }

    std::string get_password() {
      return "test";
    }

    // See https://wiki.mozilla.org/Security/Server_Side_TLS for more details about
    // the TLS modes. The code below demonstrates how to implement both the modern
    enum tls_mode {
      MOZILLA_INTERMEDIATE = 1,
      MOZILLA_MODERN = 2
    };

    context_ptr on_tls_init(tls_mode mode, connection_hdl hdl) {
      namespace asio = websocketpp::lib::asio;

      std::cout << "on_tls_init called with hdl: " << hdl.lock().get() << std::endl;
      std::cout << "using TLS mode: " << (mode == MOZILLA_MODERN ? "Mozilla Modern" : "Mozilla Intermediate") << std::endl;

      context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);

      try {
        if (mode == MOZILLA_MODERN) {
          // Modern disables TLSv1
          ctx->set_options(asio::ssl::context::default_workarounds |
            asio::ssl::context::no_sslv2 |
            asio::ssl::context::no_sslv3 |
            asio::ssl::context::no_tlsv1 |
            asio::ssl::context::single_dh_use);
        }
        else {
          ctx->set_options(asio::ssl::context::default_workarounds |
            asio::ssl::context::no_sslv2 |
            asio::ssl::context::no_sslv3 |
            asio::ssl::context::single_dh_use);
        }
        //ctx->set_password_callback(bind(&get_password));
        ctx->use_certificate_chain_file("./tls/cert.pem");
        ctx->use_private_key_file("./tls/key.pem", asio::ssl::context::pem);
        //ctx->use_certificate_chain_file("./tls/server.pem");
        //ctx->use_private_key_file("./tls/server.pem", asio::ssl::context::pem);

        // Example method of generating this file:
        // `openssl dhparam -out dh.pem 2048`
        // Mozilla Intermediate suggests 1024 as the minimum size to use
        // Mozilla Modern suggests 2048 as the minimum size to use.
        ctx->use_tmp_dh_file("./tls/dh.pem");

        std::string ciphers;

        if (mode == MOZILLA_MODERN) {
          ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK";
        }
        else {
          ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:AES:CAMELLIA:DES-CBC3-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!aECDH:!EDH-DSS-DES-CBC3-SHA:!EDH-RSA-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA";
        }

        if (SSL_CTX_set_cipher_list(ctx->native_handle(), ciphers.c_str()) != 1) {
          std::cout << "Error setting cipher list" << std::endl;
        }
      }
      catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
      }
      return ctx;
    }
    ///////////////////////////////

  public:
    Imp()
      :m_wsLogerOs(&m_wsLoger)
    {
    }

    ~Imp()
    {
    }

    void sendMessage(const std::vector<uint8_t> & msg, const std::string& connId)
    {
      std::string msgStr((char*)msg.data(), msg.size());
      sendMessage(msgStr, connId);
      TRC_FUNCTION_LEAVE("");
    }

    void sendMessage(const std::string & msg, const std::string& connId)
    {
      //TRC_FUNCTION_ENTER(PAR(connId));
      if (m_runThd) {
        if (connId.empty()) { //broadcast if empty
          for (auto it : m_connectionsStrMap) {

            websocketpp::lib::error_code ec;
            m_server.send(it.first, msg, websocketpp::frame::opcode::text, ec); // send text message.
            if (ec) {
              TRC_WARNING("Cannot send message: " << PAR(m_port) << ec.message());
            }
          }
        }
        else {
          for (auto it : m_connectionsStrMap) {
            if (it.second == connId) {

              websocketpp::lib::error_code ec;
              m_server.send(it.first, msg, websocketpp::frame::opcode::text, ec); // send text message.
              if (ec) {
                auto conState = m_server.get_con_from_hdl(it.first)->get_state();
                TRC_WARNING("Cannot send message: " << PAR(conState) << PAR(m_port) << ec.message());
              }
              break;
            }
          }
        }
      }
      else {
        TRC_WARNING("Websocket is not started" << PAR(m_port));
      }
      //TRC_FUNCTION_LEAVE("");
    }

    void start()
    {
      TRC_FUNCTION_ENTER("");

      // listen on specified port
      try {
        m_server.set_reuse_addr(true);
        m_server.listen(m_port);
      }
      catch (websocketpp::exception const &e) {
        // Websocket exception on listen. Get char string via e.what().
        CATCH_EXC_TRC_WAR(websocketpp::exception, e, "listen failed");
      }

      // Starting Websocket accept.
      websocketpp::lib::error_code ec;
      m_server.start_accept(ec);
      if (ec) {
        // Can log an error message with the contents of ec.message() here.
      }

      if (!m_runThd) {
        m_runThd = true;
        m_thd = std::thread([this]() { this->runThd(); });
      }
      TRC_FUNCTION_LEAVE("");
    }

    void stop()
    {
      TRC_FUNCTION_ENTER("");
      if (m_runThd) {
        m_runThd = false;

        TRC_INFORMATION("stop listen");
        // Stopping the Websocket listener and closing outstanding connections.
        websocketpp::lib::error_code ec;
        if (m_server.is_listening()) {
          m_server.stop_listening(ec);
          if (ec) {
            // Failed to stop listening. Log reason using ec.message().
            TRC_INFORMATION("Failed stop_listening: " << ec.message());
            //return;
          }
        }

        // copy all existing websocket connections.
        std::map<connection_hdl, std::string, std::owner_less<connection_hdl>> connectionsStrMap;
        {
          std::unique_lock<std::mutex> lock(m_mux);
          connectionsStrMap = m_connectionsStrMap;
        }

        //now close unlocked - we have to avoid deadlock on_close()
        TRC_INFORMATION("close connections");
        std::string data = "Terminating connection...";
        for (auto con : connectionsStrMap) {
          websocketpp::lib::error_code ec;
          TRC_INFORMATION("close connection: " << con.second);
          m_server.close(con.first, websocketpp::close::status::normal, data, ec); // send text message.
          if (ec) { // we got an error
                    // Error closing websocket. Log reason using ec.message().
          }
        }

        // clear all existing websocket connections mapping.
        {
          std::unique_lock<std::mutex> lock(m_mux);
          m_connectionsStrMap.clear();
        }

        // Stop the endpoint.
        TRC_INFORMATION("stop server");
        //m_server.stop();
        //m_server.reset();

        if (m_thd.joinable()) {
          std::cout << "Joining WsServer thread ..." << std::endl;
          m_thd.join();
          std::cout << "WsServer thread joined" << std::endl;
        }
      }
      TRC_FUNCTION_LEAVE("");
    }

    bool isStarted() const
    {
      return m_runThd;
    }

    void registerMessageHandler(MessageHandlerFunc hndl)
    {
      m_messageHandlerFunc = hndl;
    }

    void registerMessageStrHandler(MessageStrHandlerFunc hndl)
    {
      m_messageStrHandlerFunc = hndl;
    }

    void registerOpenHandler(OpenHandlerFunc hndl)
    {
      m_openHandlerFunc = hndl;
    }

    void registerCloseHandler(CloseHandlerFunc hndl)
    {
      m_closeHandlerFunc = hndl;
    }

    void unregisterMessageHandler()
    {
      m_messageHandlerFunc = nullptr;
    }

    void unregisterMessageStrHandler()
    {
      m_messageStrHandlerFunc = nullptr;
    }

    void unregisterOpenHandler()
    {
      m_openHandlerFunc = nullptr;
    }

    void unregisterCloseHandler()
    {
      m_closeHandlerFunc = nullptr;
    }

    int getPort() const
    {
      return m_port;
    }

    void activate(const shape::Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "WebsocketCppService instance activate" << std::endl <<
        "******************************"
      );

      // WsServer url will be http://localhost:<port> default port: 1338
      props->getMemberAsInt("WebsocketPort", m_port);
      props->getMemberAsBool("AutoStart", m_autoStart);
      props->getMemberAsBool("acceptOnlyLocalhost", m_acceptOnlyLocalhost);
      TRC_INFORMATION(PAR(m_port) << PAR(m_autoStart) << PAR(m_acceptOnlyLocalhost));

      // set up access channels to only log interesting things
      m_server.set_access_channels(websocketpp::log::alevel::all);
      m_server.set_access_channels(websocketpp::log::elevel::all);

      // Set custom logger (ostream-based).
      m_server.get_alog().set_ostream(&m_wsLogerOs);
      m_server.get_elog().set_ostream(&m_wsLogerOs);

      // Initialize Asio
      m_server.init_asio();

      m_server.set_validate_handler([&](connection_hdl hdl)->bool {
        return on_validate(hdl);
      });

      m_server.set_fail_handler([&](connection_hdl hdl) {
        on_fail(hdl);
      });

      m_server.set_close_handler([&](connection_hdl hdl) {
        on_close(hdl);
      });

      //m_server.set_http_handler([&](connection_hdl hdl, WsServer::message_ptr msg) {
      //  on_message(hdl, msg);
      //});

//??????????????
      m_server.set_http_handler([&](connection_hdl hdl) {
        on_http(hdl);
      });

      m_server.set_tls_init_handler([&](connection_hdl hdl)->context_ptr {
        //return on_tls_init(MOZILLA_INTERMEDIATE, hdl);
        return on_tls_init(MOZILLA_MODERN, hdl);
      });

//??????????????
      if (m_autoStart) {
        start();
      }

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "WebsocketCppService instance deactivate" << std::endl <<
        "******************************"
      );

      stop();

      TRC_FUNCTION_LEAVE("")
    }

  private:

    void runThd()
    {
      TRC_FUNCTION_ENTER("");

      while (m_runThd) {
        // Start the ASIO io_service run loop
        try {
          m_server.run();
        }
        catch (websocketpp::exception const & e) {
          std::cout << e.what() << std::endl;
        }
      }
    }

  };

  ///////////////////////////////////////
  WebsocketCppService::WebsocketCppService()
  {
    m_imp = shape_new Imp();
  }

  WebsocketCppService::~WebsocketCppService()
  {
    delete m_imp;
  }

  void WebsocketCppService::sendMessage(const std::vector<uint8_t> & msg, const std::string& connId)
  {
    m_imp->sendMessage(msg, connId);
  }

  void WebsocketCppService::sendMessage(const std::string & msg, const std::string& connId)
  {
    m_imp->sendMessage(msg, connId);
  }

  void WebsocketCppService::start()
  {
    m_imp->start();
  }

  void WebsocketCppService::stop()
  {
    m_imp->stop();
  }

  bool WebsocketCppService::isStarted() const
  {
    return m_imp->isStarted();
  }

  int WebsocketCppService::getPort() const
  {
    return m_imp->getPort();
  }

  void WebsocketCppService::registerMessageHandler(MessageHandlerFunc hndl)
  {
    m_imp->registerMessageHandler(hndl);
  }

  void WebsocketCppService::registerMessageStrHandler(MessageStrHandlerFunc hndl)
  {
    m_imp->registerMessageStrHandler(hndl);
  }

  void WebsocketCppService::registerOpenHandler(OpenHandlerFunc hndl)
  {
    m_imp->registerOpenHandler(hndl);
  }

  void WebsocketCppService::registerCloseHandler(CloseHandlerFunc hndl)
  {
    m_imp->registerCloseHandler(hndl);
  }

  void WebsocketCppService::unregisterMessageHandler()
  {
    m_imp->unregisterMessageHandler();
  }

  void WebsocketCppService::unregisterMessageStrHandler()
  {
    m_imp->unregisterMessageStrHandler();
  }

  void WebsocketCppService::unregisterOpenHandler()
  {
    m_imp->unregisterOpenHandler();
  }

  void WebsocketCppService::unregisterCloseHandler()
  {
    m_imp->unregisterCloseHandler();
  }

  void WebsocketCppService::activate(const shape::Properties *props)
  {
    m_imp->activate(props);
  }

  void WebsocketCppService::deactivate()
  {
    m_imp->deactivate();
  }

  void WebsocketCppService::modify(const shape::Properties *props)
  {
    (void)props; //silence -Wunused-parameter
  }

  void WebsocketCppService::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void WebsocketCppService::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }


}
