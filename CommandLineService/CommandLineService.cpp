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

#include "CommandLineService.h"
#include "ICommandService.h"
#include "Trace.h"
#include <iostream>
#include <atomic>
#include <thread>

#include "shape__CommandLineService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::CommandLineService);

namespace shape {

  class CommandLineService::Imp
  {
  private:
    shape::ICommandService* m_iCommandService = nullptr;

    std::mutex m_mux;
    std::atomic_bool m_runThdFlg;
    std::thread m_thd;

  public:
    Imp()
    {
    }

    ~Imp()
    {
    }

    void activate(const shape::Properties *props)
    {
      (void)props; //silence -Wunused-parameter
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "CommandLineService instance activate" << std::endl <<
        "******************************"
      );

      m_thd = std::thread([&]() {
        runThd();
      });

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "CommandLineService instance deactivate" << std::endl <<
        "******************************"
      );

      //std::cout << "joining CommandLineService thd ... ";
      if (m_thd.joinable()) {
        m_thd.join();
      }
      //std::cout << "done" << std::endl;

      TRC_FUNCTION_LEAVE("")
    }

    void attachInterface(shape::ICommandService* iface)
    {
      TRC_FUNCTION_ENTER("");
      m_iCommandService = iface;
      TRC_FUNCTION_LEAVE("")
    }

    void detachInterface(shape::ICommandService* iface)
    {
      TRC_FUNCTION_ENTER("");
      if (m_iCommandService == iface) {
        m_iCommandService = nullptr;
      }
      TRC_FUNCTION_LEAVE("")
    }

    void runThd()
    {
      std::string cmdLine;
      while (m_runThdFlg) {

        if (m_iCommandService->isQuiting()) {
          break;
        }

        std::cout << "cmd> ";

        std::getline(std::cin, cmdLine);

        if (std::cin.rdstate())
          break;
        if (cmdLine == "") {
          continue;
        }

        std::istringstream istr(cmdLine);
        std::string cmdStr;
        istr >> cmdStr;

        auto cmdPtr = m_iCommandService->findCommand(cmdStr);
        if (cmdPtr) {
          try {
            std::cout << cmdPtr->doCmd(istr.str()) << std::endl;
          }
          catch (std::exception &e) {
            CATCH_EXC_TRC_WAR(std::exception, e, "Command failure: " << PAR(cmdStr));
            std::cout << "Command failure with an exception: " << e.what() << std::endl;
          }
        }
        else {
          auto defaultCmd = m_iCommandService->getDefaultCommand();
          std::cout << "Unknown command: " << cmdStr << std::endl;
          if (defaultCmd) {
            std::cout << defaultCmd->doCmd(istr.str()) << std::endl;
          }
        }
      }
      std::cout << std::endl;
    }

  };

  ///////////////////////////////////////
  CommandLineService::CommandLineService()
  {
    m_imp = shape_new Imp();
  }

  CommandLineService::~CommandLineService()
  {
    delete m_imp;
  }

  void CommandLineService::activate(const shape::Properties *props)
  {
    m_imp->activate(props);
  }

  void CommandLineService::deactivate()
  {
    m_imp->deactivate();
  }

  void CommandLineService::modify(const shape::Properties *props)
  {
    (void)props; //silence -Wunused-parameter
  }

  void CommandLineService::attachInterface(shape::ICommandService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void CommandLineService::detachInterface(shape::ICommandService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void CommandLineService::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void CommandLineService::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

}
