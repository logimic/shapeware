/**
 * Copyright 2019 Logimic,s.r.o.
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

#include "TestZeroMqService.h"
#include "IZeroMqService.h"
#include "ILaunchService.h"
#include "Args.h"
#include "GTestStaticRunner.h"

#include "Trace.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <condition_variable>
#include <random>

#include "shape__TestZeroMqService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::TestZeroMqService);

namespace shape {
  const std::string TEST_MSG = "Test message 0";
  const std::string TEST_MSG1 = "Test message 1";
  const std::string TEST_MSG2 = "Test message 2";
  const std::string TEST_MSG3 = "Test message 3";
  const std::string TEST_MSG4 = "Test message 4";
  const std::string TEST_MSG5 = "Test message 5";
  const std::string TEST_MSG6 = "Test message 6";
  const std::string ON_TIMEOUT = "OnTimeout";
  const unsigned MILLIS_WAIT = 10000;
  static int cnt = 0;

  class ClientEventHandler
  {
  public:
    ClientEventHandler()
    {
      m_onMessageFunc = [&](const std::string& msg) { messageStrHandler(msg); };
      m_onReqTimeoutFunc = [&]() { onReqTimeoutHandler(); };
    }

    std::string fetchMessage(unsigned millisToWait)
    {
      TRC_FUNCTION_ENTER(PAR(millisToWait));

      std::unique_lock<std::mutex> lck(m_mux);
      if (m_expectedMessage.empty()) {
        while (m_conVar.wait_for(lck, std::chrono::milliseconds(millisToWait)) != std::cv_status::timeout) {
          if (!m_expectedMessage.empty()) break;
        }
      }
      std::string expectedMessage = m_expectedMessage;
      m_expectedMessage.clear();
      TRC_FUNCTION_LEAVE(PAR(expectedMessage));
      return expectedMessage;
    }

    void messageStrHandler(const std::string& msg)
    {
      TRC_FUNCTION_ENTER(PAR(msg));

      std::unique_lock<std::mutex> lck(m_mux);
      m_expectedMessage = msg;
      //std::cout << m_expectedMessage << std::endl;
      m_conVar.notify_all();

      TRC_FUNCTION_LEAVE("");
    }

    void onReqTimeoutHandler()
    {
      TRC_FUNCTION_ENTER("");

      std::unique_lock<std::mutex> lck(m_mux);
      m_expectedMessage = ON_TIMEOUT;
      m_conVar.notify_all();

      TRC_FUNCTION_LEAVE("");
    }

    std::condition_variable m_conVar;
    std::mutex m_mux;
    std::string m_expectedMessage;
    IZeroMqService::OnMessageFunc m_onMessageFunc;
    IZeroMqService::OnReqTimeoutFunc m_onReqTimeoutFunc;
  };

  class Imp
  {
  private:
    Imp()
    {}

  public:
    GTestStaticRunner m_gtest;
    ILaunchService* m_iLaunchService = nullptr;

    std::map<IZeroMqService*, std::shared_ptr<ClientEventHandler>> m_iZeroMqServices;
    std::mutex m_iZeroServicesMux;

    static Imp& get() {
      static Imp imp;
      return imp;
    }

    ~Imp()
    {}

    void activate(const Properties *props)
    {
      (void)props; //silence -Wunused-parameter
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "TestZeroMqService instance activate" << std::endl <<
        "******************************"
      );

      m_gtest.runAllTests(m_iLaunchService);

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "TestZeroMqService instance deactivate" << std::endl <<
        "******************************"
      );

      TRC_FUNCTION_LEAVE("")
    }

    void modify(const Properties *props)
    {
      (void)props; //silence -Wunused-parameter
    }

    void attachInterface(IZeroMqService* iface)
    {
      std::lock_guard<std::mutex> lck(m_iZeroServicesMux);

      auto ret = m_iZeroMqServices.insert(std::make_pair(iface, std::shared_ptr<ClientEventHandler>(shape_new ClientEventHandler())));

      // client handlers
      iface->registerOnMessage(ret.first->second->m_onMessageFunc);
      iface->registerOnReqTimeout(ret.first->second->m_onReqTimeoutFunc);
    }

    void detachInterface(IZeroMqService* iface)
    {
      std::lock_guard<std::mutex> lck(m_iZeroServicesMux);
      iface->unregisterOnMessage();
      iface->unregisterOnReqTimeout();
      m_iZeroMqServices.erase(iface);
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
  TestZeroMqService::TestZeroMqService()
  {
  }

  TestZeroMqService::~TestZeroMqService()
  {
  }

  void TestZeroMqService::activate(const Properties *props)
  {
    Imp::get().activate(props);
  }

  void TestZeroMqService::deactivate()
  {
    Imp::get().deactivate();
  }

  void TestZeroMqService::modify(const Properties *props)
  {
    Imp::get().modify(props);
  }

  void TestZeroMqService::attachInterface(IZeroMqService* iface)
  {
    Imp::get().attachInterface(iface);
  }

  void TestZeroMqService::detachInterface(IZeroMqService* iface)
  {
    Imp::get().detachInterface(iface);
  }

  void TestZeroMqService::attachInterface(ILaunchService* iface)
  {
    Imp::get().attachInterface(iface);
  }

  void TestZeroMqService::detachInterface(ILaunchService* iface)
  {
    Imp::get().detachInterface(iface);
  }

  void TestZeroMqService::attachInterface(ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void TestZeroMqService::detachInterface(ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

  ////////////////////////////////////////////////////////
  class FixTestZeroMqService : public ::testing::Test
  {
  protected:
    IZeroMqService *zReq = nullptr;
    IZeroMqService *zRep = nullptr;
    ClientEventHandler *zReqH = nullptr;
    ClientEventHandler *zRepH = nullptr;
    
    void SetUp(void) override
    {
      ASSERT_EQ((size_t)2, Imp::get().m_iZeroMqServices.size());

      auto itc = Imp::get().m_iZeroMqServices.begin();
      zRep = itc->first;
      zRepH = itc->second.get();
      zReq = (++itc)->first;
      zReqH = itc->second.get();
      ASSERT_NE(nullptr, zReq);
      ASSERT_NE(nullptr, zRep);
      ASSERT_NE(nullptr, zReqH);
      ASSERT_NE(nullptr, zRepH);
      //ASSERT_TRUE(zReq->isConnected());
      //ASSERT_TRUE(zRep->isConnected());
    };

    void TearDown(void) override
    {};

  };

  /*
  TEST_F(FixTestZeroMqService, ReqSendRepRecv1)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> ReqSendRepRecv1");
    zReq->sendMessage(TEST_MSG1);
    EXPECT_EQ(TEST_MSG1, zRepH->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestZeroMqService, RepSendReqRecv1)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> RepSendReqRecv1");
    zRep->sendMessage(TEST_MSG2);
    EXPECT_EQ(TEST_MSG2, zReqH->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestZeroMqService, ReqSendRepRecv2)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> ReqSendRepRecv2");
    zReq->sendMessage(TEST_MSG3);
    EXPECT_EQ(TEST_MSG3, zRepH->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestZeroMqService, RepSendReqRecv2)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> RepSendReqRecv2");
    zRep->sendMessage(TEST_MSG4);
    EXPECT_EQ(TEST_MSG4, zReqH->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestZeroMqService, RepClose)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> RepClose");
    zRep->close();
    EXPECT_FALSE(zRep->isConnected());
  }

  TEST_F(FixTestZeroMqService, ReqTimeout)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> ReqTimeout");
    zReq->sendMessage(TEST_MSG3);
    EXPECT_EQ(ON_TIMEOUT, zReqH->fetchMessage(MILLIS_WAIT));
    //EXPECT_FALSE(zReq->isConnected());
  }

  TEST_F(FixTestZeroMqService, RepReOpen)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> RepReOpen");
    zRep->open();
    //EXPECT_TRUE(zRep->isConnected());
  }

  TEST_F(FixTestZeroMqService, ReqSendRepRecv3)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> ReqSendRepRecv3");
    zReq->sendMessage(TEST_MSG5);
    EXPECT_EQ(TEST_MSG5, zRepH->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestZeroMqService, RepSendReqRecv3)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> RepSendReqRecv3");
    zRep->sendMessage(TEST_MSG6);
    EXPECT_EQ(TEST_MSG6, zReqH->fetchMessage(MILLIS_WAIT));
    //zReq->close();
  }
  */
}
