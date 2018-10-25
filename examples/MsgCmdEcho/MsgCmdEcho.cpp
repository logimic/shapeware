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

#include "MsgCmdEcho.h"
#include "IMessageService.h"
#include "Trace.h"

#include "shape__MsgCmdEcho.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::MsgCmd);

namespace shape {
  class MsgCmdEcho::Imp
  {
  private:
    IMessageService* m_iMessageService = nullptr;

  public:
    Imp()
    {}

    virtual ~Imp()
    {}

    void activate(const Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "MsgCmdEcho instance activate" << std::endl <<
        "******************************"
      );

      m_iMessageService->registerMessageHandler([&](const std::vector<uint8_t>& msg)
      {
        //just echo back
        m_iMessageService->sendMessage(msg);
      });

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "MsgCmdEcho instance deactivate" << std::endl <<
        "******************************"
      );

      m_iMessageService->unregisterMessageHandler();

      TRC_FUNCTION_LEAVE("")
    }

    void modify(const Properties *props)
    {
    }

    void attachInterface(IMessageService* iface)
    {
      m_iMessageService = iface;
    }

    void detachInterface(IMessageService* iface)
    {
      if (m_iMessageService == iface) {
        m_iMessageService = nullptr;
      }
    }

  };

  /////////////////////////////////
  MsgCmdEcho::MsgCmdEcho()
  {
    m_imp = shape_new Imp();
  }

  MsgCmdEcho::~MsgCmdEcho()
  {
    delete m_imp;
  }

  void MsgCmdEcho::activate(const Properties *props)
  {
    m_imp->activate(props);
  }

  void MsgCmdEcho::deactivate()
  {
    m_imp->deactivate();
  }

  void MsgCmdEcho::modify(const Properties *props)
  {
    m_imp->modify(props);
  }

  void MsgCmdEcho::attachInterface(IMessageService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void MsgCmdEcho::detachInterface(IMessageService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void MsgCmdEcho::attachInterface(ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void MsgCmdEcho::detachInterface(ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

}
