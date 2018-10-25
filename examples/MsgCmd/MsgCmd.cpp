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

#include "shape__MsgCmd.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::MsgCmd);

namespace shape {
  class MsgCmd::Imp : public ICommand, public std::enable_shared_from_this<ICommand>
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

    void usage(std::ostringstream& ostr)
    {
      ostr <<
        std::left << std::setw(15) << "c h" << "show help" << std::endl <<
        //std::left << std::setw(15) << "fr m <age>" << "add male face" << std::endl <<
        //std::left << std::setw(15) << "fr f <age>" << "add female face" << std::endl <<
        //std::left << std::setw(15) << "fr c" << "remove all faces" << std::endl <<
        //std::left << std::setw(15) << "fr l" << "list all faces" << std::endl <<
        //std::left << std::setw(15) << "fr r <label>" << "remove face with <label> or the last if <label> not provided" <<
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

      if (subcmd == "h") {
        usage(ostr);
      }
      //else if (subcmd == "m" || subcmd == "f") {
      //  bool male = subcmd == "m";
      //  auto & faces = m_imp->getFaces();
      //  if (faces.size() < 16) {
      //    int age = -1;
      //    istr >> age;

      //    Face face;
      //    face.age = age > 0 ? age : 40;
      //    face.maleProb = male ? 0.9 : 0.1;

      //    int label = m_imp->addFace(face);
      //    ostr << "added: " << face;
      //  }
      //  else {
      //    ostr << "maximum faces: 16";
      //  }
      //}
      //else if (subcmd == "c") {
      //  auto & faces = m_imp->getFaces();
      //  size_t sz = faces.size();
      //  m_imp->remFaceAll();
      //  ostr << sz << " faces removed";
      //}
      //else if (subcmd == "l") {
      //  auto & faces = m_imp->getFaces();
      //  ostr << "faces: " << faces.size() << std::endl;
      //  for (auto face : faces) {
      //    ostr << face.second << std::endl;
      //  }
      //}
      //else if (subcmd == "r") {
      //  int label = -1;
      //  istr >> label;

      //  auto & faces = m_imp->getFaces();

      //  if (faces.size() > 0) {
      //    if (label <= 0) {
      //      auto rit = faces.rbegin();
      //      label = rit->first;
      //    }
      //    auto it = faces.find(label);
      //    if (it != faces.end()) {
      //      Face face = it->second;
      //      m_imp->remFace(it->first);
      //      ostr << "removed: " << face;
      //    }
      //    else {
      //      ostr << "label: " << label << " doesn't exist";
      //    }
      //  }
      //  else {
      //    ostr << "nothing to remove";
      //  }
      //}
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

    void activate(const Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "MsgCmd instance activate" << std::endl <<
        "******************************"
      );

      auto ptr = shared_from_this();
      m_iCommandService->addCommand("c", ptr);

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
