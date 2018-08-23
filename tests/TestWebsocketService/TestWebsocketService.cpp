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
#include "IWebsocketClientService.h"

#include "Trace.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <condition_variable>

#include "shape__TestWebsocketService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::TestWebsocketService);

//#define __null nullptr

namespace shape {
  static std::map<std::string, TestWebsocketService::Imp*> s_instances;
  const std::string TEST_MSG_CLIENT = "Test message from client";
  const std::string TEST_MSG_SERVER = "Test message from server";
  const unsigned MILLIS_WAIT = 2000;
  static int cnt = 0;

  class TestWebsocketService::Imp
  {
  public:
    std::string m_instanceName;
    IWebsocketClientService* m_iWebsocketClientService = nullptr;
    IWebsocketService* m_iWebsocketService = nullptr;
    
    std::condition_variable m_msgCon;
    std::mutex m_mux;
    std::string m_expectedMessage;

    std::vector<std::string> m_connectionIdVect;
    std::thread m_thread;

    Imp()
    {}

    ~Imp()
    {}

    std::string fetchMessage(unsigned millisToWait)
    {
      TRC_FUNCTION_ENTER(PAR(millisToWait));

      std::unique_lock<std::mutex> lck(m_mux);
      if (m_expectedMessage.empty()) {
        while (m_msgCon.wait_for(lck, std::chrono::milliseconds(millisToWait)) != std::cv_status::timeout) {
          if (!m_expectedMessage.empty()) break;
        }
      }
      std::string expectedMessage = m_expectedMessage;
      m_expectedMessage.clear();
      TRC_FUNCTION_LEAVE(PAR(expectedMessage));
      return expectedMessage;
    }

    void activate(const Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "TestWebsocketService instance activate" << std::endl <<
        "******************************"
      );

      std::cout << ">>> TestWebsocketService instance activate" << std::endl;

      props->getMemberAsString("instance", m_instanceName);

      s_instances.insert(std::make_pair(m_instanceName, this));

      // server handlers
      m_iWebsocketService->registerMessageStrHandler([&](const std::string& msg, const std::string& connId)
      {
        TRC_FUNCTION_ENTER(PAR(msg) << PAR(connId));

        std::unique_lock<std::mutex> lck(m_mux);
        m_expectedMessage = msg;
        std::cout << m_expectedMessage << std::endl;
        m_msgCon.notify_all();

        TRC_FUNCTION_LEAVE("");
      });

      m_iWebsocketService->registerOpenHandler([&](const std::string& connId)
      {
        TRC_FUNCTION_ENTER(PAR(connId));
        m_connectionIdVect.push_back(connId);
        std::cout << ">>> TestWebsocketService OnOpen" << std::endl;
        TRC_FUNCTION_LEAVE("");
      });

      m_iWebsocketService->registerCloseHandler([&](const std::string& connId)
      {
        TRC_FUNCTION_ENTER(PAR(connId));
        for (auto it = m_connectionIdVect.begin(); it != m_connectionIdVect.end(); it++) {
          if (*it == connId) {
            m_connectionIdVect.erase(it);
          }
          break;
        }
        TRC_FUNCTION_LEAVE("");
      });

      // client handlers
      m_iWebsocketClientService->registerMessageStrHandler([&](const std::string& msg)
      {
        TRC_FUNCTION_ENTER(PAR(msg));

        std::unique_lock<std::mutex> lck(m_mux);
        m_expectedMessage = msg;
        std::cout << m_expectedMessage << std::endl;
        m_msgCon.notify_all();

        TRC_FUNCTION_LEAVE("");
      });

        std::cout << ">>> Start thread" << std::endl;
       m_thread = std::thread([this]() { this->runTread(); });

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "TestWebsocketService instance deactivate" << std::endl <<
        "******************************"
      );

      m_iWebsocketService->unregisterMessageStrHandler();
      m_iWebsocketService->unregisterMessageHandler();
      m_iWebsocketService->unregisterOpenHandler();
      m_iWebsocketService->unregisterCloseHandler();

      m_iWebsocketClientService->unregisterMessageStrHandler();

      s_instances.erase(m_instanceName);

