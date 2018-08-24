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
#include "ILaunchService.h"

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

namespace shape {
  const std::string TEST_MSG_CLIENT = "Test message from client";
  const std::string TEST_MSG_SERVER = "Test message from server";
  const std::string OPEN_MSG_SERVER = "OnOpen connection server";
  const std::string CLOSE_MSG_SERVER = "OnClose connection server";
  const std::string OPEN_MSG_CLIENT = "OnOpen connection client";
  const std::string CLOSE_MSG_CLIENT = "OnClose connection client";
  const unsigned MILLIS_WAIT = 1000;
  static int cnt = 0;

  class ClientEventHandler
  {
  public:
    ClientEventHandler()
    {
      m_messageStrHandlerFunc = [&](const std::string& msg) { messageStrHandler(msg); };
      m_closeHandlerFunc = [&]() { closeHandler(); };
      m_openHandlerFunc = [&]() { openHandler(); };
    }

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

    void messageStrHandler(const std::string& msg)
    {
      TRC_FUNCTION_ENTER(PAR(msg));

      std::unique_lock<std::mutex> lck(m_mux);
      m_expectedMessage = msg;
      std::cout << m_expectedMessage << std::endl;
      m_msgCon.notify_all();

      TRC_FUNCTION_LEAVE("");
    }

    void openHandler()
    {
      TRC_FUNCTION_ENTER("");

      std::unique_lock<std::mutex> lck(m_mux);
      m_expectedMessage = OPEN_MSG_CLIENT;
      std::cout << ">>> TestWebsocketService OnOpen client" << std::endl;
      m_msgCon.notify_all();

      TRC_FUNCTION_LEAVE("");
    }

    void closeHandler()
    {
      TRC_FUNCTION_ENTER("");

      std::unique_lock<std::mutex> lck(m_mux);
      m_expectedMessage = CLOSE_MSG_CLIENT;
      std::cout << ">>> TestWebsocketService OnClose client" << std::endl;
      m_msgCon.notify_all();

      TRC_FUNCTION_LEAVE("");
    }

    std::condition_variable m_msgCon;
    std::mutex m_mux;
    std::string m_expectedMessage;
    IWebsocketClientService::MessageStrHandlerFunc m_messageStrHandlerFunc;
    IWebsocketClientService::OpenHandlerFunc m_openHandlerFunc;
    IWebsocketClientService::CloseHandlerFunc m_closeHandlerFunc;
  };

  class ServerEventHandler
  {
  public:
    ServerEventHandler()
    {
      m_messageStrHandlerFunc = [&](const std::string& msg, const std::string& connId) { messageStrHandler(msg, connId); };
      m_openHandlerFunc = [&](const std::string& connId) { openHandler(connId); };
      m_closeHandlerFunc = [&](const std::string& connId) { closeHandler(connId); };
    }

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

    void messageStrHandler(const std::string& msg, const std::string& connId)
    {
      TRC_FUNCTION_ENTER(PAR(msg) << PAR(connId));

      std::unique_lock<std::mutex> lck(m_mux);
      m_expectedMessage = msg;
      std::cout << m_expectedMessage << std::endl;
      m_msgCon.notify_all();

      TRC_FUNCTION_LEAVE("");
    }

    void openHandler(const std::string& connId)
    {
      TRC_FUNCTION_ENTER(PAR(connId));

      std::unique_lock<std::mutex> lck(m_mux);
      m_expectedMessage = OPEN_MSG_SERVER;
      m_connectionIdVect.push_back(connId);
      std::cout << ">>> TestWebsocketService OnOpen server" << std::endl;
      m_msgCon.notify_all();

      TRC_FUNCTION_LEAVE("");
    }

