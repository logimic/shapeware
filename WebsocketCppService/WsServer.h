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

#pragma once

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_INTERNAL_

#include "IWebsocketService.h"
#include "LogStream.h"
#include <websocketpp/server.hpp>
#include "Trace.h"

typedef websocketpp::connection_hdl connection_hdl;
//typedef websocketpp::config::core::message_type::ptr message_ptr;

namespace shape {
  class WsServer
  {
  public:
    WsServer()
    {}

    virtual ~WsServer() {}
    virtual void run() = 0;
    virtual bool is_listening() = 0;
    virtual void listen(int port) = 0;
    virtual void start_accept() = 0;
    virtual void send(connection_hdl chdl, const std::string & msg) = 0;
    virtual void close(connection_hdl chndl, const std::string & descr, const std::string & data) = 0;
    virtual void stop_listening() = 0;
    virtual void getConnParams(connection_hdl chdl, std::string & connId, websocketpp::uri_ptr & uri) = 0;
    
    typedef std::function<bool(connection_hdl hdl, const std::string & connId, const std::string &, const std::string &)> OnValidate;
    typedef std::function<void(connection_hdl hdl)> OnFail;
    typedef std::function<void(connection_hdl hdl)> OnClose;
    typedef std::function<void(connection_hdl hdl, std::string msg)> OnMessage;

    virtual void setOnFunctions(OnValidate onValidate, OnFail onFail, OnClose onClose, OnMessage onMessage) = 0;
  };

  template<typename T>
  class WsServerTyped
  {
  public:
    typedef typename T::message_ptr MsgPtr;

    ~WsServerTyped()
    {}

    WsServerTyped()
      :m_wsLogerOs(&m_wsLoger)
    {
      // set up access channels to only log interesting things
      m_server.set_access_channels(websocketpp::log::alevel::all);
      m_server.set_access_channels(websocketpp::log::elevel::all);

      // Set custom logger (ostream-based).
      m_server.get_alog().set_ostream(&m_wsLogerOs);
      m_server.get_elog().set_ostream(&m_wsLogerOs);

      // Initialize Asio
      m_server.init_asio();

      m_server.set_validate_handler([&](connection_hdl hdl)->bool {
        //TODO on_connection can be use instead, however we're ready for authentication by a token
        TRC_FUNCTION_ENTER("");
        bool valid = false;

        std::string connId;
        websocketpp::uri_ptr uri;
        
        getConnParams(hdl, connId, uri);
        //void getConnParams(connection_hdl chdl, std::string & connId, websocketpp::uri_ptr & uri) override
        //{
          //auto con = m_server.get_con_from_hdl(hdl);

          //std::ostringstream os;
          //os << con->get_handle().lock().get();
          //connId = os.str();

          //uri = con->get_uri();
        //}


        std::string query = uri->get_query(); // returns empty string if no query string set.
        std::string host = uri->get_host();

        //if (m_acceptOnlyLocalhost) {
        //  if (host == "localhost" || host == "127.0.0.1" || host == "[::1]") {
        //    valid = true;
        //  }
        //  else {
        //    valid = false;
        //    TRC_INFORMATION("Connection refused: " << PAR(connId) << PAR(host));;
        //  }
        //}

        if (m_onValidate) {
          valid = m_onValidate(hdl, connId, host, query);
        }
        else {
          TRC_WARNING("onValidate not set");
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

          //{
          //  std::unique_lock<std::mutex> lock(m_mux);
          //  m_connectionsStrMap.insert(std::make_pair(hdl, connId));
          //}

          //if (m_openHandlerFunc) {
          //  m_openHandlerFunc(connId);
          //}
          //else {
          //  TRC_WARNING("Message handler is not registered");
          //}
        }
        TRC_FUNCTION_LEAVE(PAR(valid));
        return valid;
      });

      m_server.set_fail_handler([&](connection_hdl hdl) {
        if (m_onFail) {
          m_onFail(hdl);
        }
        else {
          TRC_WARNING("m_onFail not set");
        }
      });

      m_server.set_close_handler([&](connection_hdl hdl) {
        //on_close(hdl);
        if (m_onClose) {
          m_onClose(hdl);
        }
        else {
          TRC_WARNING("onClose not set");
        }
      });

      m_server.set_message_handler([&](connection_hdl hdl, MsgPtr msg) {
        TRC_FUNCTION_ENTER("");

        std::string msgPayload = msg->get_payload().data();

        if (m_onMessage) {
          m_onMessage(hdl, msgPayload);
        }
        else {
          TRC_WARNING("onMessage");
        }
      });
    }

