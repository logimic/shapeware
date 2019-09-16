/**
 * Copyright 2019 Logimic,s.r.o.
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

#pragma once

#include "IMqttService.h"
#include "ShapeProperties.h"
#include "ILaunchService.h"
#include "IBufferService.h"
#include "ITraceService.h"
#include <string>

namespace shape {
  class MqttService : public IMqttService
  {
  public:
    MqttService();
    virtual ~MqttService();
    
    void create(const std::string& clientId) override;
    void connect() override;
    void disconnect() override;
    bool isReady() const override;
    void subscribe(const std::string& topic) override;
    void publish(const std::string& topic, const std::vector<uint8_t> & msg) override;
    void publish(const std::string& topic, const std::string & msg) override;

    void registerMessageHandler(MqttMessageHandlerFunc hndl) override;
    void unregisterMessageHandler() override;
    void registerMessageStrHandler(MqttMessageStrHandlerFunc hndl) override;
    void unregisterMessageStrHandler() override;
    void registerOnConnectHandler(MqttOnConnectHandlerFunc hndl) override;
    void unregisterOnConnectHandler() override;
    void registerOnSubscribeHandler(MqttOnSubscribeHandlerFunc hndl) override;
    void unregisterOnSubscribeHandler() override;
    void registerOnDisconnectHandler(MqttOnDisconnectHandlerFunc hndl) override;
    void unregisterOnDisconnectHandler() override;

    //////////////
    void activate(const shape::Properties *props = 0);
    void deactivate();
    void modify(const shape::Properties *props);

    void attachInterface(shape::IBufferService* iface);
    void detachInterface(shape::IBufferService* iface);

    void attachInterface(shape::ILaunchService* iface);
    void detachInterface(shape::ILaunchService* iface);

    void attachInterface(shape::ITraceService* iface);
    void detachInterface(shape::ITraceService* iface);

  private:
    class Imp;
    Imp* m_impl;
  };
}