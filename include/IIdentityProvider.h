#pragma once

#include <string>

namespace oegw {
  ////////////////
  class IIdentityProvider
  {
  public:
    class IdentityParams
    {
    public:
      std::string m_topicRoot;
      std::string m_devStage;
      std::string m_vendor;
      std::string m_product;
      std::string m_serialNumber;
      std::string m_gwType;
    };

    virtual const IdentityParams & getParams() const = 0;

    virtual ~IIdentityProvider() {};
  };
}
