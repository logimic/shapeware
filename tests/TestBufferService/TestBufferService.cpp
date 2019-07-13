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

  const IBufferService::Record MSG1 = { "tpc1", "msg1" };
  const IBufferService::Record MSG2 = { "tpc2", "msg2" };
  const IBufferService::Record MSG3 = { "tpc3", "msg3" };
  const IBufferService::Record MSG4 = { "tpc4", "msg4" };
  const IBufferService::Record MSG5 = { "tpc5", "msg5" };
  const IBufferService::Record MSG6 = { "tpc6", "msg6" };
  const IBufferService::Record MSG7 = { "tpc7", "msg7" };

  const IBufferService::Record MSG8 = { "tpc8", "msg8" };

  ////////////////////////////////////////////////////////
  class FixTestBufferService : public ::testing::Test
  {
  protected:

    void SetUp(void) override
    {
    }

    void TearDown(void) override
    {
    }

  };

  bool eqrec(const IBufferService::Record &rec1, const IBufferService::Record &rec2)
  {
    if (rec1.address != rec2.address) {
      return false;
    }
    if (rec1.content.size() != rec2.content.size()) {
      return false;
    }
    for (size_t i = 0; i < rec1.content.size(); i++) {
      if (rec1.content[i] != rec2.content[i]) {
        return false;
      }
    }
    return true;
  }

  TEST_F(FixTestBufferService, size)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> size");

    Imp::get().m_iBufferService->push(MSG1);
    Imp::get().m_iBufferService->push(MSG2);
    Imp::get().m_iBufferService->push(MSG3);
    Imp::get().m_iBufferService->push(MSG4);
    Imp::get().m_iBufferService->push(MSG5);
    Imp::get().m_iBufferService->push(MSG6);
    Imp::get().m_iBufferService->push(MSG7);

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
    EXPECT_TRUE(eqrec(st, MSG1));
  }

  TEST_F(FixTestBufferService, back)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> back");
    const auto & st = Imp::get().m_iBufferService->back();
    EXPECT_TRUE(eqrec(st, MSG7));
  }

  TEST_F(FixTestBufferService, push)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> push");
    Imp::get().m_iBufferService->push(MSG8);
    const auto & st = Imp::get().m_iBufferService->back();
    EXPECT_TRUE(eqrec(st, MSG8));
  }

  TEST_F(FixTestBufferService, pop)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> pop");
    Imp::get().m_iBufferService->pop();
    const auto & st = Imp::get().m_iBufferService->front();
    EXPECT_TRUE(eqrec(st, MSG2));
  }

  TEST_F(FixTestBufferService, save)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> save");
    Imp::get().m_iBufferService->save();
    EXPECT_EQ(Imp::get().m_iBufferService->size(), 0);
  }

  TEST_F(FixTestBufferService, empty1)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> empty1");
    while (!Imp::get().m_iBufferService->empty()) {
      Imp::get().m_iBufferService->pop();
    }
    EXPECT_TRUE(Imp::get().m_iBufferService->empty());
  }

  TEST_F(FixTestBufferService, load)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> load");
    Imp::get().m_iBufferService->load();
    EXPECT_EQ(Imp::get().m_iBufferService->size(), 7);
  }

  TEST_F(FixTestBufferService, checkload)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> checkLoad");
    const auto & st1 = Imp::get().m_iBufferService->front();
    EXPECT_TRUE(eqrec(st1, MSG2));
    const auto & st2 = Imp::get().m_iBufferService->back();
    EXPECT_TRUE(eqrec(st2, MSG8));
  }

}
