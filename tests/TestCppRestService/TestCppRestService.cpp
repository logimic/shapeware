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

#include "TestCppRestService.h"
#include "IRestApiService.h"

#include "Trace.h"
#include <chrono>
#include <iostream>

#include "shape__TestCppRestService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::TestCppRestService);

namespace shape {
  TestCppRestService::TestCppRestService()
  {
    TRC_FUNCTION_ENTER("");
    TRC_FUNCTION_LEAVE("")
  }

  TestCppRestService::~TestCppRestService()
  {
    TRC_FUNCTION_ENTER("");
    TRC_FUNCTION_LEAVE("")
  }

  void TestCppRestService::activate(const Properties *props)
  {
    TRC_FUNCTION_ENTER("");
    TRC_INFORMATION(std::endl <<
      "******************************" << std::endl <<
      "TestCppRestService instance activate" << std::endl <<
      "******************************"
    );

    //invoking thread by lambda
    m_thread = std::thread([this]() { this->runTread(); });
    m_iRestApiService->registerDataHandler([&](const std::string& data)
    {
      std::string in((char*)data.data(), data.size());
      std::string out("Fuck your: ");
      out += in;
      std::cout << "Input: " << in << " Output: " << out << std::endl;
      //m_iRestApiService->sendMessage(std::vector<uint8_t>((uint8_t*)out.data(), (uint8_t*)out.data() + out.size()));
    });

    TRC_FUNCTION_LEAVE("")
  }

  void TestCppRestService::deactivate()
  {
    TRC_FUNCTION_ENTER("");
    TRC_INFORMATION(std::endl <<
      "******************************" << std::endl <<
      "TestCppRestService instance deactivate" << std::endl <<
      "******************************"
    );

    m_iRestApiService->unregisterDataHandler();

    //graceful thread finish
    m_runTreadFlag = false;
    if (m_thread.joinable()) {
      m_thread.join();
    }

    TRC_FUNCTION_LEAVE("")
  }

  void TestCppRestService::modify(const Properties *props)
  {
  }

  void TestCppRestService::attachInterface(IRestApiService* iface)
  {
    m_iRestApiService = iface;
  }

  void TestCppRestService::detachInterface(IRestApiService* iface)
  {
    if (m_iRestApiService == iface) {
      m_iRestApiService = nullptr;
    }
  }

  void TestCppRestService::attachInterface(ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void TestCppRestService::detachInterface(ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

  ////////////////////////
  void TestCppRestService::runTread()
  {
    TRC_FUNCTION_ENTER("");

    static int num = 0;

    while (m_runTreadFlag) {
      std::cout << std::endl << num++;
      std::ostringstream os;
      os << "{\"data\": {\"counter\": " << num << "}}";
      std::string out = os.str();
      //m_iRestApiService->sendMessage(std::vector<uint8_t>((uint8_t*)out.data(), (uint8_t*)out.data() + out.size()));
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    TRC_FUNCTION_LEAVE("")
  }

}
