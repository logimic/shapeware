#pragma once

#include "IMqttService.h" 

namespace shape {
  ////////////////
  class IMqttConnectionParsProvider
  {
  public:
    class ProvisioningData
    {
    public:
      shape::IMqttService::ConnectionPars m_connectionPars;
      std::string m_provisionedKey; //cloud generated provisioned key usable as GW ID
      bool m_isProvisioned;
    };

    typedef std::function<void(ProvisioningData provisioningData)> MqttProvisioningHandlerFunc;
    typedef std::function<void(std::string error)> MqttProvisioningHandlerErrorFunc;

    virtual void launchProvisioning(MqttProvisioningHandlerFunc onProvisioned, MqttProvisioningHandlerErrorFunc onError) = 0;
    virtual void unregisterProvisioningHandlers() = 0;
    virtual ProvisioningData getProvisioningData() const = 0;

    //topic prefix assembled from identity and provisioningKey
    virtual const std::string & getTopicPrefix() const = 0;

    virtual ~IMqttConnectionParsProvider() {};
  };
}
