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

#include "TestWebsocketService.h"
#include "IWebsocketService.h"

#include "Trace.h"
#include <chrono>
#include <iostream>

#include "shape__TestWebsocketService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::TestWebsocketService);

namespace shape {
  TestWebsocketService::TestWebsocketService()
  {
    TRC_FUNCTION_ENTER("");
    TRC_FUNCTION_LEAVE("")
  }

  TestWebsocketService::~TestWebsocketService()
  {
    TRC_FUNCTION_ENTER("");
    TRC_FUNCTION_LEAVE("")
  }

  void TestWebsocketService::activate(const Properties *props)
  {
    TRC_FUNCTION_ENTER("");
    TRC_INFORMATION(std::endl <<
      "******************************" << std::endl <<
      "TestWebsocketService instance activate" << std::endl <<
      "******************************"
    );

    //invoking thread by lambda
    m_thread = std::thread([this]() { this->runTread(); });
    m_iWebsocketService->registerMessageHandler([&](const std::vector<uint8_t> msg)
    {
      std::string in((char*)msg.data(), msg.size());
      std::string out("Fuck your: ");
      out += in;
      std::cout << "Input: " << in << " Output: " << out << std::endl;
      //m_iWebsocketService->sendMessage(std::vector<uint8_t>((uint8_t)out.data(), out.size()));
      m_iWebsocketService->sendMessage(std::vector<uint8_t>((uint8_t*)out.data(), (uint8_t*)out.data() + out.size()));
    });

    TRC_FUNCTION_LEAVE("")
  }

  void TestWebsocketService::deactivate()
  {
    TRC_FUNCTION_ENTER("");
    TRC_INFORMATION(std::endl <<
      "******************************" << std::endl <<
      "TestWebsocketService instance deactivate" << std::endl <<
      "******************************"
    );

    //graceful thread finish
    m_runTreadFlag = false;
    if (m_thread.joinable()) {
      m_thread.join();
    }

    TRC_FUNCTION_LEAVE("")
  }

  void TestWebsocketService::modify(const Properties *props)
  {
  }

  void TestWebsocketService::attachInterface(IWebsocketService* iface)
  {
    m_iWebsocketService = iface;
  }

  void TestWebsocketService::detachInterface(IWebsocketService* iface)
  {
    if (m_iWebsocketService == iface) {
      m_iWebsocketService = nullptr;
    }
  }

  void TestWebsocketService::attachInterface(ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void TestWebsocketService::detachInterface(ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

  ////////////////////////
  void TestWebsocketService::runTread()
  {
    TRC_FUNCTION_ENTER("");

    static int num = 0;

    while (m_runTreadFlag) {
      std::cout << std::endl << num++;
      std::ostringstream os;
      os << "{\"data\": {\"counter\": " << num << "}}";
      std::string out = os.str();
      m_iWebsocketService->sendMessage(std::vector<uint8_t>((uint8_t*)out.data(), (uint8_t*)out.data() + out.size()));
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    TRC_FUNCTION_LEAVE("")
  }

}
