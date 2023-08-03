#pragma once

#include "ShapeProperties.h"
#include "IMqttService.h"
#include "IMqttConnectionParsProvider.h"
#include "IIdentityProvider.h"
#include "ILaunchService.h"
#include "ITraceService.h"
#include <string>
#include <thread>

namespace oegw {
  class AwsFleetProv: public IMqttConnectionParsProvider
  {
  public:
    AwsFleetProv();
    virtual ~AwsFleetProv();

    void launchProvisioning(MqttProvisioningHandlerFunc onProvisioned) override;
    ProvisioningData getProvisioningData() const override;

    const std::string & getTopicPrefix() const override;

    void activate(const shape::Properties *props = 0);
    void deactivate();
    void modify(const shape::Properties *props);

    void attachInterface(oegw::IIdentityProvider* iface);
    void detachInterface(oegw::IIdentityProvider* iface);

    void attachInterface(shape::IMqttService* iface);
    void detachInterface(shape::IMqttService* iface);

    void attachInterface(shape::ILaunchService* iface);
    void detachInterface(shape::ILaunchService* iface);

    void attachInterface(shape::ITraceService* iface);
    void detachInterface(shape::ITraceService* iface);

  private:
    class Imp;
    Imp* m_imp;
  };
}