    //void setOnFunctions(OnValidate onValidate, OnFail onFail, OnClose onClose, OnMessage onMessage)
    //{
    //  m_server.set_validate_handler([&](connection_hdl hdl)->bool {
    //    return on_validate<T>(hdl);
    //  });

    //  server.set_fail_handler([&](connection_hdl hdl) {
    //    TRC_FUNCTION_ENTER("on_fail(): ");
    //    auto con = server.get_con_from_hdl(hdl);
    //    websocketpp::lib::error_code ec = con->get_ec();
    //    TRC_WARNING("on_fail(): Error: " << NAME_PAR(hdl, hdl.lock().get()) << " " << ec.message());
    //    TRC_FUNCTION_LEAVE("");
    //  });

    //  server.set_close_handler([&](connection_hdl hdl) {
    //    on_close(hdl);
    //  });

    //  server.set_message_handler([&](connection_hdl hdl, message_ptr msg) {
    //    on_message(hdl, msg);
    //  });

    //}

    void run()
    {
      m_server.run();
    }

    bool is_listening()
    {
      return m_server.is_listening();
    }

    void listen(int port)
    {
      m_server.set_reuse_addr(true);
      m_server.listen(port);
    }

    void start_accept()
    {
      m_server.start_accept();
    }

    void send(connection_hdl chdl, const std::string & msg)
    {
      websocketpp::lib::error_code ec;
      m_server.send(chdl, msg, websocketpp::frame::opcode::text, ec); // send text message.
      if (ec) {
        auto conState = m_server.get_con_from_hdl(chdl)->get_state();
        TRC_WARNING("Cannot send message: " << PAR(conState) << ec.message());
      }
    }

    void close(connection_hdl chndl, const std::string & descr, const std::string & data)
    {
      websocketpp::lib::error_code ec;
      m_server.close(chndl, websocketpp::close::status::normal, data, ec); // send text message.
      if (ec) { // we got an error
         // Error closing websocket. Log reason using ec.message().
        TRC_WARNING("close connection: " << PAR(descr) << ec.message());
      }
    }

    void stop_listening()
    {
      websocketpp::lib::error_code ec;
      m_server.stop_listening(ec);
      if (ec) {
        // Failed to stop listening. Log reason using ec.message().
        TRC_INFORMATION("Failed stop_listening: " << ec.message());
      }
    }

    void getConnParams(connection_hdl chdl, std::string & connId, websocketpp::uri_ptr & uri)
    {
      auto con = m_server.get_con_from_hdl(chdl);

      std::ostringstream os;
      os << con->get_handle().lock().get();
      connId = os.str();

      uri = con->get_uri();
    }

    T & getServer()
    {
      return m_server;
    }

    void setOnFunctions(WsServer::OnValidate onValidate, WsServer::OnFail onFail, WsServer::OnClose onClose, WsServer::OnMessage onMessage)
    {
      m_onValidate = onValidate;
      m_onFail = onFail;
      m_onClose = onClose;
      m_onMessage = onMessage;
    }

  private:
    T m_server;
    LogStream m_wsLoger;
    std::ostream m_wsLogerOs;

    WsServer::OnValidate m_onValidate;
    WsServer::OnFail m_onFail;
    WsServer::OnClose m_onClose;
    WsServer::OnMessage m_onMessage;

  };
}
