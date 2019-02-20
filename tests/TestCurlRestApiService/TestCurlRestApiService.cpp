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

#include "TestCurlRestApiService.h"
#include "IRestApiService.h"
#include "ILaunchService.h"
#include "GTestStaticRunner.h"

#include "gtest/gtest.h"

#include "Trace.h"
#include <fstream>

#include "shape__TestCurlRestApiService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::TestCurlRestApiService);

namespace shape {

  class Imp
  {
  private:
    Imp()
    {}

  public:
    GTestStaticRunner m_gtest;
    ILaunchService* m_iLaunchService = nullptr;
    IRestApiService* m_iRestApiService = nullptr;

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
        "TestCurlRestApiService instance activate" << std::endl <<
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
        "TestCurlRestApiService instance deactivate" << std::endl <<
        "******************************"
      );

      TRC_FUNCTION_LEAVE("")
    }

    void modify(const Properties *props)
    {
      (void)props; //silence -Wunused-parameter
    }

    void attachInterface(IRestApiService* iface)
    {
      m_iRestApiService = iface;
    }

    void detachInterface(IRestApiService* iface)
    {
      if (m_iRestApiService == iface) {
        m_iRestApiService = nullptr;
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
  TestCurlRestApiService::TestCurlRestApiService()
  {
  }

  TestCurlRestApiService::~TestCurlRestApiService()
  {
  }

  void TestCurlRestApiService::activate(const Properties *props)
  {
    Imp::get().activate(props);
  }

  void TestCurlRestApiService::deactivate()
  {
    Imp::get().deactivate();
  }

  void TestCurlRestApiService::modify(const Properties *props)
  {
    Imp::get().modify(props);
  }

  void TestCurlRestApiService::attachInterface(IRestApiService* iface)
  {
    Imp::get().attachInterface(iface);
  }

  void TestCurlRestApiService::detachInterface(IRestApiService* iface)
  {
    Imp::get().detachInterface(iface);
  }

  void TestCurlRestApiService::attachInterface(ILaunchService* iface)
  {
    Imp::get().attachInterface(iface);
  }

  void TestCurlRestApiService::detachInterface(ILaunchService* iface)
  {
    Imp::get().detachInterface(iface);
  }

  void TestCurlRestApiService::attachInterface(ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void TestCurlRestApiService::detachInterface(ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

  ////////////////////////////////////////////////////////
  class FixTestCurlRestApiService : public ::testing::Test
  {
  protected:
    //estApiService *m_iRestApiService = nullptr;

    void SetUp(void) override
    {}

    void TearDown(void) override
    {}

  };

  TEST_F(FixTestCurlRestApiService, GetFile)
  {
    TRC_INFORMATION(std::endl << ">>>>>>>>>>>>>>>>>>>>>>>>>>> GetFile");

    const std::string TFILE("strandard.json");

    std::remove(TFILE.c_str());
    std::ifstream ifs(TFILE);
    ASSERT_FALSE(ifs.good());
    ifs.close();

    Imp::get().m_iRestApiService->getFile("https://repository.iqrfalliance.org/api/standards", "strandard.json");

    ifs.open(TFILE);
    ASSERT_TRUE(ifs.good());
    ifs.close();
  }

}
