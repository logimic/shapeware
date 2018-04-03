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

    void getFile(const std::string & url, const std::string& fname)
    {
      TRC_FUNCTION_ENTER(PAR(url));

      auto wfname = utility::conversions::to_string_t(fname);
      // Open a stream to the file to write the HTTP response body into.
      auto fileBuffer = std::make_shared<streambuf<uint8_t>>();

      { // scope of Tasks lifetime
        auto openTask = file_buffer<uint8_t>::open(wfname, std::ios::out).then([=](streambuf<uint8_t> outFile) -> pplx::task<http_response>
        {
          *fileBuffer = outFile;

          http_client_config config;
          config.set_validate_certificates(false);

          auto wurl = utility::conversions::to_string_t(url);

          // Create an HTTP request.
          // Encode the URI query since it could contain special characters like spaces.
          http_client client(wurl, config);
          return client.request(methods::GET);
        });

        // Write the response body into the file buffer.
        auto responseTask = openTask.then([=](http_response response) -> pplx::task<size_t>
        {
          if (response.status_code() != status_codes::OK) {
            THROW_EXC_TRC_WAR(std::logic_error, "response status code: " << NAME_PAR(statusCode, (int)response.status_code()));
          }
          return response.body().read_to_end(*fileBuffer);
        });

        // Close the file buffer.
        auto writtenTask = responseTask.then([=](size_t) {
          fileBuffer->close();
        });

        // Wait for all the outstanding I/O to complete and handle any exceptions
        try
        {
          openTask.get();
          responseTask.get();
          writtenTask.get();
        }
        catch (const std::exception &e)
        {
          CATCH_EXC_TRC_WAR(std::exception, e, "When GET: " << url);
          THROW_EXC_TRC_WAR(std::logic_error, "Cannot download:" << PAR(url));
        }
      }

      TRC_FUNCTION_LEAVE("");
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

  void CppRestService::getFile(const std::string & url, const std::string& fname)
  {
    return m_imp->getFile(url, fname);
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
