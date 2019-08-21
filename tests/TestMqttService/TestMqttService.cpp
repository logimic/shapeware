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

#include "TestMqttService.h"
#include "IMqttService.h"
#include "ILaunchService.h"
#include "Args.h"
#include "GTestStaticRunner.h"

#include "Trace.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <condition_variable>
#include <random>

#include "shape__TestMqttService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::TestMqttService);

namespace shape {
  const std::string TEST_MSG = "Test message 0";
  const std::string TEST_MSG1 = "Test message 1";
  const std::string TEST_MSG2 = "Test message 2";
  const std::string TEST_MSG3 = "Test message 3";
  const std::string ON_CONNECT = "OnConnect";
  const std::string ON_SUBSCRIBE = "OnSubscribe";
  const std::string ON_DISCONNECT = "OnDisconnect";
  const unsigned MILLIS_WAIT = 10000;
  static int cnt = 0;

  class ClientEventHandler
  {
  public:
    ClientEventHandler()
    {
      m_mqttMessageStrHandlerFunc = [&](const std::string& topic, const std::string& msg) { messageStrHandler(msg); };
      m_mqttOnConnectHandlerFunc = [&]() { connectHandler(); };
      m_mqttOnSubscribeHandlerFunc = [&](const std::string& topic, bool result) { subscribeHandler(topic, result); };
      m_mqttOnDisconnectHandlerFunc = [&]() { disconnectHandler(); };
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

    void connectHandler()
    {
      TRC_FUNCTION_ENTER("");

      std::unique_lock<std::mutex> lck(m_mux);
      m_expectedMessage = ON_CONNECT;
      //std::cout << ">>> TestMqttService OnOpen client" << std::endl;
      m_conVar.notify_all();

      TRC_FUNCTION_LEAVE("");
    }

    void subscribeHandler(const std::string & topic, bool result)
    {
      TRC_FUNCTION_ENTER(PAR(topic) << PAR(result));

      std::unique_lock<std::mutex> lck(m_mux);
      m_expectedMessage = ON_SUBSCRIBE;
      ////std::cout << ">>> TestMqttService OnClose client" << std::endl;
      m_conVar.notify_all();

      TRC_FUNCTION_LEAVE("");
    }

    void disconnectHandler()
    {
      TRC_FUNCTION_ENTER("");

      std::unique_lock<std::mutex> lck(m_mux);
      m_expectedMessage = ON_DISCONNECT;
      //std::cout << ">>> TestMqttService OnClose client" << std::endl;
      m_conVar.notify_all();

      TRC_FUNCTION_LEAVE("");
    }

    std::condition_variable m_conVar;
    std::mutex m_mux;
    std::string m_expectedMessage;
    IMqttService::MqttMessageStrHandlerFunc m_mqttMessageStrHandlerFunc;
    IMqttService::MqttOnConnectHandlerFunc m_mqttOnConnectHandlerFunc;
    IMqttService::MqttOnSubscribeHandlerFunc m_mqttOnSubscribeHandlerFunc;
    IMqttService::MqttOnDisconnectHandlerFunc m_mqttOnDisconnectHandlerFunc;
  };

  class Imp
  {
  private:
    Imp()
    {}

  public:
    GTestStaticRunner m_gtest;
    ILaunchService* m_iLaunchService = nullptr;

    std::map<IMqttService*, std::shared_ptr<ClientEventHandler>> m_iMqttServices;
    std::mutex m_iMqttServicesMux;

    std::random_device rd;
    std::string m_client1 = "mqttlgmc1_" + std::to_string(rd());
    std::string m_client2 = "mqttlgmc2_" + std::to_string(rd());
    std::string m_topic = "mqttlgmc_topic" + std::to_string(rd());

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
        "TestMqttService instance activate" << std::endl <<
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
        "TestMqttService instance deactivate" << std::endl <<
        "******************************"
      );

      TRC_FUNCTION_LEAVE("")
    }

    void modify(const Properties *props)
    {
      (void)props; //silence -Wunused-parameter
    }

    void attachInterface(IMqttService* iface)
    {
      std::lock_guard<std::mutex> lck(m_iMqttServicesMux);

      auto ret = m_iMqttServices.insert(std::make_pair(iface, std::shared_ptr<ClientEventHandler>(shape_new ClientEventHandler())));

      // client handlers
      iface->registerMessageStrHandler(ret.first->second->m_mqttMessageStrHandlerFunc);
      iface->registerOnConnectHandler(ret.first->second->m_mqttOnConnectHandlerFunc);
      iface->registerOnSubscribeHandler(ret.first->second->m_mqttOnSubscribeHandlerFunc);
      iface->registerOnDisconnectHandler(ret.first->second->m_mqttOnDisconnectHandlerFunc);
    }

