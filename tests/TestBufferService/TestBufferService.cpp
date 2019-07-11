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

#include "TestBufferService.h"
#include "IBufferService.h"
#include "ILaunchService.h"
#include "GTestStaticRunner.h"

#include "gtest/gtest.h"

#include "Trace.h"
#include <fstream>

#include "shape__TestBufferService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::TestBufferService);

namespace shape {

  class Imp
  {
  private:
    Imp()
    {}

  public:
    GTestStaticRunner m_gtest;
    ILaunchService* m_iLaunchService = nullptr;
    IBufferService* m_iBufferService = nullptr;

    static Imp& get()
    {
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
        "TestBufferService instance activate" << std::endl <<
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
        "TestBufferService instance deactivate" << std::endl <<
        "******************************"
      );

      TRC_FUNCTION_LEAVE("")
    }

    void modify(const Properties *props)
    {
      (void)props; //silence -Wunused-parameter
    }

    void attachInterface(IBufferService* iface)
    {
      m_iBufferService = iface;
    }

    void detachInterface(IBufferService* iface)
    {
      if (m_iBufferService == iface) {
        m_iBufferService = nullptr;
      }
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
  TestBufferService::TestBufferService()
  {
  }

  TestBufferService::~TestBufferService()
  {
  }

  void TestBufferService::activate(const Properties *props)
  {
    Imp::get().activate(props);
  }

  void TestBufferService::deactivate()
  {
    Imp::get().deactivate();
  }

  void TestBufferService::modify(const Properties *props)
  {
    Imp::get().modify(props);
  }

  void TestBufferService::attachInterface(IBufferService* iface)
  {
    Imp::get().attachInterface(iface);
  }

  void TestBufferService::detachInterface(IBufferService* iface)
  {
    Imp::get().detachInterface(iface);
  }

  void TestBufferService::attachInterface(ILaunchService* iface)
  {
    Imp::get().attachInterface(iface);
  }

  void TestBufferService::detachInterface(ILaunchService* iface)
  {
    Imp::get().detachInterface(iface);
  }

  void TestBufferService::attachInterface(ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void TestBufferService::detachInterface(ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

  const std::string MSG1("msg1");
  const std::string MSG2("msg2");
  const std::string MSG3("msg3");
  const std::string MSG4("msg4");
  const std::string MSG5("msg5");
  const std::string MSG6("msg6");
  const std::string MSG7("msg7");

  const std::string MSG8("msg8");

  ////////////////////////////////////////////////////////
  class FixTestBufferService : public ::testing::Test
  {
  protected:

    void SetUp(void) override
    {
      Imp::get().m_iBufferService->push(MSG1);
      Imp::get().m_iBufferService->push(MSG2);
      Imp::get().m_iBufferService->push(MSG3);
      Imp::get().m_iBufferService->push(MSG4);
      Imp::get().m_iBufferService->push(MSG5);
      Imp::get().m_iBufferService->push(MSG6);
      Imp::get().m_iBufferService->push(MSG7);
    }

    void TearDown(void) override
    {}

  };

  TEST_F(FixTestBufferService, size)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> size");
    EXPECT_EQ(Imp::get().m_iBufferService->size(), 7);
  }

  TEST_F(FixTestBufferService, empty0)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> empty0");
    EXPECT_FALSE(Imp::get().m_iBufferService->empty());
  }

  TEST_F(FixTestBufferService, front)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> front");
    const auto & st = Imp::get().m_iBufferService->front();
    EXPECT_EQ(st, MSG1);
  }

  TEST_F(FixTestBufferService, back)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> back");
    const auto & st = Imp::get().m_iBufferService->back();
    EXPECT_EQ(st, MSG7);
  }

  TEST_F(FixTestBufferService, push)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> push");
    Imp::get().m_iBufferService->push(MSG8);
    EXPECT_EQ(Imp::get().m_iBufferService->back(), MSG8);
  }

  TEST_F(FixTestBufferService, pop)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> pop");
    Imp::get().m_iBufferService->pop();
    EXPECT_EQ(Imp::get().m_iBufferService->front(), MSG2);
  }

  TEST_F(FixTestBufferService, empty1)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> empty1");
    while (!Imp::get().m_iBufferService->empty()) {
      Imp::get().m_iBufferService->pop();
    }
    EXPECT_TRUE(Imp::get().m_iBufferService->empty());
  }

}
