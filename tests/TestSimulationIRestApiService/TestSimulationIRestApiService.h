#pragma once

#include "IRestApiService.h"
#include "ITestSimulationIRestApiService.h"
#include "ShapeProperties.h"
#include "ILaunchService.h"
#include "ITraceService.h"

namespace shape {

  class TestSimulationIRestApiService : public ITestSimulationIRestApiService
  {
  public:
    TestSimulationIRestApiService();
    virtual ~TestSimulationIRestApiService();

    void getFile(const std::string & url, const std::string& fname) override;

    std::string popIncomingRequest(unsigned millisToWait) override;
    void clearIncomingRequest() override;
    void setResourceDirectory(const std::string& dir) override;

    void activate(const shape::Properties *props = 0);
    void deactivate();
    void modify(const shape::Properties *props);

    void attachInterface(shape::ITraceService* iface);
    void detachInterface(shape::ITraceService* iface);
  private:
    class Imp;
    Imp* m_imp = nullptr;
  };

}
