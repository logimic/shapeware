#pragma once

#include "IMessageService.h"
#include "ShapeProperties.h"
#include "ITraceService.h"
#include <string>

namespace shape {
  class MqMessageService : public IMessageService
  {
  public:
    MqMessageService();
    virtual ~MqMessageService();

    void registerMessageHandler(MessageHandlerFunc hndl) override;
    void unregisterMessageHandler() override;
    void sendMessage(const std::vector<uint8_t> & msg) override;
    void start() override;
    void stop() override;
    bool isReady() const override;

    void activate(const shape::Properties *props = 0);
    void deactivate();
    void modify(const shape::Properties *props);

    void attachInterface(shape::ITraceService* iface);
    void detachInterface(shape::ITraceService* iface);

  private:
    class Imp;
    Imp *m_imp = nullptr;
  };
}
