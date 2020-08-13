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

#include "WsServer.h"
#include <string>

namespace shape {
  class WsServerPlain : public WsServer
  {
  public:
    WsServerPlain();
    ~WsServerPlain();
    void run() override;
    bool is_listening() override;
    void listen(int m_port) override;
    void start_accept() override;
    void send(connection_hdl chdl, const std::string & msg) override;
    void close(connection_hdl chndl, const std::string & descr, const std::string & data) override;
    void stop_listening() override;
    void getConnParams(connection_hdl chdl, std::string & connId, websocketpp::uri_ptr & uri) override;
    void setOnFunctions(OnValidate onValidate, OnFail onFail, OnClose onClose, OnMessage onMessage) override;

  private:
    class Imp;
    Imp* m_imp;
  };
}
