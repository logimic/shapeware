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

#define IRestApiService_EXPORTS

#ifdef TRC_CHANNEL
#undefine TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

#include "CppRestService.h"
#include "Trace.h"
#include <libwebsockets.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <cstring>
#include <queue>
#include <vector>

#include "shape__CppRestService.hxx"

TRC_INIT_MODULE(shape::CppRestService);

namespace shape {
  class CppRestService::Imp
  {
  public:
    void getData(const std::string & url)
    {

    }

    void registerDataHandler(DataHandlerFunc dataHandlerFunc)
    {
      m_dataHandlerFunc = dataHandlerFunc;
    }

    void unregisterDataHandler()
    {
      m_dataHandlerFunc = nullptr;
    }

    void handleData(const std::string & data)
    {
      if (m_dataHandlerFunc) {
        TRC_WARNING("Message handler is not registered");
        m_dataHandlerFunc(data);
      }
    }

    ~Imp()
    {
    }

    Imp()
    {
    }

    void activate(const shape::Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "CppRestService instance activate" << std::endl <<
        "******************************"
      );

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "CppRestService instance deactivate" << std::endl <<
        "******************************"
      );

      TRC_FUNCTION_LEAVE("")
    }

  private:

    DataHandlerFunc m_dataHandlerFunc;

  };

  ///////////////////////////////////////
  CppRestService::CppRestService()
  {
    m_imp = shape_new Imp();
  }

  CppRestService::~CppRestService()
  {
    delete m_imp;
  }

  void CppRestService::registerDataHandler(DataHandlerFunc dataHandlerFunc)
  {
    m_imp->registerDataHandler(dataHandlerFunc);
  }

  void CppRestService::unregisterDataHandler()
  {
    m_imp->unregisterDataHandler();
  }

  void CppRestService::getData(const std::string & url)
  {
    m_imp->getData(url);
  }

  void CppRestService::activate(const shape::Properties *props)
  {
    m_imp->activate(props);
  }

  void CppRestService::deactivate()
  {
    m_imp->deactivate();
  }

  void CppRestService::modify(const shape::Properties *props)
  {
  }

  void CppRestService::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void CppRestService::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }


}