      TRC_FUNCTION_LEAVE("")
    }

    void modify(const Properties *props)
    {
    }

    void attachInterface(IWebsocketClientService* iface)
    {
      m_iWebsocketClientService = iface;
    }

    void detachInterface(IWebsocketClientService* iface)
    {
      if (m_iWebsocketClientService == iface) {
        m_iWebsocketClientService = nullptr;
      }
    }

    void attachInterface(IWebsocketService* iface)
    {
      m_iWebsocketService = iface;
    }

    void detachInterface(IWebsocketService* iface)
    {
      if (m_iWebsocketService == iface) {
        m_iWebsocketService = nullptr;
      }
    }

    void runTread()
    {
      TRC_FUNCTION_ENTER("");

      //static int num = 0;

      //while (m_runTreadFlag) {
      //  num++;
      //  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      //}

      //::testing::InitGoogleTest(&argc, argv);
      //int argc = 1;
      //char argv[] = { "app" };

      char  arg0[] = "app";
      char* argv[] = { &arg0[0], NULL };
      int   argc = (int)(sizeof(argv) / sizeof(argv[0])) - 1;

      ::testing::InitGoogleTest(&argc, (char**)&argv);
      int retval = RUN_ALL_TESTS();
      std::cout << std::endl << "RUN_ALL_TESTS" << PAR(retval) << std::endl;

      //m_iLaunchService->exit(retval);

      TRC_FUNCTION_LEAVE("")
    }


  };

  TestWebsocketService::TestWebsocketService()
  {
    m_imp = shape_new Imp();
  }

  TestWebsocketService::~TestWebsocketService()
  {
    delete m_imp;
  }

  void TestWebsocketService::activate(const Properties *props)
  {
    m_imp->activate(props);
  }

  void TestWebsocketService::deactivate()
  {
    m_imp->deactivate();
  }

  void TestWebsocketService::modify(const Properties *props)
  {
    m_imp->modify(props);
  }

  void TestWebsocketService::attachInterface(IWebsocketClientService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void TestWebsocketService::detachInterface(IWebsocketClientService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void TestWebsocketService::attachInterface(IWebsocketService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void TestWebsocketService::detachInterface(IWebsocketService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void TestWebsocketService::attachInterface(ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void TestWebsocketService::detachInterface(ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

  ////////////////////////////////////////////////////////
  class FixTestWebsocketService : public ::testing::Test
  {
  protected:
    TestWebsocketService::Imp *tws1 = nullptr;
    TestWebsocketService::Imp *tws2 = nullptr;
    IWebsocketService *wss1 = nullptr;
    IWebsocketService *wss2 = nullptr;
    IWebsocketClientService *wsc1 = nullptr;
    IWebsocketClientService *wsc2 = nullptr;
    int port1 = 0;
    int port2 = 0;
    const std::string uri = "ws://localhost:";
    std::string uri1;
    std::string uri2;

    void SetUp(void) override
    {
      std::cout << ">>> SetUp" << std::endl;
      //we have 2 test instances
      ASSERT_EQ(2, s_instances.size());

      auto it = s_instances.begin();
      tws1 = it->second;
      wss1 = tws1->m_iWebsocketService;
      wsc1 = tws1->m_iWebsocketClientService;
      ++it;
      tws2 = it->second;
      wss2 = tws2->m_iWebsocketService;
      wsc2 = tws2->m_iWebsocketClientService;
      ASSERT_NE(nullptr, tws1);
      ASSERT_NE(nullptr, tws2);
      ASSERT_NE(nullptr, wss1);
      ASSERT_NE(nullptr, wsc1);
      ASSERT_NE(nullptr, wss2);
      ASSERT_NE(nullptr, wsc2);

      port1 = wss1->getPort();
      port2 = wss2->getPort();
      EXPECT_NE(0, port1);
      EXPECT_NE(0, port2);

      //TODO test bad and empty uri
      uri1 = uri;
      uri1 += std::to_string(port1);
      uri2 = uri;
      uri2 += std::to_string(port2);
    };
    
    void TearDown(void) override
    {};

  };

  TEST_F(FixTestWebsocketService, Client1Server1Message1)
  {
    std::cout << ">>> TEST_F" << std::endl;
    //EXPECT_EQ(true, wss1->isStarted());
    EXPECT_TRUE(true == wss1->isStarted());
    
    //test 1st connect
    wsc1->connect(uri1);
    EXPECT_EQ(true, wsc1->isConnected());

    std::string msg(TEST_MSG_CLIENT);
    msg += std::to_string(++cnt);
    wsc1->sendMessage(msg);
    EXPECT_EQ(msg, tws1->fetchMessage(MILLIS_WAIT));

    wsc1->close();
    //EXPECT_EQ(false, wsc1->isConnected());
    EXPECT_FALSE(true == wss1->isStarted());
  }

  TEST_F(FixTestWebsocketService, Client1Server1Message2)
  {
    //test reconnect
    wsc1->connect(uri1);
    EXPECT_EQ(true, wsc1->isConnected());

    std::string msg(TEST_MSG_CLIENT);
    msg += std::to_string(++cnt);
    wsc1->sendMessage(msg);
    EXPECT_EQ(msg, tws1->fetchMessage(MILLIS_WAIT));

    wsc1->close();
    EXPECT_EQ(false, wsc1->isConnected());
  }

  TEST_F(FixTestWebsocketService, Client1Server2Message)
  {
    wsc1->connect(uri2);
    EXPECT_EQ(true, wsc1->isConnected());

    std::string msg(TEST_MSG_CLIENT);
    msg += std::to_string(++cnt);
    wsc1->sendMessage(msg);
    EXPECT_EQ(msg, tws2->fetchMessage(MILLIS_WAIT));

    wsc1->close();
    EXPECT_EQ(false, wsc1->isConnected());
  }

  TEST_F(FixTestWebsocketService, Client2Server1Message)
  {
    wsc2->connect(uri1);
    EXPECT_EQ(true, wsc2->isConnected());

    std::string msg(TEST_MSG_CLIENT);
    msg += std::to_string(++cnt);
    wsc2->sendMessage(msg);
    EXPECT_EQ(msg, tws1->fetchMessage(MILLIS_WAIT));

    wsc2->close();
    EXPECT_EQ(false, wsc1->isConnected());
  }

  TEST_F(FixTestWebsocketService, Server1Client1Message)
  {
    wsc1->connect(uri1);
    EXPECT_EQ(true, wsc1->isConnected());

    ASSERT_GE(1, tws1->m_connectionIdVect.size());
    std::string msg(TEST_MSG_SERVER);
    msg += std::to_string(++cnt);
    wss1->sendMessage(msg, tws1->m_connectionIdVect[0]);
    EXPECT_EQ(msg, tws1->fetchMessage(MILLIS_WAIT));

    wsc1->close();
    EXPECT_EQ(false, wsc1->isConnected());
  }

  TEST_F(FixTestWebsocketService, Server2Client2Message)
  {
    wsc2->connect(uri2);
    EXPECT_EQ(true, wsc2->isConnected());

    ASSERT_GE(1, tws2->m_connectionIdVect.size());
    std::string msg(TEST_MSG_SERVER);
    msg += std::to_string(++cnt);
    wss2->sendMessage(msg, tws2->m_connectionIdVect[0]);
    EXPECT_EQ(msg, tws2->fetchMessage(MILLIS_WAIT));

    wsc2->close();
    EXPECT_EQ(false, wsc2->isConnected());
  }

  TEST_F(FixTestWebsocketService, Client12Server1Message)
  {
    wsc1->connect(uri1);
    EXPECT_EQ(true, wsc1->isConnected());
    wsc2->connect(uri1);
    EXPECT_EQ(true, wsc2->isConnected());

    {
      std::string msg(TEST_MSG_CLIENT);
      msg += std::to_string(++cnt);
      wsc1->sendMessage(msg);
      EXPECT_EQ(msg, tws1->fetchMessage(MILLIS_WAIT));
    }

    {
      std::string msg(TEST_MSG_CLIENT);
      msg += std::to_string(++cnt);
      wsc2->sendMessage(msg);
      EXPECT_EQ(msg, tws1->fetchMessage(MILLIS_WAIT));
    }
    //keep connections
  }

  TEST_F(FixTestWebsocketService, Server1Client12Message)
  {
    ASSERT_GE(2, tws1->m_connectionIdVect.size());

    {
      std::string msg(TEST_MSG_SERVER);
      msg += std::to_string(++cnt);
      wss1->sendMessage(msg, tws1->m_connectionIdVect[0]);
      EXPECT_EQ(msg, tws1->fetchMessage(MILLIS_WAIT));
    }

    {
      std::string msg(TEST_MSG_SERVER);
      msg += std::to_string(++cnt);
      wss1->sendMessage(msg, tws1->m_connectionIdVect[1]);
      EXPECT_EQ(msg, tws2->fetchMessage(MILLIS_WAIT));
    }

  }

  TEST_F(FixTestWebsocketService, Server1BroadcastMessage)
  {
    EXPECT_GE(2, tws1->m_connectionIdVect.size());

    std::string msg(TEST_MSG_SERVER);
    msg += std::to_string(++cnt);
    wss1->sendMessage(msg, "");

    EXPECT_EQ(msg, tws1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(msg, tws2->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestWebsocketService, Client1Server1MessageVect)
  {
    EXPECT_EQ(true, wsc1->isConnected());

    std::string msg(TEST_MSG_CLIENT);
    msg += std::to_string(++cnt);
    std::vector<uint8_t> msgVect((uint8_t*)msg.data(), (uint8_t*)msg.data() + msg.size());
    wsc1->sendMessage(msgVect);
    EXPECT_EQ(msg, tws1->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestWebsocketService, Server1Client1MessageVect)
  {
    EXPECT_EQ(true, wsc1->isConnected());

    ASSERT_LE(1, tws1->m_connectionIdVect.size());
    std::string msg(TEST_MSG_SERVER);
    msg += std::to_string(++cnt);
    std::vector<uint8_t> msgVect((uint8_t*)msg.data(), (uint8_t*)msg.data() + msg.size());
    wss1->sendMessage(msgVect, tws1->m_connectionIdVect[0]);
    EXPECT_EQ(msg, tws1->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestWebsocketService, Client12Close)
  {
    EXPECT_EQ(true, wsc1->isConnected());
    EXPECT_EQ(true, wsc2->isConnected());

    wsc1->close();
    EXPECT_EQ(false, wsc1->isConnected());
    wsc2->close();
    EXPECT_EQ(false, wsc2->isConnected());
  }
}
