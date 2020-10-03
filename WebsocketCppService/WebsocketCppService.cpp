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

#include "WsServerPlain.h"
#include "WsServerTls.h"

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/pointer.h"

#include "shape__WebsocketCppService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::WebsocketCppService);

namespace shape {

  typedef websocketpp::connection_hdl connection_hdl;

  class WebsocketCppService::Imp
  {
  private:
    shape::ILaunchService* m_iLaunchService = nullptr;

    std::unique_ptr<WsServer> m_server;

    int m_port = 1338;

    std::mutex m_mux;
    std::map<connection_hdl, std::string, std::owner_less<connection_hdl>> m_connectionsStrMap;

    bool m_autoStart = true;
    bool m_acceptOnlyLocalhost = false;
    bool m_tlsEnabled = false;
    std::string m_tlsMode = "intermediate";
    std::string m_cert;
    std::string m_key;

    bool m_runThd = false;
    std::thread m_thd;

    MessageHandlerFunc m_messageHandlerFunc;
    MessageStrHandlerFunc m_messageStrHandlerFunc;
    OpenHandlerFunc m_openHandlerFunc;
    CloseHandlerFunc m_closeHandlerFunc;

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

    void on_message(connection_hdl hdl, std::string msg)
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
          m_messageStrHandlerFunc(msg, connId);
          found = true;
        }

