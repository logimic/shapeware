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

/***************************************************************************
*                                  _   _ ____  _
*  Project                     ___| | | |  _ \| |
*                             / __| | | | |_) | |
*                            | (__| |_| |  _ <| |___
*                             \___|\___/|_| \_\_____|
*
* Copyright (C) 1998 - 2015, Daniel Stenberg, <daniel@haxx.se>, et al.
*
* This software is licensed as described in the file COPYING, which
* you should have received as part of this distribution. The terms
* are also available at https://curl.haxx.se/docs/copyright.html.
*
* You may opt to use, copy, modify, merge, publish, distribute and/or sell
* copies of the Software, and permit persons to whom the Software is
* furnished to do so, under the terms of the COPYING file.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
***************************************************************************/

#define IRestApiService_EXPORTS

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

#include "CurlRestApiService.h"
#include <stdio.h>
#include <curl/curl.h>

#include "Trace.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <cstring>
#include <queue>
#include <vector>

#include "shape__CurlRestApiService.hxx"

TRC_INIT_MODULE(shape::CurlRestApiService);

namespace shape {
  class CurlRestApiService::Imp
  {
  public:
    static size_t writeCallback(void *contents, size_t size, size_t nitems, FILE *file) {
      return fwrite(contents, size, nitems, file);
    }

    void getFile(const std::string & url, const std::string& fname)
    {
      TRC_FUNCTION_ENTER(PAR(url));

      CURL *curl = curl_easy_init();
      if (!curl) {
        THROW_EXC_TRC_WAR(std::logic_error, "Failed curl init");
      }

      if (CURLE_OK != curl_easy_setopt(curl, CURLOPT_URL, url.c_str()))
        THROW_EXC_TRC_WAR(std::logic_error, "Failed curl set option: " << PAR(url));
      

      FILE* file = fopen(fname.c_str(), "wb");
      if (!file)
        THROW_EXC_TRC_WAR(std::logic_error, "Could not open output file: " << PAR(fname));

      // if redirected, follow redirection
      if (CURLE_OK != curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L))
        THROW_EXC_TRC_WAR(std::logic_error, "Failed curl set option: " << PAR(CURLOPT_FOLLOWLOCATION));

      // When data arrives, curl will call writeCallback.
      if (CURLE_OK != curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback))
        THROW_EXC_TRC_WAR(std::logic_error, "Failed curl set option: " << PAR(CURLOPT_WRITEFUNCTION));

      // The last argument to writeCallback will be our file:
      if (CURLE_OK != curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)file))
        THROW_EXC_TRC_WAR(std::logic_error, "Failed curl set option: " << PAR(CURLOPT_WRITEDATA));

      /*
      * If you want to connect to a site who isn't using a certificate that is
      * signed by one of the certs in the CA bundle you have, you can skip the
      * verification of the server's certificate. This makes the connection
      * A LOT LESS SECURE.
      *
      * If you have a CA cert for the server stored someplace else than in the
      * default bundle, then the CURLOPT_CAPATH option might come handy for
      * you.
      */
      if (CURLE_OK != curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L))
        THROW_EXC_TRC_WAR(std::logic_error, "Failed curl set option: " << PAR(CURLOPT_SSL_VERIFYPEER));

      /*
      * If the site you're connecting to uses a different host name that what
      * they have mentioned in their server certificate's commonName (or
      * subjectAltName) fields, libcurl will refuse to connect. You can skip
      * this check, but this will make the connection less secure.
      */
      // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

      // Perform the request, res will get the return code
      CURLcode res = curl_easy_perform(curl);
      // Check for errors
      if (res != CURLE_OK)
        THROW_EXC_TRC_WAR(std::logic_error, "Failed curl perform: " << PAR(res) << NAME_PAR(strerror, curl_easy_strerror(res)));

      // always cleanup
      curl_easy_cleanup(curl);

      fclose(file);

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
      (void)props; //silence -Wunused-parameter
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "CurlRestApiService instance activate" << std::endl <<
        "******************************"
      );

      if (CURLE_OK != curl_global_init(CURL_GLOBAL_DEFAULT))
        THROW_EXC_TRC_WAR(std::logic_error, "Failed curl global init");

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "CurlRestApiService instance deactivate" << std::endl <<
        "******************************"
      );

      curl_global_cleanup();

      TRC_FUNCTION_LEAVE("")
    }

  private:

  };

  ///////////////////////////////////////
  CurlRestApiService::CurlRestApiService()
  {
    m_imp = shape_new Imp();
  }

  CurlRestApiService::~CurlRestApiService()
  {
    delete m_imp;
  }

  void CurlRestApiService::getFile(const std::string & url, const std::string& fname)
  {
    return m_imp->getFile(url, fname);
  }

  void CurlRestApiService::activate(const shape::Properties *props)
  {
    m_imp->activate(props);
  }

  void CurlRestApiService::deactivate()
  {
    m_imp->deactivate();
  }

  void CurlRestApiService::modify(const shape::Properties *props)
  {
    (void)props; //silence -Wunused-parameter
  }

  void CurlRestApiService::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void CurlRestApiService::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }


}
