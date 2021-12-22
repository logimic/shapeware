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

#define IMessageService_EXPORTS

#include "MqttService.h"
#include "mqtt_utils.h"
#include "TaskQueue2.h"
#include "MQTTAsync.h"

#include "JsonMacro.h"
#include "rapidjson/pointer.h"

#include <set>
#include <atomic>
#include <future>

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

#include "Trace.h"

#include "shape__MqttService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::MqttService);

namespace shape {

  typedef std::basic_string<uint8_t> ustring;

  class MqttService::Imp
  {

  private:
    shape::ILaunchService* m_iLaunchService = nullptr;

    //configuration
    std::string m_mqttBrokerAddr;
    std::string m_mqttClientId;
    int m_mqttPersistence = 0;
    std::string m_mqttUser;
    std::string m_mqttPassword;
    bool m_mqttEnabledSSL = false;
    int m_mqttKeepAliveInterval = 20; //special msg sent to keep connection alive
    int m_mqttConnectTimeout = 5; //waits for accept from broker side
    int m_mqttMinReconnect = 1; //waits to reconnect when connection broken
    int m_mqttMaxReconnect = 64; //waits time *= 2 with every unsuccessful attempt up to this value
    int m_seconds = m_mqttMinReconnect;
    bool m_buffered = false; //Whether to allow messages to be sent when the client library is not connected
    int m_bufferSize = 1024; //The maximum number of messages allowed to be buffered while not connected

    //The file in PEM format containing the public digital certificates trusted by the client.
    std::string m_trustStore;
    //The file in PEM format containing the public certificate chain of the client. It may also include
    //the client's private key.
    std::string m_keyStore;
    //If not included in the sslKeyStore, this setting points to the file in PEM format containing
    //the client's private key.
    std::string m_privateKey;
    //The password to load the client's privateKey if encrypted.
    std::string m_privateKeyPassword;
    //The list of cipher suites that the client will present to the server during the SSL handshake.For a
    //full explanation of the cipher list format, please see the OpenSSL on - line documentation :
    //http ://www.openssl.org/docs/apps/ciphers.html#CIPHER_LIST_FORMAT
    std::string m_enabledCipherSuites;
    //True/False option to enable verification of the server certificate
    bool m_enableServerCertAuth = true;

    class SubscribeContext
    {
    public:
      SubscribeContext() = default;
      SubscribeContext(const std::string & topic, int qos, MqttOnSubscribeQosHandlerFunc onSubscribeHndl)
        :m_topic(topic)
        , m_qos(qos)
        , m_onSubscribeHndl(onSubscribeHndl)
      {}

      void onSubscribe(int qos, bool result)
      {
        m_onSubscribeHndl(m_topic, qos, result);
      }

    private:
      std::string m_topic;
      int m_qos;
      MqttOnSubscribeQosHandlerFunc m_onSubscribeHndl;
    };

    class UnsubscribeContext
    {
    public:
      UnsubscribeContext() = default;
      UnsubscribeContext(const std::string & topic, MqttOnUnsubscribeHandlerFunc onUnsubscribeHndl)
        :m_topic(topic)
        , m_onUnsubscribeHndl(onUnsubscribeHndl)
      {}

      void onUnsubscribe(bool result)
      {
        m_onUnsubscribeHndl(m_topic, result);
      }

    private:
      std::string m_topic;
      int m_qos;
      MqttOnUnsubscribeHandlerFunc m_onUnsubscribeHndl;
    };

    class PublishContext
    {
    public:
      PublishContext() = default;
      PublishContext(const std::string & topic, int qos, std::vector<uint8_t> msg
        , MqttOnSendHandlerFunc onSend, MqttOnDeliveryHandlerFunc onDelivery)
        :m_topic(topic)
        , m_qos(qos)
        , m_msg(msg)
        , m_onSendHndl(onSend)
        , m_onDeliveryHndl(onDelivery)
      {}

      const std::string & getTopic() const { return m_topic; }
      int getQos() const { return m_qos; }
      const std::vector<uint8_t> & getMsg() const { return m_msg; }

      void onSend(int qos, bool result, int token) const { m_onSendHndl(m_topic, qos, result); }

      void onDelivery(int qos, bool result, int token) const { m_onDeliveryHndl(m_topic, qos, result); }

    private:
      std::string m_topic;
      int m_qos;
      std::vector<uint8_t> m_msg;
      MqttOnSendHandlerFunc m_onSendHndl;
      MqttOnDeliveryHandlerFunc m_onDeliveryHndl;
    };

    //TaskQueue<PublishContext> * m_messageQueue = nullptr;
    MqttMessageHandlerFunc m_mqttMessageHandlerFunc;
    MqttMessageStrHandlerFunc m_mqttMessageStrHandlerFunc;
    MqttOnConnectHandlerFunc m_mqttOnConnectHandlerFunc;
    MqttOnSubscribeHandlerFunc m_mqttOnSubscribeHandlerFunc;
    MqttOnDisconnectHandlerFunc m_mqttOnDisconnectHandlerFunc;

    // map of [token, subscribeContext] used to invoke onSubscribe according token in asyc result
    std::map<MQTTAsync_token, SubscribeContext> m_subscribeContextMap;

    // map of [token, subscribeContext] used to invoke onSubscribe according token in asyc result
    std::map<MQTTAsync_token, UnsubscribeContext> m_unsubscribeContextMap;

    // map of [token, publishContext] used to invoke onDelivery according token in asyc result
    std::map<MQTTAsync_token, PublishContext> m_publishContextMap;

    // map of [topic, handler] used to invoke onMessage according topic
    std::map<std::string, MqttMessageStrHandlerFunc> m_onMessageHndlMap;

    MQTTAsync m_client = nullptr;

    std::thread m_connectThread;
    bool m_runConnectThread = true;

    std::mutex m_connectionMutex;
    std::condition_variable m_connectionVariable;

    std::unique_ptr<std::promise<bool>> m_disconnect_promise_uptr;

  public:
    //------------------------
    Imp()
    //  : m_messageQueue(nullptr)
    {}

    //------------------------
    ~Imp()
    {}

    //------------------------

    /////////////////////////
    // interface implementation functions
    /////////////////////////