    void detachInterface(IMqttService* iface)
    {
      std::lock_guard<std::mutex> lck(m_iMqttServicesMux);
      iface->unregisterMessageStrHandler();
      iface->unregisterOnConnectHandler();
      iface->unregisterOnSubscribeHandler();
      iface->unregisterOnDisconnectHandler();
      m_iMqttServices.erase(iface);
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
  TestMqttService::TestMqttService()
  {
  }

  TestMqttService::~TestMqttService()
  {
  }

  void TestMqttService::activate(const Properties *props)
  {
    Imp::get().activate(props);
  }

  void TestMqttService::deactivate()
  {
    Imp::get().deactivate();
  }

  void TestMqttService::modify(const Properties *props)
  {
    Imp::get().modify(props);
  }

  void TestMqttService::attachInterface(IMqttService* iface)
  {
    Imp::get().attachInterface(iface);
  }

  void TestMqttService::detachInterface(IMqttService* iface)
  {
    Imp::get().detachInterface(iface);
  }

  void TestMqttService::attachInterface(ILaunchService* iface)
  {
    Imp::get().attachInterface(iface);
  }

  void TestMqttService::detachInterface(ILaunchService* iface)
  {
    Imp::get().detachInterface(iface);
  }

  void TestMqttService::attachInterface(ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void TestMqttService::detachInterface(ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

  ////////////////////////////////////////////////////////
  class FixTestMqttService : public ::testing::Test
  {
  protected:
    Imp *tms1 = nullptr;
    Imp *tms2 = nullptr;
    IMqttService *mqttc1 = nullptr;
    IMqttService *mqttc2 = nullptr;
    ClientEventHandler *mqttch1 = nullptr;
    ClientEventHandler *mqttch2 = nullptr;
    const std::string uri = "test.mosquitto.org:1883";
    
    std::random_device rd;
    std::string client1 = "mqttlgmc1_" + std::to_string(rd());
    std::string client2 = "mqttlgmc2_" + std::to_string(rd());
    std::string topic = "mqttlgmc_topic" + std::to_string(rd());

    void SetUp(void) override
    {
      //std::cout << ">>> SetUp" << std::endl;
      //we have 2 pairs of test instances
      tms1 = &Imp::get();
      tms2 = &Imp::get();
      ASSERT_EQ((size_t)2, tms1->m_iMqttServices.size());

      client1 = Imp::get().m_client1;
      client2 = Imp::get().m_client2;
      topic = Imp::get().m_topic;

      auto itc = Imp::get().m_iMqttServices.begin();
      mqttc1 = itc->first;
      mqttch1 = itc->second.get();
      mqttc2 = (++itc)->first;
      mqttch2 = itc->second.get();
      ASSERT_NE(nullptr, mqttc1);
      ASSERT_NE(nullptr, mqttc2);
      ASSERT_NE(nullptr, mqttch1);
      ASSERT_NE(nullptr, mqttch1);
    };

    void TearDown(void) override
    {};

  };

  TEST_F(FixTestMqttService, Mqttc1Connect)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Mqttc1Connect");
    mqttc1->create(client1);
    mqttc1->connect();
    EXPECT_EQ(ON_CONNECT, mqttch1->fetchMessage(MILLIS_WAIT));
    EXPECT_TRUE(mqttc1->isReady());
  }

  TEST_F(FixTestMqttService, Mqttc2Connect)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Mqttc2Connect");
    mqttc2->create(client2);
    mqttc2->connect();
    EXPECT_EQ(ON_CONNECT, mqttch2->fetchMessage(MILLIS_WAIT));
    EXPECT_TRUE(mqttc2->isReady());
  }

  TEST_F(FixTestMqttService, Mqttc2Subscribe)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Mqttc2Subscribe");
    mqttc2->subscribe(topic);
    EXPECT_EQ(ON_SUBSCRIBE, mqttch2->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestMqttService, Mqttc1PublishMqttc2Receive)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Mqttc1PublishMqttc2Receive");
    mqttc1->publish(topic, TEST_MSG);
    EXPECT_EQ(TEST_MSG, mqttch2->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestMqttService, Mqttc1Disconnect)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Mqttc1Disconnect");
    mqttc1->disconnect();
    EXPECT_EQ(ON_DISCONNECT, mqttch1->fetchMessage(MILLIS_WAIT));
    EXPECT_FALSE(mqttc1->isReady());
  }

  TEST_F(FixTestMqttService, Mqttc1PublishDisconMqttc2Receive)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Mqttc1PublishDisconMqttc2Receive");
    mqttc1->publish(topic, TEST_MSG1);
    mqttc1->publish(topic, TEST_MSG2);
    mqttc1->publish(topic, TEST_MSG3);

    mqttc1->connect();
    EXPECT_EQ(ON_CONNECT, mqttch1->fetchMessage(MILLIS_WAIT));
    EXPECT_TRUE(mqttc1->isReady());

    EXPECT_EQ(TEST_MSG1, mqttch2->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(TEST_MSG2, mqttch2->fetchMessage(MILLIS_WAIT));
    EXPECT_EQ(TEST_MSG3, mqttch2->fetchMessage(MILLIS_WAIT));
  }

  TEST_F(FixTestMqttService, Mqttc1Disconnect2)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> Mqttc1Disconnect2");
    mqttc1->disconnect();
    EXPECT_EQ(ON_DISCONNECT, mqttch1->fetchMessage(MILLIS_WAIT));
    EXPECT_FALSE(mqttc1->isReady());
  }

}