    void closeHandler(const std::string& connId)
    {
      TRC_FUNCTION_ENTER(PAR(connId));

      std::unique_lock<std::mutex> lck(m_mux);
      m_expectedMessage = CLOSE_MSG_SERVER;

      for (auto it = m_connectionIdVect.begin(); it != m_connectionIdVect.end(); it++) {
        std::cout << ">>> TestWebsocketService OnClose server compare: " << connId << " " << *it << std::endl;
        if (*it == connId) {
          m_connectionIdVect.erase(it);
        }
        break;
      }

      std::cout << ">>> TestWebsocketService OnClose server: " << PAR(m_connectionIdVect.size()) << std::endl;
      m_msgCon.notify_all();

      TRC_FUNCTION_LEAVE("");
    }

    std::condition_variable m_msgCon;
    std::mutex m_mux;
    std::string m_expectedMessage;
    IWebsocketService::MessageStrHandlerFunc m_messageStrHandlerFunc;
    IWebsocketService::OpenHandlerFunc m_openHandlerFunc;
    IWebsocketService::CloseHandlerFunc m_closeHandlerFunc;
    std::vector<std::string> m_connectionIdVect;

  };

  class TestWebsocketService::Imp
  {
  private:
    Imp()
    {}

  public:
    ILaunchService* m_iLaunchService = nullptr;

    std::string m_instanceName;
    std::map<IWebsocketClientService*, std::shared_ptr<ClientEventHandler>> m_iWebsocketClientServices;
    std::map<IWebsocketService*, std::shared_ptr<ServerEventHandler>> m_iWebsocketServices;
    std::mutex m_iWebsocketServicesMux;
    std::mutex m_iWebsocketClientServicesMux;

    std::thread m_thread;

    static Imp& get() {
      static Imp imp;
      return imp;
    }

    ~Imp()
    {}

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

      if(m_thread.joinable()) {
        m_thread.join();
      }

      TRC_FUNCTION_LEAVE("")
    }

    void modify(const Properties *props)
    {
    }

    void attachInterface(IWebsocketClientService* iface)
    {
      std::lock_guard<std::mutex> lck(m_iWebsocketClientServicesMux);

      auto ret = m_iWebsocketClientServices.insert(std::make_pair(iface, std::shared_ptr<ClientEventHandler>(shape_new ClientEventHandler())));

      // client handlers
      iface->registerMessageStrHandler(ret.first->second->m_messageStrHandlerFunc);
      iface->registerOpenHandler(ret.first->second->m_openHandlerFunc);
      iface->registerCloseHandler(ret.first->second->m_closeHandlerFunc);
    }

    void detachInterface(IWebsocketClientService* iface)
    {
      std::lock_guard<std::mutex> lck(m_iWebsocketClientServicesMux);
      iface->unregisterMessageStrHandler();
      iface->unregisterMessageHandler();
      iface->unregisterOpenHandler();
      iface->unregisterCloseHandler();
      m_iWebsocketClientServices.erase(iface);
    }

    void attachInterface(IWebsocketService* iface)
    {
      std::lock_guard<std::mutex> lck(m_iWebsocketServicesMux);

      auto ret = m_iWebsocketServices.insert(std::make_pair(iface, std::shared_ptr<ServerEventHandler>(shape_new ServerEventHandler())));
      
      // server handlers
      iface->registerMessageStrHandler(ret.first->second->m_messageStrHandlerFunc);
      iface->registerOpenHandler(ret.first->second->m_openHandlerFunc);
      iface->registerCloseHandler(ret.first->second->m_closeHandlerFunc);
    }

    void detachInterface(IWebsocketService* iface)
    {
      std::lock_guard<std::mutex> lck(m_iWebsocketServicesMux);
      iface->unregisterMessageStrHandler();
      iface->unregisterMessageHandler();
      iface->unregisterOpenHandler();
      iface->unregisterCloseHandler();
      m_iWebsocketServices.erase(iface);
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

    void runTread()
    {
      TRC_FUNCTION_ENTER("");

      char  arg0[] = "app";
      char* argv[] = { &arg0[0], NULL };
      int   argc = (int)(sizeof(argv) / sizeof(argv[0])) - 1;

      ::testing::InitGoogleTest(&argc, (char**)&argv);
      int retval = RUN_ALL_TESTS();
      std::cout << std::endl << "RUN_ALL_TESTS" << PAR(retval) << std::endl;

      m_iLaunchService->exit(retval);

      TRC_FUNCTION_LEAVE("")
    }


  };

