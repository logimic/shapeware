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
#include "ICommandService.h"
#include "IMessageService.h"
#include "ILaunchService.h"
#include "Trace.h"
#include <fstream>
#include <iostream>

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
    ICommandService* m_iCommandService = nullptr;

  public:
    Imp()
    {}

    virtual ~Imp()
    {}

    friend class Cmd;
    
    class Cmd : public ICommand {
    private:
      MsgCmd::Imp* m_msgCmd = nullptr;

    public:
      Cmd() = delete;
      Cmd(MsgCmd::Imp* msgCmd)
        :m_msgCmd(msgCmd) 
      {}

      ~Cmd() {}

      void usage(std::ostringstream& ostr)
      {
        ostr <<
          std::left << std::setw(15) << "c h" << "show help" << std::endl <<
          std::left << std::setw(15) << "c f file" << "send content of file" << std::endl <<
          std::left << std::setw(15) << "c t \"text\"" << "send text" << std::endl <<
          std::endl;
      }

      std::string doCmd(const std::string& params) override
      {
        TRC_FUNCTION_ENTER("");

        std::string cmd;
        std::string subcmd;
        std::istringstream istr(params);
        std::ostringstream ostr;

        istr >> cmd >> subcmd;

        TRC_DEBUG("process: " << PAR(subcmd));

        ///////////////////
        if (subcmd == "h") {
          usage(ostr);
        }
        ///////////////////
        else if (subcmd == "f") {
          std::string fname;
          istr >> fname;

          std::ifstream file(fname);
          if (file.is_open()) {
            std::ostringstream strStream;
            strStream << file.rdbuf();
            std::string fileContent = strStream.str();
            
            if (!fileContent.empty()) {
              m_msgCmd->m_iMessageService->sendMessage(std::vector<uint8_t>((uint8_t*)fileContent.data(), (uint8_t*)fileContent.data() + fileContent.size()));
            }
            else {
              ostr << "Empty file: " << PAR(fname);
            }
          }
          else {
            ostr << "Cannot open: " << PAR(fname);
          }
        }
        ///////////////////
        else if (subcmd == "t") {
          std::string text;
          istr >> text;

          if (!text.empty()) {
            m_msgCmd->m_iMessageService->sendMessage(std::vector<uint8_t>((uint8_t*)text.data(), (uint8_t*)text.data() + text.size()));
          }
          else {
            ostr << "Add text to send";
          }

        }
        ///////////////////
        else {
          ostr << "usage: " << std::endl;
          usage(ostr);
        }

        TRC_FUNCTION_LEAVE("");
        return ostr.str();
      }

      std::string getHelp() override
      {
        return "to communicate with msg. Type cmd h for help";
      }
    };

    void activate(const Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "MsgCmd instance activate" << std::endl <<
        "******************************"
      );

      m_iCommandService->addCommand("c", std::shared_ptr<Cmd>(shape_new Cmd(this)));

      m_iMessageService->registerMessageHandler([&](const std::vector<uint8_t>& msg)
      {
        //output received msg
        std::cout << "received: " << std::endl <<
          std::string((char*)msg.data(), msg.size()) <<
          std::endl;
      });

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

      m_iCommandService->removeCommand("c");
      m_iMessageService->unregisterMessageHandler();

      TRC_FUNCTION_LEAVE("")
    }

    void modify(const Properties *props)
    {
    }

    void attachInterface(ICommandService* iface)
    {
      m_iCommandService = iface;
    }

    void detachInterface(ICommandService* iface)
    {
      if (m_iCommandService == iface) {
        m_iCommandService = nullptr;
      }
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

  void MsgCmd::attachInterface(ICommandService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void MsgCmd::detachInterface(ICommandService* iface)
  {
    m_imp->detachInterface(iface);
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