        if (m_messageHandlerFunc) {
          uint8_t* buf = (uint8_t*)msg.data();
          std::vector<uint8_t> vmsg(buf, buf + msg.size());
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

    void on_fail(connection_hdl chdl, std::string errstr)
    {
      TRC_FUNCTION_ENTER("on_fail(): ");
      TRC_WARNING("on_fail(): Error: " << NAME_PAR(hdl, chdl.lock().get()) << " " << errstr);
      TRC_FUNCTION_LEAVE("");
    }

    bool on_validate(connection_hdl chdl, const std::string & connId, const std::string & host, const std::string & query)
    {
      //TODO on_connection can be use instead, however we're ready for authentication by a token
      TRC_FUNCTION_ENTER("");
      bool valid = true;

      if (m_acceptOnlyLocalhost) {
        if (host == "localhost" || host == "127.0.0.1" || host == "[::1]") {
          valid = true;
        }
        else {
          valid = false;
        }
      }

      if (valid) {
        if (!query.empty()) {
          // Split the query parameter string here, if desired.
          // We assume we extracted a string called 'id' here.
        }
        else {
          // Reject if no query parameter provided, for example.
          //return false;
        }

        TRC_INFORMATION("Connected: " << PAR(connId) << PAR(host));;

        {
          std::unique_lock<std::mutex> lock(m_mux);
          m_connectionsStrMap.insert(std::make_pair(chdl, connId));
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

#if 0
    //TODO for future use
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
#endif

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

  public:
    Imp()
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
            m_server->send(it.first, msg); // send text message.
          }
        }
        else {
          for (auto it : m_connectionsStrMap) {
            if (it.second == connId) {
              m_server->send(it.first, msg); // send text message.
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
        m_server->listen(m_port);

        // Starting Websocket accept.
        m_server->start_accept();
      }
      catch (websocketpp::exception const &e) {
        // Websocket exception on listen. Get char string via e.what().
        CATCH_EXC_TRC_WAR(websocketpp::exception, e, "listen or start_accept failed");
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
        if (m_server->is_listening()) {
          m_server->stop_listening();
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
          m_server->close(con.first, con.second, data); // send text message.
        }

        // clear all existing websocket connections mapping.
        {
          std::unique_lock<std::mutex> lock(m_mux);
          m_connectionsStrMap.clear();
        }

        // Stop the endpoint.
        TRC_INFORMATION("stop server");

        if (m_thd.joinable()) {
          //std::cout << "Joining WsServerBase thread ..." << std::endl;
          m_thd.join();
          //std::cout << "WsServerBase thread joined" << std::endl;
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

    std::string getPath(const std::string &path)
    {
      if (path.empty()) {
        return "";
      }
      if (path.at(0) == '/') {
        return path;
      }
      std::string configDir = m_iLaunchService->getConfigurationDir();
      return configDir + "/certs/" + path;
    }

    void activate(const shape::Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "WebsocketCppService instance activate" << std::endl <<
        "******************************"
      );

      using namespace rapidjson;

      const Document& doc = props->getAsJson();

      {
        const Value* v = Pointer("/WebsocketPort").Get(doc);
        if (v && v->IsInt()) {
          m_port = v->GetInt();
        }
        else {
          TRC_WARNING("WebsocketPort not specified => used default: " << PAR(m_port));
        }
      }

      {
        const Value* v = Pointer("/AutoStart").Get(doc);
        if (v && v->IsBool()) {
          m_port = v->GetBool();
        }
        else {
          TRC_WARNING("AutoStart not specified => used default: " << PAR(m_autoStart));
        }
      }

      {
        const Value* v = Pointer("/acceptOnlyLocalhost").Get(doc);
        if (v && v->IsBool()) {
          m_acceptOnlyLocalhost = v->GetBool();
        }
        else {
          TRC_WARNING("acceptOnlyLocalhost not specified => used default: " << PAR(m_acceptOnlyLocalhost));
        }
      }

      {
        const Value* v = Pointer("/tlsEnabled").Get(doc);
        if (v && v->IsBool()) {
          m_tlsEnabled = v->GetBool();
        }
        else {
          TRC_WARNING("TLS enablement not specified => used default: " << PAR(m_tlsEnabled));
        }
      }

      {
        const Value* v = Pointer("/tlsMode").Get(doc);
        if (v && v->IsString()) {
          m_tlsMode = v->GetString();
        } else {
          TRC_WARNING("TLS mode not specified => used default: " << PAR(m_tlsMode));
        }
      }

      {
        const Value* v = Pointer("/certificate").Get(doc);
        if (v && v->IsString()) {
          m_cert = v->GetString();
        }
        else {
          TRC_WARNING("Certificate not specified => used default: " << PAR(m_cert));
        }
      }

      {
        const Value* v = Pointer("/privateKey").Get(doc);
        if (v && v->IsString()) {
          m_key = v->GetString();
        }
        else {
          TRC_WARNING("Private key not specified => used default: " << PAR(m_key));
        }
      }

      TRC_INFORMATION(PAR(m_port) << PAR(m_autoStart) << PAR(m_acceptOnlyLocalhost) << PAR(m_tlsEnabled) << PAR(m_cert) << PAR(m_key));

      m_cert = getPath(m_cert);
      m_key = getPath(m_key);

      if (!m_tlsEnabled) {
        std::unique_ptr<WsServerPlain> ptr = std::unique_ptr<WsServerPlain>(shape_new WsServerPlain);
        ptr->setOnFunctions(
          [&](connection_hdl hdl, const std::string & connId, const std::string & host, const std::string & query) { return on_validate(hdl, connId, host, query); }
        , [&](connection_hdl hdl, const std::string &errstr) { on_fail(hdl, errstr); }
        , [&](connection_hdl hdl) { on_close(hdl); }
          , [&](connection_hdl hdl, const std::string &msg) { on_message(hdl, msg); }
          );
        m_server = std::move(ptr);
      } else {
        std::unique_ptr<WsServerTls> ptr = std::unique_ptr<WsServerTls>(shape_new WsServerTls);
        ptr->setOnFunctions(
          [&](connection_hdl hdl, const std::string & connId, const std::string & host, const std::string & query) { return on_validate(hdl, connId, host, query); }
        , [&](connection_hdl hdl, const std::string &errstr) { on_fail(hdl, errstr); }
        , [&](connection_hdl hdl) { on_close(hdl); }
        , [&](connection_hdl hdl, const std::string &msg) { on_message(hdl, msg); }
        );
        ptr->setTls(m_tlsMode, m_cert, m_key);
        m_server = std::move(ptr);
      }

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

    void attachInterface(shape::ILaunchService* iface)
    {
      m_iLaunchService = iface;
    }

    void detachInterface(shape::ILaunchService* iface)
    {
      if (m_iLaunchService == iface) {
        m_iLaunchService = nullptr;
      }
    }

  private:

    void runThd()
    {
      TRC_FUNCTION_ENTER("");

      while (m_runThd) {
        // Start the ASIO io_service run loop
        try {
          m_server->run();
        } catch (websocketpp::exception const & e) {
          CATCH_EXC_TRC_WAR(websocketpp::exception, e, "Unexpected Asio error: ")
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

  void WebsocketCppService::attachInterface(shape::ILaunchService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void WebsocketCppService::detachInterface(shape::ILaunchService* iface)
  {
    m_imp->detachInterface(iface);
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
