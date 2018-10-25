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

#include "MsgCmd.h"
#include "IMessageService.h"
#include "ILaunchService.h"
#include "Trace.h"

#include "shape__MsgCmd.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::MsgCmd);

namespace shape {
  class MsgCmd::Imp
  {
  private:
    ILaunchService* m_iLaunchService = nullptr;
    IMessageService* m_iMessageService = nullptr;

  public:
    Imp()
    {}

    ~Imp()
    {}

    void activate(const Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "MsgCmd instance activate" << std::endl <<
        "******************************"
      );

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "MsgCmd instance deactivate" << std::endl <<
        "******************************"
      );

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

    void attachInterface(ILaunchService* iface)
    {
      m_iLaunchService = iface;
    }

    void detachInterface(ILaunchService* iface)
    {
      if (m_iLaunchService == iface) {
        m_iLaunchService = nullptr;
      }
    }
  };

  /////////////////////////////////
  MsgCmd::MsgCmd()
  {
    m_imp = shape_new Imp();
  }

  MsgCmd::~MsgCmd()
  {
    delete m_imp;
  }

  void MsgCmd::activate(const Properties *props)
  {
    m_imp->activate(props);
  }

  void MsgCmd::deactivate()
  {
    m_imp->deactivate();
  }

  void MsgCmd::modify(const Properties *props)
  {
    m_imp->modify(props);
  }

  void MsgCmd::attachInterface(IMessageService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void MsgCmd::detachInterface(IMessageService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void MsgCmd::attachInterface(ILaunchService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void MsgCmd::detachInterface(ILaunchService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void MsgCmd::attachInterface(ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void MsgCmd::detachInterface(ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

}
