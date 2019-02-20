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

#define ICommandService_EXPORTS

#include "CommandService.h"
#include "Trace.h"
#include <map>
#include <signal.h>
#include <atomic>

#include "shape__CommandService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::CommandService);

namespace shape {

  class CommandService::Imp
  {
  private:
    std::mutex m_mux;
    std::map<std::string, std::shared_ptr<ICommand>> m_commands;
    std::shared_ptr<ICommand> m_defaultCmd;
    std::atomic_bool m_quitFlg;

    //////////////////////
    class HelpCommand : public ICommand
    {
    public:
      HelpCommand() = delete;
      HelpCommand(CommandService::Imp* imp)
        :m_imp(imp)
      {
      }

      std::string doCmd(const std::string& params) override
      {
        TRC_FUNCTION_ENTER("");
        auto commands = m_imp->getCommands();
        std::ostringstream os;
        for (auto cmd : commands) {
          os << std::left << std::setw(10) << cmd.first << cmd.second->getHelp() << std::endl;
        }
        TRC_FUNCTION_LEAVE("");
        return os.str();
      }

      std::string getHelp() override
      {
        return "for help";
      }

      ~HelpCommand() {}
    private:
      CommandService::Imp* m_imp = nullptr;
    };

    //////////////////////
    class QuitCommand : public ICommand
    {
    public:
      QuitCommand(CommandService::Imp* imp)
        :m_imp(imp)
      {
      }

      std::string doCmd(const std::string& params) override
      {
        TRC_FUNCTION_ENTER("");

        std::string retval = "quit command invoked";
        
        m_imp->setQuit();
        raise(SIGINT);

        TRC_FUNCTION_LEAVE("");
        return retval;
      }

      std::string getHelp() override
      {
        return "for quit";
      }

      ~QuitCommand() {}
    private:
      CommandService::Imp* m_imp = nullptr;
    };


  ///////////////////////////////
  public:
    Imp()
     :m_quitFlg(false)
    {
    }

    ~Imp()
    {
    }

    std::map<std::string, std::shared_ptr<ICommand>> getCommands()
    {
      std::unique_lock<std::mutex> lck(m_mux);
      return m_commands;
    }
    
    void setQuit()
    {
      m_quitFlg = true;
    }

    void addCommand(const std::string & cmdStr, std::shared_ptr<ICommand> cmd)
    {
      TRC_FUNCTION_ENTER(PAR(cmdStr));

      std::unique_lock<std::mutex> lck(m_mux);
      auto ret = m_commands.insert(std::make_pair(cmdStr, cmd));
      if (ret.second == false) {
        TRC_WARNING(PAR(cmdStr) << " already registered")
      }

      TRC_FUNCTION_LEAVE("");
    }
    
    void removeCommand(const std::string & cmdStr)
    {
      TRC_FUNCTION_ENTER(PAR(cmdStr));

      std::unique_lock<std::mutex> lck(m_mux);
      if (1 != m_commands.erase(cmdStr)) {
        TRC_WARNING(PAR(cmdStr) << "isn't registered")
      }

      TRC_FUNCTION_LEAVE("");
    }

    std::shared_ptr<ICommand> findCommand(const std::string & cmdStr)
    {
      TRC_FUNCTION_ENTER(PAR(cmdStr));
      std::shared_ptr<ICommand> retval;

      std::unique_lock<std::mutex> lck(m_mux);
      auto ret = m_commands.find(cmdStr);
      if (ret != m_commands.end()) {
        retval = ret->second;
      }

      TRC_FUNCTION_LEAVE("");
      return retval;
    }

    std::shared_ptr<ICommand> getDefaultCommand()
    {
      return m_defaultCmd;
    }

    bool isQuiting() const
    {
      return m_quitFlg;
    }

    void activate(const shape::Properties *props)
    {
      (void)props; //silence -Wunused-parameter
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "CommandService instance activate" << std::endl <<
        "******************************"
      );

      m_defaultCmd = std::shared_ptr<ICommand>(shape_new HelpCommand(this));
      addCommand("h", m_defaultCmd);
      addCommand("q", std::shared_ptr<ICommand>(shape_new QuitCommand(this)));

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "CommandService instance deactivate" << std::endl <<
        "******************************"
      );

      TRC_FUNCTION_LEAVE("")
    }

  };

  ///////////////////////////////////////
  CommandService::CommandService()
  {
    m_imp = shape_new Imp();
  }

  CommandService::~CommandService()
  {
    delete m_imp;
  }

  void CommandService::addCommand(const std::string & cmdStr, std::shared_ptr<ICommand> cmd)
  {
    m_imp->addCommand(cmdStr, cmd);
  }

  void CommandService::removeCommand(const std::string & cmdStr)
  {
    m_imp->removeCommand(cmdStr);
  }

  std::shared_ptr<ICommand> CommandService::findCommand(const std::string & cmdStr)
  {
    return m_imp->findCommand(cmdStr);
  }

  std::shared_ptr<ICommand> CommandService::getDefaultCommand()
  {
    return m_imp->getDefaultCommand();
  }

  bool CommandService::isQuiting() const
  {
    return m_imp->isQuiting();
  }

  void CommandService::activate(const shape::Properties *props)
  {
    m_imp->activate(props);
  }

  void CommandService::deactivate()
  {
    m_imp->deactivate();
  }

  void CommandService::modify(const shape::Properties *props)
  {
    (void)props; //silence -Wunused-parameter
  }

  void CommandService::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void CommandService::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

}