    //------------------------
    void create(const std::string& clientId, const ConnectionPars& cp = ConnectionPars())
    {
      TRC_FUNCTION_ENTER(PAR(this) << PAR(clientId));

      if (nullptr != m_client) {
        THROW_EXC_TRC_WAR(std::logic_error, PAR(clientId) << " already created. Was IMqttService::create(clientId) called earlier?" );
      }

      // init connection options
      MQTTAsync_createOptions create_opts = MQTTAsync_createOptions_initializer;
      
      create_opts.sendWhileDisconnected = m_buffered ? 1 : 0;
      create_opts.maxBufferedMessages = m_bufferSize;
      create_opts.deleteOldestMessages = 1;

      if (!cp.brokerAddress.empty()) m_mqttBrokerAddr = cp.brokerAddress;
      if (!cp.trustStore.empty()) m_trustStore = cp.trustStore;
      if (!cp.certificate.empty()) m_keyStore = cp.certificate;
      if (!cp.privateKey.empty()) m_privateKey = cp.privateKey;

      m_mqttClientId = clientId;

      int retval;
      if ((retval = MQTTAsync_createWithOptions(&m_client, m_mqttBrokerAddr.c_str(),
        m_mqttClientId.c_str(), m_mqttPersistence, NULL, &create_opts)) != MQTTASYNC_SUCCESS) {
        THROW_EXC_TRC_WAR(std::logic_error, "MQTTClient_create() failed: " << PAR(retval));
      }

      // init event callbacks
      if ((retval = MQTTAsync_setCallbacks(m_client, this, s_connlost, s_msgarrvd, s_delivered)) != MQTTASYNC_SUCCESS) {
        THROW_EXC_TRC_WAR(std::logic_error, "MQTTClient_setCallbacks() failed: " << PAR(retval));
      }

      TRC_FUNCTION_LEAVE(PAR(this));
    }

    void destroy(const std::string& clientId)
    {
      TRC_FUNCTION_ENTER(PAR(this) << PAR(clientId));

      disconnect();

      MQTTAsync_setCallbacks(m_client, nullptr, nullptr, nullptr, nullptr);
      MQTTAsync_destroy(&m_client);

      TRC_INFORMATION(PAR(this) << PAR(clientId) << "destroyed");

      TRC_FUNCTION_LEAVE(PAR(this));
    }

    //------------------------
    void connect()
    {
      TRC_FUNCTION_ENTER(PAR(this));

      if (nullptr == m_client) {
        THROW_EXC_TRC_WAR(std::logic_error, " Client is not created. Consider calling IMqttService::create(clientId)");
      }

      m_runConnectThread = true;
      m_connectionVariable.notify_all();

      if (m_connectThread.joinable())
        m_connectThread.join();

      m_connectThread = std::thread([this]() { this->connectThread(); });
      TRC_FUNCTION_LEAVE(PAR(this));
    }

    void connect(MqttOnConnectHandlerFunc onConnect)
    {
      m_mqttOnConnectHandlerFunc = onConnect;
      connect();
    }

    //------------------------
    void disconnect()
    {
      TRC_FUNCTION_ENTER(PAR(this));

      if (nullptr == m_client) {
        TRC_WARNING(PAR(this) << " Client was not created at all");
      }

      m_disconnect_promise_uptr.reset(shape_new std::promise<bool>());
      std::future<bool> disconnect_future = m_disconnect_promise_uptr->get_future();

      ///stop connect thread
      m_runConnectThread = false;
      m_connectionVariable.notify_all();

      onConnectFailure(nullptr);
      if (m_connectThread.joinable())
        m_connectThread.join();

      TRC_WARNING(PAR(this) << PAR(m_mqttClientId) << " Disconnect: => Message queue will be stopped ");
      //m_messageQueue->stopQueue();

      // init disconnect options
      MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
      disc_opts.onSuccess = s_onDisconnect;
      disc_opts.onFailure = s_onDisconnectFailure;
      disc_opts.context = this;

      int retval;
      if ((retval = MQTTAsync_disconnect(m_client, &disc_opts)) != MQTTASYNC_SUCCESS) {
        TRC_WARNING(PAR(this) << " Failed to start disconnect: " << PAR(retval));
      }

      std::chrono::milliseconds span(5000);
      if (disconnect_future.wait_for(span) == std::future_status::timeout) {
        TRC_WARNING(PAR(this) << " Timeout to wait disconnect");
      }

      TRC_INFORMATION(PAR(this) << " MQTT disconnected");

      TRC_FUNCTION_LEAVE(PAR(this));
    }

    void disconnect(MqttOnDisconnectHandlerFunc onDisconnect)
    {
      m_mqttOnDisconnectHandlerFunc = onDisconnect;
      disconnect();
    }

    bool isReady() const
    {
      if (nullptr == m_client) {
        TRC_WARNING(PAR(this) << " Client was not created at all");
        return false;
      }
      return MQTTAsync_isConnected(m_client);
    }

