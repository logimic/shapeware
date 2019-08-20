#pragma once

#include "IMessageService.h"
#include "ShapeProperties.h"
#include <string>

namespace shape {
  class IMqttService
  {
  public:
    typedef std::function<void(const std::string& topic, const std::vector<uint8_t> & msg)> MqttMessageHandlerFunc;
    typedef std::function<void(const std::string& topic, const std::string & msg)> MqttMessageStrHandlerFunc;
    typedef std::function<void()> MqttOnConnectHandlerFunc;
    typedef std::function<void(const std::string& topic, bool result)> MqttOnSubscribeHandlerFunc;
    typedef std::function<void()> MqttOnDisconnectHandlerFunc;

    virtual ~IMqttService() {};

    virtual void create(const std::string& clientId) = 0;
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isReady() const = 0;
    virtual void subscribe(const std::string& topic) = 0;
    virtual void publish(const std::string& topic, const std::vector<uint8_t> & msg) = 0;
    virtual void publish(const std::string& topic, const std::string & msg) = 0;

    virtual void registerMessageHandler(MqttMessageHandlerFunc hndl) = 0;
    virtual void unregisterMessageHandler() = 0;
    virtual void registerMessageStrHandler(MqttMessageStrHandlerFunc hndl) = 0;
    virtual void unregisterMessageStrHandler() = 0;
    virtual void registerOnConnectHandler(MqttOnConnectHandlerFunc hndl) = 0;
    virtual void unregisterOnConnectHandler() = 0;
    virtual void registerOnSubscribeHandler(MqttOnSubscribeHandlerFunc hndl) = 0;
    virtual void unregisterOnSubscribeHandler() = 0;
    virtual void registerOnDisconnectHandler(MqttOnDisconnectHandlerFunc hndl) = 0;
    virtual void unregisterOnDisconnectHandler() = 0;
  };
}
