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

#define IWebsocketClientService_EXPORTS

#include "WebsocketCppClientService.h"
#include "Trace.h"
#include <thread>
#include <mutex>
#include <map>

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_INTERNAL_

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/client.hpp>

#include "shape__WebsocketCppClientService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::WebsocketCppClientService);

namespace shape {

  typedef websocketpp::connection_hdl connection_hdl;
  typedef websocketpp::client<websocketpp::config::asio> WsClient;

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

  class WebsocketCppClientService::Imp
  {
  private:
    WsClient m_client;
    connection_hdl m_connection_hdl;
    std::string m_uri;
    std::string m_server;
    std::string m_error_reason;

    std::thread m_thd;
    std::condition_variable m_connectedCondition;
    mutable std::mutex m_connectedMux;
    bool m_connected = false;

    MessageHandlerFunc m_messageHandlerFunc;
    MessageStrHandlerFunc m_messageStrHandlerFunc;
    OpenHandlerFunc m_openHandlerFunc;
    CloseHandlerFunc m_closeHandlerFunc;

    LogStream m_wsLoger;
    std::ostream m_wsLogerOs;

    void on_open(connection_hdl hdl)
    {
      TRC_FUNCTION_ENTER("");

      m_connection_hdl = hdl;

      std::unique_lock<std::mutex> lck(m_connectedMux);
      m_connected = true;
      //std::cout << ">>> WebsocketCppClientService on_open" << std::endl;
      m_server = m_client.get_con_from_hdl(hdl)->get_response_header("Server");
      m_connectedCondition.notify_all();

      if (m_openHandlerFunc) {
        m_openHandlerFunc();
      }

      TRC_FUNCTION_LEAVE("");
    }

    void on_message(connection_hdl hdl, WsClient::message_ptr msg)
    {
      //TRC_FUNCTION_ENTER("");
      
      (void)hdl; //silence -Wunused-parameter

      if (m_messageStrHandlerFunc) {
        m_messageStrHandlerFunc(msg->get_payload());
      }

      if (m_messageHandlerFunc) {
        uint8_t* buf = (uint8_t*)msg->get_payload().data();
        std::vector<uint8_t> vmsg(buf, buf + msg->get_payload().size());
        m_messageHandlerFunc(vmsg);
      }

      //TRC_FUNCTION_LEAVE("");
    }

    void on_fail(connection_hdl hdl)
    {
      TRC_FUNCTION_ENTER("");

      std::unique_lock<std::mutex> lck(m_connectedMux);
      m_connected = false;
      //std::cout << ">>> WebsocketCppClientService on_fail" << std::endl;
      m_server = m_client.get_con_from_hdl(hdl)->get_response_header("Server");
      m_error_reason = m_client.get_con_from_hdl(hdl)->get_ec().message();
      m_connectedCondition.notify_all();
      TRC_WARNING("Error: " << PAR(m_error_reason));

      TRC_FUNCTION_LEAVE("");
    }