    void registerMessageHandler(MqttMessageHandlerFunc hndl)
    {
      TRC_FUNCTION_ENTER(PAR(this));
      m_mqttMessageHandlerFunc = hndl;
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void unregisterMessageHandler()
    {
      TRC_FUNCTION_ENTER(PAR(this));
      m_mqttMessageHandlerFunc = nullptr;
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void registerMessageStrHandler(MqttMessageStrHandlerFunc hndl)
    {
      TRC_FUNCTION_ENTER(PAR(this));
      m_mqttMessageStrHandlerFunc = hndl;
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void unregisterMessageStrHandler()
    {
      TRC_FUNCTION_ENTER(PAR(this));
      m_mqttMessageStrHandlerFunc = nullptr;
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void registerOnConnectHandler(MqttOnConnectHandlerFunc hndl)
    {
      TRC_FUNCTION_ENTER(PAR(this));
      m_mqttOnConnectHandlerFunc = hndl;
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void unregisterOnConnectHandler()
    {
      TRC_FUNCTION_ENTER(PAR(this));
      m_mqttOnConnectHandlerFunc = nullptr;
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void registerOnSubscribeHandler(MqttOnSubscribeHandlerFunc hndl)
    {
      TRC_FUNCTION_ENTER(PAR(this));
      m_mqttOnSubscribeHandlerFunc = hndl;
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void unregisterOnSubscribeHandler()
    {
      TRC_FUNCTION_ENTER(PAR(this));
      m_mqttOnSubscribeHandlerFunc = nullptr;
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void registerOnDisconnectHandler(MqttOnDisconnectHandlerFunc hndl)
    {
      TRC_FUNCTION_ENTER(PAR(this));
      m_mqttOnDisconnectHandlerFunc = hndl;
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void unregisterOnDisconnectHandler()
    {
      TRC_FUNCTION_ENTER(PAR(this));
      m_mqttOnDisconnectHandlerFunc = nullptr;
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    //TODO obsolete subscribe() version
    void subscribe(const std::string& topic, int qos)
    {
      TRC_FUNCTION_ENTER(PAR(this) << PAR(topic));

      if (nullptr == m_client) {
        THROW_EXC_TRC_WAR(std::logic_error, " Client is not created. Consider calling IMqttService::create(clientId)");
      }

      auto onSubscribe = [&](const std::string& topic, int qos, bool result)
      {
        TRC_INFORMATION(PAR(this) << " Subscribed result: " << PAR(topic) << PAR(result))
        if (m_mqttOnSubscribeHandlerFunc) {
          m_mqttOnSubscribeHandlerFunc(topic, true);
        }
      };

      auto onMessage = [&](const std::string& topic, const std::string & message)
      {
        TRC_DEBUG(PAR(this) << " ==================================" << std::endl <<
          "Received from MQTT: " << std::endl << MEM_HEX_CHAR(message.data(), message.size()));

        if (m_mqttMessageHandlerFunc) {
          m_mqttMessageHandlerFunc(topic, std::vector<uint8_t>(message.data(), message.data() + message.size()));
        }
        if (m_mqttMessageStrHandlerFunc) {
          m_mqttMessageStrHandlerFunc(topic, std::string((char*)message.data(), message.size()));
        }
      };

      subscribe(topic, qos, onSubscribe, onMessage);
      
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void subscribe(const std::string& topic, int qos, MqttOnSubscribeQosHandlerFunc onSubscribe, MqttMessageStrHandlerFunc onMessage)
    {
      TRC_FUNCTION_ENTER(PAR(this) << PAR(topic));

      if (nullptr == m_client) {
        THROW_EXC_TRC_WAR(std::logic_error, " Client is not created. Consider calling IMqttService::create(clientId)");
      }

      TRC_DEBUG(PAR(this) << "LCK-hndlMutex");
      std::lock_guard<std::mutex> lck(m_connectionMutex); //protects handlers maps
      TRC_DEBUG(PAR(this) << "AQR-hndlMutex");

      MQTTAsync_responseOptions subs_opts = MQTTAsync_responseOptions_initializer;

      // init subscription options
      subs_opts.onSuccess = s_onSubscribe;
      subs_opts.onFailure = s_onSubscribeFailure;
      subs_opts.context = this;

      int retval;
      if ((retval = MQTTAsync_subscribe(m_client, topic.c_str(), qos, &subs_opts)) != MQTTASYNC_SUCCESS) {
        THROW_EXC_TRC_WAR(std::logic_error, "MQTTAsync_subscribe() failed: " << PAR(retval) << PAR(topic) << PAR(qos));
      }

      TRC_DEBUG(PAR(this) << PAR(subs_opts.token))
      m_subscribeContextMap[subs_opts.token] = SubscribeContext(topic, qos, onSubscribe);
      m_onMessageHndlMap[topic] = onMessage;

      TRC_DEBUG(PAR(this) << "ULCK-hndlMutex");
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void unsubscribe(const std::string& topic, MqttOnUnsubscribeHandlerFunc onUnsubscribe)
    {
      TRC_FUNCTION_ENTER(PAR(this) << PAR(topic));

      if (nullptr == m_client) {
        THROW_EXC_TRC_WAR(std::logic_error, " Client is not created. Consider calling IMqttService::create(clientId)");
      }

      TRC_DEBUG(PAR(this) << "LCK-hndlMutex");
      std::lock_guard<std::mutex> lck(m_connectionMutex); //protects handlers maps
      TRC_DEBUG(PAR(this) << "AQR-hndlMutex");

      m_onMessageHndlMap.erase(topic);

      MQTTAsync_responseOptions subs_opts = MQTTAsync_responseOptions_initializer;

      // init subscription options
      subs_opts.onSuccess = s_onUnsubscribe;
      subs_opts.onFailure = s_onUnsubscribeFailure;
      subs_opts.context = this;

      int retval;
      if ((retval = MQTTAsync_unsubscribe(m_client, topic.c_str(), &subs_opts)) != MQTTASYNC_SUCCESS) {
        THROW_EXC_TRC_WAR(std::logic_error, "MQTTAsync_unsubscribe() failed: " << PAR(retval) << PAR(topic));
      }

      TRC_DEBUG(PAR(this) << PAR(subs_opts.token))
        m_unsubscribeContextMap[subs_opts.token] = UnsubscribeContext(topic, onUnsubscribe);

      TRC_DEBUG(PAR(this) << "ULCK-hndlMutex");
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void publish(const std::string& topic, int qos, const std::vector<uint8_t> & msg)
    {
      auto onSend = [&](const std::string& topic, int qos, bool result)
      {
        TRC_DEBUG(PAR(this) << " onSend: " << PAR(topic) << PAR(qos) << PAR(result));
      };

      auto onDelivery = [&](const std::string& topic, int qos, bool result)
      {
        TRC_DEBUG(PAR(this) << " onDelivery: " << PAR(topic) << PAR(qos) << PAR(result));
      };
      
      publish(topic, qos, msg, onSend, onDelivery);
    }

    void publish(const std::string& topic, int qos, const std::string & msg)
    {
      publish(topic, qos, std::vector<uint8_t>(msg.data(), msg.data() + msg.size()));
    }
 
#if 0
    void publish0(const std::string& topic, int qos, const std::vector<uint8_t> & msg
      , MqttOnSendHandlerFunc onSend, MqttOnDeliveryHandlerFunc onDelivery)
    {
      TRC_FUNCTION_ENTER(PAR(this));

      TRC_INFORMATION(PAR(this) << PAR(topic) << PAR(qos));

      if (nullptr == m_client) {
        THROW_EXC_TRC_WAR(std::logic_error, " Client is not created. Consider calling IMqttService::create(clientId)" << PAR(topic));
      }

      int retval = m_messageQueue->pushToQueue(PublishContext(topic, qos, msg, onSend, onDelivery));
      if (retval > m_bufferSize && m_buffered) {
        auto task = m_messageQueue->pop();
        TRC_WARNING(PAR(this) << " Buffer overload => remove the oldest msg: " << std::endl <<
          NAME_PAR(topic, task.getTopic()) << std::endl <<
          std::string((char*)task.getMsg().data(), task.getMsg().size()));
      }
      TRC_FUNCTION_LEAVE(PAR(this));
    }
#endif  

    void publish(const std::string& topic, int qos, const std::string & msg
      , MqttOnSendHandlerFunc onSend, MqttOnDeliveryHandlerFunc onDelivery)
    {
      publish(topic, qos, std::vector<uint8_t>(msg.data(), msg.data() + msg.size()), onSend, onDelivery);
    }

    ///////////////////////
    // connection functions
    ///////////////////////

    void connectThread()
    {
      TRC_FUNCTION_ENTER(PAR(this));
      //TODO verify paho autoconnect and reuse if applicable - does not work now
      int retval;
      static int wait_cnt = 0;

      while (m_runConnectThread) {
        if (!MQTTAsync_isConnected(m_client)) {
          // init connection options
          MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
          MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;

          conn_opts.keepAliveInterval = m_mqttKeepAliveInterval;
          conn_opts.cleansession = 1;
          conn_opts.connectTimeout = m_mqttConnectTimeout;
          conn_opts.username = m_mqttUser.c_str();
          conn_opts.password = m_mqttPassword.c_str();
          conn_opts.onSuccess = s_onConnect;
          conn_opts.onFailure = s_onConnectFailure;
          conn_opts.context = this;
          conn_opts.automaticReconnect = 0; //1 doesn't work with aws

          // init ssl options if required
          if (m_mqttEnabledSSL) {
            ssl_opts.enableServerCertAuth = true;
            if (!m_trustStore.empty()) ssl_opts.trustStore = m_trustStore.c_str();
            if (!m_keyStore.empty()) ssl_opts.keyStore = m_keyStore.c_str();
            if (!m_privateKey.empty()) ssl_opts.privateKey = m_privateKey.c_str();
            if (!m_privateKeyPassword.empty()) ssl_opts.privateKeyPassword = m_privateKeyPassword.c_str();
            if (!m_enabledCipherSuites.empty()) ssl_opts.enabledCipherSuites = m_enabledCipherSuites.c_str();
            ssl_opts.enableServerCertAuth = m_enableServerCertAuth;
            conn_opts.ssl = &ssl_opts;
          }

          TRC_DEBUG(PAR(this) << " Connecting: " << PAR(m_mqttClientId) << PAR(m_mqttBrokerAddr)
            << NAME_PAR(trustStore, (ssl_opts.trustStore ? ssl_opts.trustStore : ""))
            << NAME_PAR(keyStore, (ssl_opts.keyStore ? ssl_opts.keyStore : ""))
            << NAME_PAR(privateKey, (ssl_opts.privateKey ? ssl_opts.privateKey : ""))
            << NAME_PAR(enableServerCertAuth, ssl_opts.enableServerCertAuth)
          );

          if ((retval = MQTTAsync_connect(m_client, &conn_opts)) == MQTTASYNC_SUCCESS) {
          }
          else {
            TRC_WARNING(PAR(this) << " MQTTAsync_connect() failed: " << PAR(retval));
          }

          m_seconds = m_seconds < m_mqttMaxReconnect ? m_seconds * 2 : m_mqttMaxReconnect;
          TRC_DEBUG(PAR(this) << " Going to sleep for: " << PAR(m_seconds));
        }
        else {
          m_seconds = m_mqttMaxReconnect;
        }

        // wait for connection result
        {
          TRC_DEBUG(PAR(this) << "LCK-connectionMutex");
          std::unique_lock<std::mutex> lck(m_connectionMutex);
          TRC_DEBUG(PAR(this) << "AQR-wait connectionMutex - waiting cnt: " << ++wait_cnt);
          m_connectionVariable.wait_for(lck, std::chrono::seconds(m_seconds),
            [this] {return !m_runConnectThread; });
          TRC_DEBUG(PAR(this) << "ULCK-connectionMutex: " << "out of waiting cnt: " << ++wait_cnt);
        }

      }
      TRC_FUNCTION_LEAVE(PAR(this));
    }

    //----------------------------
    // connection succes callback
    static void s_onConnect(void* context, MQTTAsync_successData* response)
    {
      ((MqttService::Imp*)context)->onConnect(response);
    }
    void onConnect(MQTTAsync_successData* response)
    {
      TRC_FUNCTION_ENTER(PAR(this));
      MQTTAsync_token token = 0;
      char* suri = nullptr;
      std::string serverUri;
      int MQTTVersion = 0;
      int sessionPresent = 0;

      if (response) {
        token = response->token;
        suri = response->alt.connect.serverURI;
        serverUri = suri ? suri : "";
        MQTTVersion = response->alt.connect.MQTTVersion;
        sessionPresent = response->alt.connect.sessionPresent;
      }

      TRC_INFORMATION(PAR(this) << " Connect succeded: " <<
        PAR(m_mqttBrokerAddr) <<
        PAR(m_mqttClientId) <<
        PAR(token) <<
        PAR(serverUri) <<
        PAR(MQTTVersion) <<
        PAR(sessionPresent)
      );

      m_connectionVariable.notify_all();

      if (m_mqttOnConnectHandlerFunc) {
        m_mqttOnConnectHandlerFunc();
      }

      //TRC_WARNING(PAR(this) << "\n Message queue => going to send buffered msgs number: " << NAME_PAR(bufferSize, m_messageQueue->size()));

      TRC_FUNCTION_LEAVE(PAR(this));
    }

    //----------------------------
    // connection failure callback
    static void s_onConnectFailure(void* context, MQTTAsync_failureData* response)
    {
      ((MqttService::Imp*)context)->onConnectFailure(response);
    }
    void onConnectFailure(MQTTAsync_failureData* response)
    {
      TRC_FUNCTION_ENTER(PAR(this));
      if (response) {
        TRC_WARNING(PAR(this) << " Connect failed: " << PAR(m_mqttClientId) << PAR(response->code) << NAME_PAR(errmsg, (response->message ? response->message : "-")));
      }
      else {
        TRC_WARNING(PAR(this) << " Connect failed: " << PAR(m_mqttClientId) << " missing more info");
      }
      {
        TRC_DEBUG(PAR(this) << "LCK-connectionMutex");
        std::unique_lock<std::mutex> lck(m_connectionMutex);
        TRC_DEBUG(PAR(this) << "AQR-connectionMutex");
        m_connectionVariable.notify_all();
        TRC_DEBUG(PAR(this) << "ULCK-connectionMutex");
      }
      TRC_FUNCTION_LEAVE(PAR(this));
    }

    ///////////////////////
    // subscribe functions
    ///////////////////////

    //------------------------
    // subscribe success
    static void s_onSubscribe(void* context, MQTTAsync_successData* response)
    {
      ((MqttService::Imp*)context)->onSubscribe(response);
    }
    void onSubscribe(MQTTAsync_successData* response)
    {
      TRC_FUNCTION_ENTER(PAR(this) << NAME_PAR(token, (response ? response->token : -1)) << NAME_PAR(qos, (response ? response->alt.qos : -1)));

      MQTTAsync_token token = 0;
      int qos = 0;

      if (response) {
        token = response->token;
        qos = response->alt.qos;
      }

      TRC_DEBUG(PAR(this) << "LCK-hndlMutex");
      std::lock_guard<std::mutex> lck(m_connectionMutex); //protects handlers maps
      TRC_DEBUG(PAR(this) << "AQR-hndlMutex");

      //based on newer subscribe() version
      auto found = m_subscribeContextMap.find(token);
      if (found != m_subscribeContextMap.end()) {
        auto & sc = found->second;
        sc.onSubscribe(qos, true);
        m_subscribeContextMap.erase(found);
      }
      else {
        TRC_WARNING(PAR(this) << " Missing onSubscribe handler: " << PAR(token));
      }

      TRC_DEBUG(PAR(this) << "LCK-hndlMutex");
      TRC_FUNCTION_LEAVE(PAR(this));
    }

    //------------------------
    // subscribe failure
    static void s_onSubscribeFailure(void* context, MQTTAsync_failureData* response) {
      ((MqttService::Imp*)context)->onSubscribeFailure(response);
    }
    void onSubscribeFailure(MQTTAsync_failureData* response)
    {
      TRC_FUNCTION_ENTER(PAR(this));

      MQTTAsync_token token = 0;
      int code = 0;
      std::string message;

      if (response) {
        token = response->token;
        code = response->code;
        message = response->message ? response->message : "";
      }

      TRC_WARNING(PAR(this) << " Subscribe failed: " <<
        PAR(token) <<
        PAR(code) <<
        PAR(message)
      );

      //based on newer subscribe() version
      auto found = m_subscribeContextMap.find(token);
      if (found != m_subscribeContextMap.end()) {
        auto & sc = found->second;
        sc.onSubscribe(0, false);
        m_subscribeContextMap.erase(found);
      }
      else {
        TRC_WARNING(PAR(this) << " Missing onSubscribe handler: " << PAR(token));
      }

      TRC_FUNCTION_LEAVE(PAR(this));
    }

    ///////////////////////
    // unsubscribe functions
    ///////////////////////

    //------------------------
    // subscribe success
    static void s_onUnsubscribe(void* context, MQTTAsync_successData* response)
    {
      ((MqttService::Imp*)context)->onUnsubscribe(response);
    }
    void onUnsubscribe(MQTTAsync_successData* response)
    {
      TRC_FUNCTION_ENTER(PAR(this) << NAME_PAR(token, (response ? response->token : -1)));

      MQTTAsync_token token = 0;

      if (response) {
        token = response->token;
      }

      TRC_DEBUG(PAR(this) << "LCK-hndlMutex");
      std::lock_guard<std::mutex> lck(m_connectionMutex); //protects handlers maps
      TRC_DEBUG(PAR(this) << "AQR-hndlMutex");

      //based on newer subscribe() version
      auto found = m_unsubscribeContextMap.find(token);
      if (found != m_unsubscribeContextMap.end()) {
        auto & sc = found->second;
        sc.onUnsubscribe(true);
        m_unsubscribeContextMap.erase(found);
      }
      else {
        TRC_WARNING(PAR(this) << " Missing onUnsubscribe handler: " << PAR(token));
      }

      TRC_DEBUG(PAR(this) << "ULCK-hndlMutex");
      TRC_FUNCTION_LEAVE(PAR(this));
    }

    //------------------------
    // subscribe failure
    static void s_onUnsubscribeFailure(void* context, MQTTAsync_failureData* response) {
      ((MqttService::Imp*)context)->onUnsubscribeFailure(response);
    }
    void onUnsubscribeFailure(MQTTAsync_failureData* response)
    {
      TRC_FUNCTION_ENTER(PAR(this));

      MQTTAsync_token token = 0;
      int code = 0;
      std::string message;

      if (response) {
        token = response->token;
        code = response->code;
        message = response->message ? response->message : "";
      }

      TRC_WARNING(PAR(this) << " Unsubscribe failed: " <<
        PAR(token) <<
        PAR(code) <<
        PAR(message)
      );

      //based on newer subscribe() version
      auto found = m_unsubscribeContextMap.find(token);
      if (found != m_unsubscribeContextMap.end()) {
        auto & sc = found->second;
        sc.onUnsubscribe(false);
        m_unsubscribeContextMap.erase(found);
      }
      else {
        TRC_WARNING(PAR(this) << " Missing onUnsubscribe handler: " << PAR(token));
      }

      TRC_FUNCTION_LEAVE(PAR(this));
    }

    ///////////////////////
    // send (publish) functions
    ///////////////////////

    void publish(const std::string& topic, int qos, const std::vector<uint8_t> & msg
      , MqttOnSendHandlerFunc onSend, MqttOnDeliveryHandlerFunc onDelivery)
    {
      TRC_FUNCTION_ENTER("Sending to MQTT: " << PAR(topic) << PAR(qos) << std::endl <<
        MEM_HEX_CHAR(msg.data(), (msg.size() > 256 ? 256 : msg.size())));

      TRC_INFORMATION(PAR(this) << PAR(topic) << PAR(qos));

      if (nullptr == m_client) {
        THROW_EXC_TRC_WAR(std::logic_error, " Client is not created. Consider calling IMqttService::create(clientId)" << PAR(topic));
      }

      bool bretval = false;
      int retval;
      MQTTAsync_message pubmsg = MQTTAsync_message_initializer;

      pubmsg.payload = (void*)msg.data();
      pubmsg.payloadlen = (int)msg.size();
      pubmsg.qos = qos;
      pubmsg.retained = 0;

      TRC_DEBUG(PAR(this) << "LCK-hndlMutex");
      std::lock_guard<std::mutex> lck(m_connectionMutex); //protects handlers maps
      TRC_DEBUG(PAR(this) << "AQR-hndlMutex");

      MQTTAsync_responseOptions send_opts = MQTTAsync_responseOptions_initializer;
      // init send options
      send_opts.onSuccess = s_onSend;
      send_opts.onFailure = s_onSendFailure;
      send_opts.context = this;
      send_opts.token = -1;

      if ((retval = MQTTAsync_sendMessage(m_client, topic.c_str(), &pubmsg, &send_opts)) == MQTTASYNC_SUCCESS) {
        bretval = true;

        PublishContext pc(topic, qos, msg, onSend, onDelivery);

        if (m_publishContextMap.size() > m_bufferSize) {
          TRC_WARNING(PAR(this) << "gets limits: " << NAME_PAR(token, send_opts.token) << NAME_PAR(topic, pc.getTopic()) << NAME_PAR(qos, pc.getQos())
            << NAME_PAR(publishContextMap.size, m_publishContextMap.size()));
        }
        else {
          TRC_DEBUG(PAR(this) << NAME_PAR(token, send_opts.token) << NAME_PAR(topic, pc.getTopic()) << NAME_PAR(qos, pc.getQos())
            << NAME_PAR(publishContextMap.size, m_publishContextMap.size()));
          m_publishContextMap[send_opts.token] = pc;
        }
      }
      else {
        TRC_WARNING(PAR(this) << " Failed to start sendMessage: " << PAR(retval));
        if (!m_buffered) {
          bretval = true; // => pop anyway from queue
        }
      }

      TRC_DEBUG(PAR(this) << "ULCK-hndlMutex");
      TRC_FUNCTION_LEAVE(PAR(this));
      //return bretval;
      return;

      //int retval = m_messageQueue->pushToQueue(PublishContext(topic, qos, msg, onSend, onDelivery));
      //if (retval > m_bufferSize && m_buffered) {
      //  auto task = m_messageQueue->pop();
      //  TRC_WARNING(PAR(this) << " Buffer overload => remove the oldest msg: " << std::endl <<
      //    NAME_PAR(topic, task.getTopic()) << std::endl <<
      //    std::string((char*)task.getMsg().data(), task.getMsg().size()));
      //}

      TRC_FUNCTION_LEAVE(PAR(this));
    }

#if 0
    // process function of message queue
    bool publishFromQueue(const PublishContext & pc)
    {
      TRC_FUNCTION_ENTER("Sending to MQTT: " << NAME_PAR(topic, pc.getTopic()) << NAME_PAR(qos, pc.getQos()) << std::endl <<
        MEM_HEX_CHAR(pc.getMsg().data(), pc.getMsg().size()));

      bool bretval = false;
      int retval;
      MQTTAsync_message pubmsg = MQTTAsync_message_initializer;

      pubmsg.payload = (void*)pc.getMsg().data();
      pubmsg.payloadlen = (int)pc.getMsg().size();
      pubmsg.qos = pc.getQos();
      pubmsg.retained = 0;

      TRC_DEBUG(PAR(this) << "LCK-hndlMutex");
      std::lock_guard<std::mutex> lck(m_connectionMutex); //protects handlers maps
      TRC_DEBUG(PAR(this) << "AQR-hndlMutex");

      MQTTAsync_responseOptions send_opts = MQTTAsync_responseOptions_initializer;
      // init send options
      send_opts.onSuccess = s_onSend;
      send_opts.onFailure = s_onSendFailure;
      send_opts.context = this;
      send_opts.token = -1;

      if ((retval = MQTTAsync_sendMessage(m_client, pc.getTopic().c_str(), &pubmsg, &send_opts)) == MQTTASYNC_SUCCESS) {
        bretval = true;
      
        TRC_INFORMATION(PAR(this) << NAME_PAR(token, send_opts.token) << NAME_PAR(topic, pc.getTopic()) << NAME_PAR(qos, pc.getQos())
          << NAME_PAR(publishContextMap.size, m_publishContextMap.size()));
        m_publishContextMap[send_opts.token] = pc;
      }
      else {
        TRC_WARNING(PAR(this) << " Failed to start sendMessage: " << PAR(retval));
        if (!m_buffered) {
          bretval = true; // => pop anyway from queue
        }
      }

      TRC_DEBUG(PAR(this) << "ULCK-hndlMutex");
      TRC_FUNCTION_LEAVE(PAR(this));
      return bretval;
    }
#endif

    //------------------------
    // send success
    static void s_onSend(void* context, MQTTAsync_successData* response)
    {
      ((MqttService::Imp*)context)->onSend(response);
    }
    void onSend(MQTTAsync_successData* response)
    {
      TRC_DEBUG(PAR(this) << " Message sent successfuly: " << NAME_PAR(token, (response ? response->token : 0)));
      
      if (response) {
        TRC_DEBUG(PAR(this) << "LCK-hndlMutex");
        std::lock_guard<std::mutex> lck(m_connectionMutex); //protects handlers maps
        TRC_DEBUG(PAR(this) << "AQR-hndlMutex");

        auto found = m_publishContextMap.find(response->token);
        if (found != m_publishContextMap.end()) {
          auto & pc = found->second;
          TRC_INFORMATION(PAR(this) << NAME_PAR(token, response->token) << NAME_PAR(topic, pc.getTopic()) << NAME_PAR(qos, pc.getQos()));
          pc.onSend(pc.getQos(), true, response->token);
          //if (pc.getQos() == 0) {
            m_publishContextMap.erase(found);
          //}
        }
        else {
          TRC_WARNING(PAR(this) << " Missing publishContext: " << PAR(response->token));
        }
        TRC_DEBUG(PAR(this) << "ULCK-hndlMutex");
      }
    }

    //------------------------
    // send failure
    static void s_onSendFailure(void* context, MQTTAsync_failureData* response)
    {
      ((MqttService::Imp*)context)->onSendFailure(response);
    }
    void onSendFailure(MQTTAsync_failureData* response)
    {
      TRC_FUNCTION_ENTER(PAR(this));

      MQTTAsync_token token = 0;
      int code = 0;
      std::string message;

      if (response) {
        token = response->token;
        code = response->code;
        message = response->message ? response->message : "";
      }

      TRC_WARNING(PAR(this) << " Send failed: " <<
        PAR(token) <<
        PAR(code) <<
        PAR(message)
      );

      auto found = m_publishContextMap.find(token);
      if (found != m_publishContextMap.end()) {
        auto & pc = found->second;
        TRC_WARNING(PAR(this) << PAR(token) << NAME_PAR(topic, pc.getTopic()) << NAME_PAR(qos, pc.getQos()));
        pc.onSend(pc.getQos(), false, token);
        m_publishContextMap.erase(found);
      }
      else {
        TRC_WARNING(PAR(this) << " Missing publishContext: " << PAR(token));
      }

      TRC_FUNCTION_LEAVE(PAR(this));
      
      
      TRC_WARNING(PAR(this) << " Message sent failure: " << PAR(response->code));
    }

    ///////////////////////
    // disconnect functions
    ///////////////////////
    
    //------------------------
    // disconnect success
    static void s_onDisconnect(void* context, MQTTAsync_successData* response)
    {
      ((MqttService::Imp*)context)->onDisconnect(response);
    }
    void onDisconnect(MQTTAsync_successData* response)
    {
      TRC_FUNCTION_ENTER(PAR(this) << NAME_PAR(token, (response ? response->token : 0)));
      m_disconnect_promise_uptr->set_value(true);

      if (m_mqttOnDisconnectHandlerFunc) {
        m_mqttOnDisconnectHandlerFunc();
      }
      TRC_FUNCTION_LEAVE(PAR(this));
    }

    //------------------------
    // disconnect failure
    static void s_onDisconnectFailure(void* context, MQTTAsync_failureData* response) {
      ((MqttService::Imp*)context)->onDisconnectFailure(response);
    }
    void onDisconnectFailure(MQTTAsync_failureData* response) {
      TRC_FUNCTION_ENTER(PAR(this) << NAME_PAR(token, (response ? response->token : 0)));
      m_disconnect_promise_uptr->set_value(false);

      TRC_FUNCTION_LEAVE(PAR(this));
    }

    /////////////////////
    // event callback functions
    /////////////////////

    //------------------------
    // delivery confirmation  of (publish) message
    static void s_delivered(void *context, MQTTAsync_token token)
    {
      ((MqttService::Imp*)context)->delivered(token);
    }
    void delivered(MQTTAsync_token token)
    {
      TRC_FUNCTION_ENTER("Message delivery confirmed: " << PAR(token));

      auto found = m_publishContextMap.find(token);
      if (found != m_publishContextMap.end()) {
        auto & pc = found->second;
        TRC_INFORMATION(PAR(this) << PAR(token) << NAME_PAR(topic, pc.getTopic()) << NAME_PAR(qos, pc.getQos()));
        pc.onDelivery(pc.getQos(), true, token);
      }
      else {
        TRC_WARNING(PAR(this) << " Missing publishContext: " << PAR(token));
      }

      TRC_FUNCTION_LEAVE(PAR(this));
    }

    //------------------------
    // receive (subscribe topic) message
    static int s_msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
    {
      return ((MqttService::Imp*)context)->msgarrvd(topicName, topicLen, message);
    }
    int msgarrvd(char *topicName, int topicLen, MQTTAsync_message *message)
    {
      TRC_FUNCTION_ENTER(PAR(this));
      ustring msg((unsigned char*)message->payload, message->payloadlen);
      std::string topic;
      if (topicLen > 0)
        topic = std::string(topicName, topicLen);
      else
        topic = std::string(topicName);

      MQTTAsync_freeMessage(&message);
      MQTTAsync_free(topicName);
      
      TRC_DEBUG(PAR(this) << PAR(topic));
      bool handled = false;

      for (auto it : m_onMessageHndlMap) {
        
        const std::string & topic2 = it.first;
        
        if (topic2 == topic) {
          it.second(topic, std::string((char*)msg.data(), msg.size()));
          handled = true;
        }

        //handle # wildcard
        size_t sz = topic2.size();
        if ('#' == topic2[--sz] && 0 == topic2.compare(0, sz, topic, 0, sz)) {
          it.second(topic, std::string((char*)msg.data(), msg.size()));
          handled = true;
        }

        //handle + wildcard
        if (topic2.find('+') != std::string::npos) {
          auto vect1 = tokenizeTopic(topic);
          auto vect2 = tokenizeTopic(topic2);
          bool match = true;

          if (vect1.size() == vect2.size()) {
            auto it1 = vect1.begin();
            for (auto it2 = vect2.begin(); it2 != vect2.end(); it2++) {
              if (*it2 == "+") {
                ++it1;
                continue;
              }
              if (*it2 != *it1) {
                match = false;
                break;
              }
              ++it1;
            }

            if (match) {
              it.second(topic, std::string((char*)msg.data(), msg.size()));
              handled = true;
            }
          }
        }
      }

      if (!handled) {
        TRC_WARNING(PAR(this) << " no handler for: " << PAR(topic))
      }

      TRC_FUNCTION_LEAVE(PAR(this));
      return 1;
    }

    //------------------------
    // connection lost
    static void s_connlost(void *context, char *cause)
    {
      ((MqttService::Imp*)context)->connlost(cause);
    }
    void connlost(char *cause) {
      TRC_FUNCTION_ENTER(PAR(this));
      TRC_WARNING(PAR(this) << " Connection lost: " << NAME_PAR(cause, (cause ? cause : "nullptr")) << " wait for automatic reconnect");
      m_seconds = m_mqttMinReconnect;
      m_connectionVariable.notify_all();
      TRC_FUNCTION_LEAVE(PAR(this));
    }

    /////////////////////
    // component functions
    /////////////////////

    void activate(const shape::Properties *props)
    {
      TRC_FUNCTION_ENTER(PAR(this));
      TRC_INFORMATION(PAR(this) << std::endl <<
        "******************************" << std::endl <<
        "MqttService instance activate" << std::endl <<
        "******************************"
      );

      modify(props);

      //m_messageQueue = shape_new TaskQueue<PublishContext>([&](PublishContext pc)->bool {
      //  return publishFromQueue(pc);
      //});

      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER(PAR(this));
      TRC_INFORMATION(PAR(this) << std::endl <<
        "******************************" << std::endl <<
        "MqttService instance deactivate" << std::endl <<
        "******************************"
      );

      disconnect();

      MQTTAsync_setCallbacks(m_client, nullptr, nullptr, nullptr, nullptr);
      MQTTAsync_destroy(&m_client);

      //delete m_messageQueue;

      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void modify(const shape::Properties *props)
    {
      TRC_FUNCTION_ENTER(PAR(this));

      props->getMemberAsString("BrokerAddr", m_mqttBrokerAddr);
      props->getMemberAsInt("Persistence", m_mqttPersistence);
      props->getMemberAsString("User", m_mqttUser);
      props->getMemberAsString("Password", m_mqttPassword);
      props->getMemberAsBool("EnabledSSL", m_mqttEnabledSSL);

      props->getMemberAsString("TrustStore", m_trustStore);
      props->getMemberAsString("KeyStore", m_keyStore);
      props->getMemberAsString("PrivateKey", m_privateKey);
      props->getMemberAsString("PrivateKeyPassword", m_privateKeyPassword);
      props->getMemberAsString("EnabledCipherSuites", m_enabledCipherSuites);
      props->getMemberAsBool("EnableServerCertAuth", m_enableServerCertAuth);

      props->getMemberAsInt("KeepAliveInterval", m_mqttKeepAliveInterval);
      props->getMemberAsInt("ConnectTimeout", m_mqttConnectTimeout);
      props->getMemberAsInt("MinReconnect", m_mqttMinReconnect);
      props->getMemberAsInt("MaxReconnect", m_mqttMaxReconnect);

      props->getMemberAsBool("Buffered", m_buffered);
      props->getMemberAsInt("BufferSize", m_bufferSize);

      std::string dataDir = m_iLaunchService->getDataDir();
      m_trustStore = m_trustStore.empty() ? "" : dataDir + "/cert/" + m_trustStore;
      m_keyStore = m_keyStore.empty() ? "" : dataDir + "/cert/" + m_keyStore;
      m_privateKey = m_privateKey.empty() ? "" : dataDir + "/cert/" + m_privateKey;

      TRC_FUNCTION_LEAVE(PAR(this));
    }

    void attachInterface(shape::ILaunchService* iface)
    {
      TRC_FUNCTION_ENTER(PAR(this));
      m_iLaunchService = iface;
      TRC_FUNCTION_LEAVE(PAR(this))
    }

    void detachInterface(shape::ILaunchService* iface)
    {
      TRC_FUNCTION_ENTER(PAR(this));
      if (m_iLaunchService == iface) {
        m_iLaunchService = nullptr;
      }
      TRC_FUNCTION_LEAVE(PAR(this))
    }
  };

  /////////////////////
  // MqttService interface functions
  /////////////////////

  MqttService::MqttService()
  {
    TRC_FUNCTION_ENTER(PAR(this));
    m_impl = shape_new MqttService::Imp();
    TRC_FUNCTION_LEAVE(PAR(this))
  }

  MqttService::~MqttService()
  {
    TRC_FUNCTION_ENTER(PAR(this));
    delete m_impl;
    TRC_FUNCTION_LEAVE(PAR(this))
  }

  void MqttService::create(const std::string& clientId, const ConnectionPars& cp)
  {
    m_impl->create(clientId, cp);
  }

  void MqttService::destroy(const std::string& clientId)
  {
    m_impl->destroy(clientId);
  }

  void MqttService::connect()
  {
    m_impl->connect();
  }

  void MqttService::connect(MqttOnConnectHandlerFunc onConnect)
  {
    m_impl->connect(onConnect);
  }

  void MqttService::disconnect()
  {
    m_impl->disconnect();
  }

  void MqttService::disconnect(MqttOnDisconnectHandlerFunc onDisconnect)
  {
    m_impl->disconnect(onDisconnect);
  }

  bool MqttService::isReady() const
  {
    return m_impl->isReady();
  }

  void MqttService::registerMessageHandler(MqttMessageHandlerFunc hndl)
  {
    m_impl->registerMessageHandler(hndl);
  }

  void MqttService::unregisterMessageHandler()
  {
    m_impl->unregisterMessageHandler();
  }

  void MqttService::registerMessageStrHandler(MqttMessageStrHandlerFunc hndl)
  {
    m_impl->registerMessageStrHandler(hndl);
  }

  void MqttService::unregisterMessageStrHandler()
  {
    m_impl->unregisterMessageStrHandler();
  }

  void MqttService::registerOnConnectHandler(MqttOnConnectHandlerFunc hndl)
  {
    m_impl->registerOnConnectHandler(hndl);
  }

  void MqttService::unregisterOnConnectHandler()
  {
    m_impl->unregisterOnConnectHandler();
  }

  void MqttService::registerOnSubscribeHandler(MqttOnSubscribeHandlerFunc hndl)
  {
    m_impl->registerOnSubscribeHandler(hndl);
  }

  void MqttService::unregisterOnSubscribeHandler()
  {
    m_impl->unregisterOnSubscribeHandler();
  }

  void MqttService::registerOnDisconnectHandler(MqttOnDisconnectHandlerFunc hndl)
  {
    m_impl->registerOnDisconnectHandler(hndl);
  }

  void MqttService::unregisterOnDisconnectHandler()
  {
    m_impl->unregisterOnDisconnectHandler();
  }

  void MqttService::subscribe(const std::string& topic, int qos)
  {
    m_impl->subscribe(topic, qos);
  }

  void MqttService::subscribe(const std::string& topic, int qos
    , MqttOnSubscribeQosHandlerFunc onSubscribe, MqttMessageStrHandlerFunc onMessage)
  {
    m_impl->subscribe(topic, qos, onSubscribe, onMessage);
  }

  void MqttService::unsubscribe(const std::string& topic, MqttOnUnsubscribeHandlerFunc onUnsubscribe)
  {
    m_impl->unsubscribe(topic, onUnsubscribe);
  }

  void MqttService::publish(const std::string& topic, const std::vector<uint8_t> & msg, int qos)
  {
    m_impl->publish(topic, qos, msg);
  }

  void MqttService::publish(const std::string& topic, const std::string & msg, int qos)
  {
    m_impl->publish(topic, qos, msg);
  }

  void MqttService::publish(const std::string& topic, int qos, const std::vector<uint8_t> & msg
    , MqttOnSendHandlerFunc onSend, MqttOnDeliveryHandlerFunc onDelivery)
  {
    m_impl->publish(topic, qos, msg, onSend, onDelivery);
  }

  void MqttService::publish(const std::string& topic, int qos, const std::string & msg
    , MqttOnSendHandlerFunc onSend, MqttOnDeliveryHandlerFunc onDelivery)
  {
    m_impl->publish(topic, qos, msg, onSend, onDelivery);
  }

  void MqttService::activate(const shape::Properties *props)
  {
    m_impl->activate(props);
  }

  void MqttService::deactivate()
  {
    m_impl->deactivate();
  }

  void MqttService::modify(const shape::Properties *props)
  {
    m_impl->modify(props);
  }

  void MqttService::attachInterface(shape::ILaunchService* iface)
  {
    m_impl->attachInterface(iface);
  }

  void MqttService::detachInterface(shape::ILaunchService* iface)
  {
    m_impl->detachInterface(iface);
  }

  void MqttService::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void MqttService::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

}
