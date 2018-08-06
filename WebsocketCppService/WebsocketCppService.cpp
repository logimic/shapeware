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

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "shape__WebsocketCppService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::WebsocketCppService);

namespace shape {

  typedef websocketpp::connection_hdl connection_hdl;
  typedef websocketpp::server<websocketpp::config::asio> WsServer;

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

        if (m_messageStrHandlerFunc) {
          m_messageStrHandlerFunc(msg->get_payload(), connId);
          found = false;
        }

        if (m_messageHandlerFunc) {
          uint8_t* buf = (uint8_t*)msg->get_payload().data();
          std::vector<uint8_t> vmsg(buf, buf + msg->get_payload().size());
          m_messageHandlerFunc(vmsg, connId);
          found = false;
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
      bool retval = true;
      
      websocketpp::server<websocketpp::config::asio>::connection_ptr con = m_server.get_con_from_hdl(hdl);

      //TODO provision id
      std::ostringstream os;
      os << con->get_handle().lock().get();
      std::string connId = os.str();

      TRC_DEBUG("Connected: " << PAR(connId));;

      websocketpp::uri_ptr uri = con->get_uri();
      std::string query = uri->get_query(); // returns empty string if no query string set.
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

      TRC_FUNCTION_LEAVE(PAR(retval));
      return true;
    }

    void on_fail(connection_hdl hdl)
    {
      TRC_FUNCTION_ENTER("");
      websocketpp::server<websocketpp::config::asio>::connection_ptr con = m_server.get_con_from_hdl(hdl);
      websocketpp::lib::error_code ec = con->get_ec();
      TRC_WARNING("Error: " << NAME_PAR(hdl, hdl.lock().get()) << " " << ec.message() );
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
                // Error closing websocket. Log reason using ec.message().
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
      sendMessage(std::string((char*)msg.data(), msg.size()), connId);
    }

    void sendMessage(const std::string & msg, const std::string& connId)
    {
      TRC_FUNCTION_ENTER(PAR(connId));
      if (m_runThd) {

        std::unique_lock<std::mutex> lock(m_mux);

        for (auto it : m_connectionsStrMap) {
          if (connId.empty() || it.second == connId) { //broadcast if empty

            websocketpp::lib::error_code ec;
            m_server.send(it.first, msg, websocketpp::frame::opcode::text, ec); // send text message.
            if (ec) {
              TRC_WARNING("Cannot send messgae: " << PAR(m_port) << ec.message());
              return;
            }
            break;
          }
        }
      }
      else {
        TRC_WARNING("Websocket is not started" << PAR(m_port));
      }
      TRC_FUNCTION_LEAVE("");
    }

    void start()
    {
      TRC_FUNCTION_ENTER("");

      // listen on specified port
      try {
        m_server.listen(m_port);
      }
      catch (websocketpp::exception const &e) {
        // Websocket exception on listen. Get char string via e.what().
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

        // Stopping the Websocket listener and closing outstanding connections.
        websocketpp::lib::error_code ec;
        m_server.stop_listening(ec);
        if (ec) {
          // Failed to stop listening. Log reason using ec.message().
          return;
        }

        // Close all existing websocket connections.
        std::unique_lock<std::mutex> lock(m_mux);

        std::string data = "Terminating connection...";
        for (auto con : m_connectionsStrMap) {
          websocketpp::lib::error_code ec;
          m_server.close(con.first, websocketpp::close::status::normal, data, ec); // send text message.
          if (ec) { // we got an error
                    // Error closing websocket. Log reason using ec.message().
          }
        }

        // Stop the endpoint.
        m_server.stop();

        if (m_thd.joinable()) {
          std::cout << "Joining WsServer thread ..." << std::endl;
          m_thd.join();
          std::cout << "WsServer thread joined" << std::endl;
        }
      }
      TRC_FUNCTION_LEAVE("");
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

    void unregisterOpenHandler(OpenHandlerFunc hndl)
    {
      m_openHandlerFunc = nullptr;
    }

    void unregisterCloseHandler(CloseHandlerFunc hndl)
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
      TRC_INFORMATION(PAR(m_port) << PAR(m_autoStart));

      // set up access channels to only log interesting things
      m_server.clear_access_channels(websocketpp::log::alevel::all);
      m_server.set_access_channels(websocketpp::log::alevel::access_core);
      m_server.set_access_channels(websocketpp::log::alevel::app);

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

      m_server.set_message_handler([&](connection_hdl hdl, WsServer::message_ptr msg) {
        on_message(hdl, msg);
      });

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
    m_imp->unregisterMessageHandler();
  }

  void WebsocketCppService::unregisterCloseHandler()
  {
    m_imp->unregisterMessageHandler();
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
