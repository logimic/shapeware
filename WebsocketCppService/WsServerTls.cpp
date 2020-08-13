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

#include "WsServerTls.h"
#include "Trace.h"

#include <websocketpp/config/asio.hpp>

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

namespace shape {

  class WsServerTls::Imp : public WsServerTyped<websocketpp::server<websocketpp::config::asio>>
  {
  public:
  };


  ////////////////////
  WsServerTls::WsServerTls()
  {
    m_imp = shape_new WsServerTls::Imp();
  }

  WsServerTls::~WsServerTls()
  {
    delete m_imp;
  }

  void WsServerTls::run()
  {
    m_imp->run();
  }

  bool WsServerTls::is_listening()
  {
    return m_imp->is_listening();
  }

  void WsServerTls::listen(int port)
  {
    m_imp->listen(port);
  }

  void WsServerTls::start_accept()
  {
    m_imp->start_accept();
  }

  void WsServerTls::send(connection_hdl chdl, const std::string & msg)
  {
    m_imp->send(chdl, msg);
  }

  void WsServerTls::close(connection_hdl chndl, const std::string & descr, const std::string & data)
  {
    m_imp->close(chndl, descr, data);
  }

  void WsServerTls::stop_listening()
  {
    m_imp->stop_listening();
  }

  void WsServerTls::getConnParams(connection_hdl chdl, std::string & connId, websocketpp::uri_ptr & uri)
  {
    m_imp->getConnParams(chdl, connId, uri);
  }

  void WsServerTls::setOnFunctions(OnValidate onValidate, OnFail onFail, OnClose onClose, OnMessage onMessage)
  {
    m_imp->setOnFunctions(onValidate, onFail, onClose, onMessage);
  }

}