    void on_close(connection_hdl hdl)
    {
      TRC_FUNCTION_ENTER("");

      std::unique_lock<std::mutex> lck(m_connectedMux);
      m_connected = false;
      
      std::stringstream s;
      auto con = m_client.get_con_from_hdl(hdl);
      s << "close code: " << con->get_remote_close_code() << " ("
        << websocketpp::close::status::get_string(con->get_remote_close_code())
        << "), close reason: " << con->get_remote_close_reason();
      m_error_reason = s.str();

      //std::cout << ">>> WebsocketCppClientService CloseRemote" << std::endl;
      m_connectedCondition.notify_all();

      if (m_closeHandlerFunc) {
        m_closeHandlerFunc();
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

    void sendMessage(const std::vector<uint8_t> & msg)
    {
      TRC_FUNCTION_ENTER("");

      websocketpp::lib::error_code ec;
      m_client.send(m_connection_hdl, std::string((char*)msg.data(), msg.size()), websocketpp::frame::opcode::text, ec); // send text message.
      if (ec) {
        TRC_WARNING("Cannot send message: " << ec.message());
      }

      TRC_FUNCTION_LEAVE("");
    }

    void sendMessage(const std::string & msg)
    {
      TRC_FUNCTION_ENTER(PAR(msg));

      websocketpp::lib::error_code ec;
      m_client.send(m_connection_hdl, msg, websocketpp::frame::opcode::text, ec); // send text message.
      if (ec) {
        TRC_WARNING("Cannot send messgae: " << ec.message());
      }

      TRC_FUNCTION_LEAVE("");
    }

    void sendPing()
    {
      //TRC_FUNCTION_ENTER("");

      websocketpp::lib::error_code ec;
      m_client.ping(m_connection_hdl, "ping", ec); // send ping message.
      if (ec) {
        TRC_WARNING("Cannot send ping messgae: " << ec.message());
      }

      //TRC_FUNCTION_LEAVE("");
    }

    void connect(const std::string & uri)
    {
      TRC_FUNCTION_ENTER(PAR(uri));

      std::unique_lock<std::mutex> lck(m_connectedMux);
      if (!m_connected) {
        m_uri = uri;
        websocketpp::lib::error_code ec;
        WsClient::connection_ptr con = m_client.get_connection(uri, ec);
        if (ec) {
          //std::cout << ">>> WebsocketCppClientService runThd connection error: " << ec.message() << std::endl;
          TRC_WARNING("Get connection error: " << ec.message());
        }
        else {
          m_client.connect(con);
          m_connectedCondition.wait(lck, [&]()->bool { return con->get_state() != websocketpp::session::state::connecting; });
        }
      }
      else {
        TRC_WARNING("Try connect to: " << PAR(m_uri) <<  "but already connected to: " << PAR(uri));
      }

      TRC_FUNCTION_LEAVE("");
    }

    void close()
    {
      TRC_FUNCTION_ENTER("");

      std::unique_lock<std::mutex> lck(m_connectedMux);
      //if (m_connected) {
        websocketpp::lib::error_code ec;
        m_client.close(m_connection_hdl, websocketpp::close::status::going_away, "Terminating connection...", ec);
        if (ec) {
          TRC_WARNING("Close error: " << ec.message());
        }

        m_connected = false;
        //std::cout << ">>> WebsocketCppClientService CloseLocal" << std::endl;
        m_connectedCondition.notify_all();

      //}
      TRC_FUNCTION_LEAVE("");
    }

    bool isConnected() const
    {
      std::unique_lock<std::mutex> lck(m_connectedMux);
      return m_connected;
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

    void activate(const shape::Properties *props)
    {
      (void)props; //silence -Wunused-parameter
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "WebsocketCppClientService instance activate" << std::endl <<
        "******************************"
      );

      m_client.clear_access_channels(websocketpp::log::alevel::all);
      m_client.clear_access_channels(websocketpp::log::elevel::all);

      // Set custom logger (ostream-based).
      m_client.get_alog().set_ostream(&m_wsLogerOs);
      m_client.get_elog().set_ostream(&m_wsLogerOs);

      // Initialize Asio
      m_client.init_asio();
      m_client.start_perpetual();

      m_client.set_open_handler([&](connection_hdl hdl) {
        on_open(hdl);
      });

      m_client.set_fail_handler([&](connection_hdl hdl) {
        on_fail(hdl);
      });

      m_client.set_close_handler([&](connection_hdl hdl) {
        on_close(hdl);
      });

      m_client.set_message_handler([&](connection_hdl hdl, WsClient::message_ptr msg) {
        on_message(hdl, msg);
      });

      m_thd = std::thread([&]() { m_client.run(); });

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "WebsocketCppClientService instance deactivate" << std::endl <<
        "******************************"
      );

      m_client.stop_perpetual();
      close();
      if (m_thd.joinable())
        m_thd.join();

      TRC_FUNCTION_LEAVE("")
    }

  };

  ///////////////////////////////////////
  WebsocketCppClientService::WebsocketCppClientService()
  {
    m_imp = shape_new Imp();
  }

  WebsocketCppClientService::~WebsocketCppClientService()
  {
    delete m_imp;
  }

  void WebsocketCppClientService::sendMessage(const std::vector<uint8_t> & msg)
  {
    m_imp->sendMessage(msg);
  }

  void WebsocketCppClientService::sendMessage(const std::string & msg)
  {
    m_imp->sendMessage(msg);
  }

  void WebsocketCppClientService::sendPing()
  {
    m_imp->sendPing();
  }

  void WebsocketCppClientService::connect(const std::string & uri)
  {
    m_imp->connect(uri);
  }

  void WebsocketCppClientService::close()
  {
    m_imp->close();
  }

  bool WebsocketCppClientService::isConnected() const
  {
    return m_imp->isConnected();
  }

  void WebsocketCppClientService::registerMessageHandler(MessageHandlerFunc hndl)
  {
    m_imp->registerMessageHandler(hndl);
  }

  void WebsocketCppClientService::registerMessageStrHandler(MessageStrHandlerFunc hndl)
  {
    m_imp->registerMessageStrHandler(hndl);
  }

  void WebsocketCppClientService::registerOpenHandler(OpenHandlerFunc hndl)
  {
    m_imp->registerOpenHandler(hndl);
  }

  void WebsocketCppClientService::registerCloseHandler(CloseHandlerFunc hndl)
  {
    m_imp->registerCloseHandler(hndl);
  }

  void WebsocketCppClientService::unregisterMessageHandler()
  {
    m_imp->unregisterMessageHandler();
  }

  void WebsocketCppClientService::unregisterMessageStrHandler()
  {
    m_imp->unregisterMessageStrHandler();
  }

  void WebsocketCppClientService::unregisterOpenHandler()
  {
    m_imp->unregisterOpenHandler();
  }

  void WebsocketCppClientService::unregisterCloseHandler()
  {
    m_imp->unregisterCloseHandler();
  }

  void WebsocketCppClientService::activate(const shape::Properties *props)
  {
    m_imp->activate(props);
  }

  void WebsocketCppClientService::deactivate()
  {
    m_imp->deactivate();
  }

  void WebsocketCppClientService::modify(const shape::Properties *props)
  {
    (void)props; //silence -Wunused-parameter
  }

  void WebsocketCppClientService::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void WebsocketCppClientService::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }


}
