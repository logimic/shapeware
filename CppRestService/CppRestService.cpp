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

#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <cpprest/http_listener.h>              // HTTP server
#include <cpprest/json.h>                       // JSON library
#include <cpprest/uri.h>                        // URI library
#include <cpprest/ws_client.h>                  // WebSocket client
#include <cpprest/containerstream.h>            // Async streams backed by STL containers
#include <cpprest/interopstream.h>              // Bridges for integrating Async streams with STL and WinRT streams
#include <cpprest/rawptrstream.h>               // Async streams backed by raw pointer to memory
#include <cpprest/producerconsumerstream.h>     // Async streams for producer consumer scenarios

using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace concurrency::streams;       // Asynchronous streams

#include "shape__CppRestService.hxx"

TRC_INIT_MODULE(shape::CppRestService);

namespace shape {
  class CppRestService::Imp
  {
  public:
    
    void getData(const std::string & url)
    {
      pplx::create_task([=]
      {
        auto wurl = utility::conversions::to_string_t(url);
        http_client client(wurl);

        return client.request(methods::GET);
      }).then([=](http_response response)
      {
        if (response.status_code() == status_codes::OK)
        {
          auto body = response.extract_string();
          //std::wstring wstr = body.get().c_str();
          //std::string rsp = utility::conversions::to_utf8string(wstr);
          std::string rsp = body.get();
          handleData((int)status_codes::OK, rsp);
        }
        else {
          handleData((int)response.status_code(), "");
        }
      });
    }

    void registerDataHandler(DataHandlerFunc dataHandlerFunc)
    {
      m_dataHandlerFunc = dataHandlerFunc;
    }

    void unregisterDataHandler()
    {
      m_dataHandlerFunc = nullptr;
    }

    void handleData(int statusCode, const std::string & data)
    {
      if (m_dataHandlerFunc) {
        TRC_WARNING("Message handler is not registered");
        m_dataHandlerFunc(statusCode, data);
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