  /////////////////////////////////
  TestWebsocketService::TestWebsocketService()
  {
  }

  TestWebsocketService::~TestWebsocketService()
  {
  }

  void TestWebsocketService::activate(const Properties *props)
  {
    Imp::get().activate(props);
  }

  void TestWebsocketService::deactivate()
  {
    Imp::get().deactivate();
  }

  void TestWebsocketService::modify(const Properties *props)
  {
    Imp::get().modify(props);
  }

  void TestWebsocketService::attachInterface(IWebsocketClientService* iface)
  {
    Imp::get().attachInterface(iface);
  }

  void TestWebsocketService::detachInterface(IWebsocketClientService* iface)
  {
    Imp::get().detachInterface(iface);
  }

  void TestWebsocketService::attachInterface(IWebsocketService* iface)
  {
    Imp::get().attachInterface(iface);
  }

  void TestWebsocketService::detachInterface(IWebsocketService* iface)
  {
    Imp::get().detachInterface(iface);
  }

  void TestWebsocketService::attachInterface(ILaunchService* iface)
  {
    Imp::get().attachInterface(iface);
  }

  void TestWebsocketService::detachInterface(ILaunchService* iface)
  {
    Imp::get().detachInterface(iface);
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
    ClientEventHandler *wsch1 = nullptr;
    ClientEventHandler *wsch2 = nullptr;
    ServerEventHandler *wssh1 = nullptr;
    ServerEventHandler *wssh2 = nullptr;
    int port1 = 0;
    int port2 = 0;
    const std::string uri = "ws://localhost:";
    std::string uri1;
    std::string uri2;

    void SetUp(void) override
    {
      std::cout << ">>> SetUp" << std::endl;
      //we have 2 pairs of test instances
      tws1 = &TestWebsocketService::Imp::get();
      tws2 = &TestWebsocketService::Imp::get();
      ASSERT_EQ(2, tws1->m_iWebsocketServices.size());
      ASSERT_EQ(2, tws1->m_iWebsocketClientServices.size());

      auto its = TestWebsocketService::Imp::get().m_iWebsocketServices.begin();
      wss1 = its->first;
      wssh1 = its->second.get();
      wss2 = (++its)->first;
      wssh2 = its->second.get();
      auto itc = TestWebsocketService::Imp::get().m_iWebsocketClientServices.begin();
      wsc1 = itc->first;
      wsch1 = itc->second.get();
      wsc2 = (++itc)->first;
      wsch2 = itc->second.get();
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

  TEST_F(FixTestWebsocketService, Server1Client1Message)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Server1Client1Message");
    wsc1->connect(uri1);
    EXPECT_EQ(OPEN_MSG_SERVER, wssh1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(OPEN_MSG_CLIENT, wsch1->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(true, wsc1->isConnected());
    //std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ASSERT_EQ(1, wssh1->m_connectionIdVect.size());
    std::string msg(TEST_MSG_SERVER);
    msg += std::to_string(++cnt);
    wss1->sendMessage(msg, wssh1->m_connectionIdVect[0]);
    EXPECT_EQ(msg, wsch1->fetchMessage(MILLIS_WAIT));

    wsc1->close();
    EXPECT_EQ(CLOSE_MSG_SERVER, wssh1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(CLOSE_MSG_CLIENT, wsch1->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(false, wsc1->isConnected());
  }

  TEST_F(FixTestWebsocketService, Server2Client2Message)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Server2Client2Message");
    wsc2->connect(uri2);
    EXPECT_EQ(OPEN_MSG_SERVER, wssh2->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(OPEN_MSG_CLIENT, wsch2->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(true, wsc2->isConnected());

    ASSERT_EQ(1, wssh2->m_connectionIdVect.size());
    std::string msg(TEST_MSG_SERVER);
    msg += std::to_string(++cnt);
    wss2->sendMessage(msg, wssh2->m_connectionIdVect[0]);
    EXPECT_EQ(msg, wsch2->fetchMessage(MILLIS_WAIT));

    wsc2->close();
    EXPECT_EQ(CLOSE_MSG_SERVER, wssh2->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(CLOSE_MSG_CLIENT, wsch2->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(false, wsc2->isConnected());
  }

  TEST_F(FixTestWebsocketService, Client1Server1Message1)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Client1Server1Message1");
    EXPECT_EQ(true, wss1->isStarted());

    //test 1st connect
    wsc1->connect(uri1);
    EXPECT_EQ(OPEN_MSG_SERVER, wssh1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(OPEN_MSG_CLIENT, wsch1->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(true, wsc1->isConnected());

    std::string msg(TEST_MSG_CLIENT);
    msg += std::to_string(++cnt);
    wsc1->sendMessage(msg);
    EXPECT_EQ(msg, wssh1->fetchMessage(MILLIS_WAIT));

    wsc1->close();
    EXPECT_EQ(CLOSE_MSG_SERVER, wssh1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(CLOSE_MSG_CLIENT, wsch1->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(false, wsc1->isConnected());
  }

  TEST_F(FixTestWebsocketService, Client1Server1Message2)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Client1Server1Message2");
    //test reconnect
    wsc1->connect(uri1);
    EXPECT_EQ(OPEN_MSG_SERVER, wssh1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(OPEN_MSG_CLIENT, wsch1->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(true, wsc1->isConnected());

    std::string msg(TEST_MSG_CLIENT);
    msg += std::to_string(++cnt);
    wsc1->sendMessage(msg);
    EXPECT_EQ(msg, wssh1->fetchMessage(MILLIS_WAIT));

    wsc1->close();
    EXPECT_EQ(CLOSE_MSG_SERVER, wssh1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(CLOSE_MSG_CLIENT, wsch1->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(false, wsc1->isConnected());
  }

  TEST_F(FixTestWebsocketService, Client1Server2Message)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Client1Server2Message");
    wsc1->connect(uri2);
    EXPECT_EQ(OPEN_MSG_SERVER, wssh2->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(OPEN_MSG_CLIENT, wsch1->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(true, wsc1->isConnected());

    std::string msg(TEST_MSG_CLIENT);
    msg += std::to_string(++cnt);
    wsc1->sendMessage(msg);
    EXPECT_EQ(msg, wssh2->fetchMessage(MILLIS_WAIT));

    wsc1->close();
    EXPECT_EQ(CLOSE_MSG_SERVER, wssh2->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(CLOSE_MSG_CLIENT, wsch1->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(false, wsc1->isConnected());
  }

  TEST_F(FixTestWebsocketService, Client2Server1Message)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Client2Server1Message");
    wsc2->connect(uri1);
    EXPECT_EQ(OPEN_MSG_SERVER, wssh1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(OPEN_MSG_CLIENT, wsch2->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(true, wsc2->isConnected());

    std::string msg(TEST_MSG_CLIENT);
    msg += std::to_string(++cnt);
    wsc2->sendMessage(msg);
    EXPECT_EQ(msg, wssh1->fetchMessage(MILLIS_WAIT));

    wsc2->close();
    EXPECT_EQ(CLOSE_MSG_SERVER, wssh1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(CLOSE_MSG_CLIENT, wsch2->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(false, wsc1->isConnected());
  }

  TEST_F(FixTestWebsocketService, Client12Server1Message)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Client12Server1Message");
    wsc1->connect(uri1);
    EXPECT_EQ(OPEN_MSG_SERVER, wssh1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(OPEN_MSG_CLIENT, wsch1->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(true, wsc1->isConnected());
    wsc2->connect(uri1);
    EXPECT_EQ(OPEN_MSG_SERVER, wssh1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(OPEN_MSG_CLIENT, wsch2->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(true, wsc2->isConnected());

    {
      std::string msg(TEST_MSG_CLIENT);
      msg += std::to_string(++cnt);
      wsc1->sendMessage(msg);
      EXPECT_EQ(msg, wssh1->fetchMessage(MILLIS_WAIT));
    }

    {
      std::string msg(TEST_MSG_CLIENT);
      msg += std::to_string(++cnt);
      wsc2->sendMessage(msg);
      EXPECT_EQ(msg, wssh1->fetchMessage(MILLIS_WAIT));
    }
    //keep connections
  }

  TEST_F(FixTestWebsocketService, Server1Client12Message)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Server1Client12Message");
    ASSERT_GE(2, wssh1->m_connectionIdVect.size());

    {
      std::string msg(TEST_MSG_SERVER);
      msg += std::to_string(++cnt);
      wss1->sendMessage(msg, wssh1->m_connectionIdVect[0]);
      EXPECT_EQ(msg, wsch1->fetchMessage(MILLIS_WAIT));
    }

    {
      std::string msg(TEST_MSG_SERVER);
      msg += std::to_string(++cnt);
      wss1->sendMessage(msg, wssh1->m_connectionIdVect[1]);
      EXPECT_EQ(msg, wsch2->fetchMessage(MILLIS_WAIT));
    }

  }

  TEST_F(FixTestWebsocketService, Server1BroadcastMessage)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Server1BroadcastMessage");
    EXPECT_GE(2, wssh1->m_connectionIdVect.size());

    std::string msg(TEST_MSG_SERVER);
    msg += std::to_string(++cnt);
    wss1->sendMessage(msg, "");

    EXPECT_EQ(msg, wsch1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(msg, wsch2->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestWebsocketService, Client1Server1MessageVect)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Client1Server1MessageVect");
    EXPECT_EQ(true, wsc1->isConnected());

    std::string msg(TEST_MSG_CLIENT);
    msg += std::to_string(++cnt);
    std::vector<uint8_t> msgVect((uint8_t*)msg.data(), (uint8_t*)msg.data() + msg.size());
    wsc1->sendMessage(msgVect);
    EXPECT_EQ(msg, wssh1->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestWebsocketService, Server1Client1MessageVect)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Server1Client1MessageVect");
    EXPECT_EQ(true, wsc1->isConnected());

    ASSERT_LE(1, wssh1->m_connectionIdVect.size());
    std::string msg(TEST_MSG_SERVER);
    msg += std::to_string(++cnt);
    std::vector<uint8_t> msgVect((uint8_t*)msg.data(), (uint8_t*)msg.data() + msg.size());
    wss1->sendMessage(msgVect, wssh1->m_connectionIdVect[0]);
    EXPECT_EQ(msg, wsch1->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestWebsocketService, Client12Close)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Client12Close");
    EXPECT_EQ(true, wsc1->isConnected());
    EXPECT_EQ(true, wsc2->isConnected());

    wsc1->close();
    EXPECT_EQ(CLOSE_MSG_SERVER, wssh1->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(CLOSE_MSG_CLIENT, wsch1->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(false, wsc1->isConnected());
    
    wsc2->close();
    EXPECT_EQ(CLOSE_MSG_SERVER, wssh1->fetchMessage(MILLIS_WAIT)); //connected to wss1 in Client12Server1Message
    EXPECT_EQ(CLOSE_MSG_CLIENT, wsch2->fetchMessage(MILLIS_WAIT));

    EXPECT_EQ(false, wsc2->isConnected());
  }
}
