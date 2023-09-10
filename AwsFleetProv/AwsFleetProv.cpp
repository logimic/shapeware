//see https://docs.aws.amazon.com/iot/latest/developerguide/fleet-provision-api.html

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 43

#include "AwsFleetProv.h"
#include "TimeString.h"
#include "Trace.h"

#include "JsonMacro.h"

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/pointer.h"

#include <thread>
#include <iostream>
#include <future>
#include <random>
#include <fstream>

#include <sstream>
#include <sys/stat.h>

// for windows mkdir
#ifdef SHAPE_PLATFORM_WINDOWS
#include <direct.h>
#endif

#include "shape__AwsFleetProv.hxx"

TRC_INIT_MODULE(shape::AwsFleetProv);

namespace shape {

  //cross platform mkdir class
  class Mkdir
  {
  public:
    static bool folder_exists(std::string path)
    {
      bool retval = false;

#ifdef SHAPE_PLATFORM_WINDOWS
      struct _stat info;
      if (_stat(path.c_str(), &info) != 0)
      {
        retval = false;
      }
      else {
        retval = ((info.st_mode & _S_IFDIR) != 0);
      }
#else 
     struct stat info;
      if (stat(path.c_str(), &info) != 0)
      {
        retval = false;
      }
      else {
        retval = ((info.st_mode & S_IFDIR) != 0);
    }
#endif      
      TRC_INFORMATION("Check folder: " << PAR(path) << " exists => " << PAR(retval));
      return retval;
    }

    static int myMkdir(const std::string & path)
    {
      TRC_INFORMATION("Create: " << PAR(path))
#ifdef SHAPE_PLATFORM_WINDOWS
      return ::_mkdir(path.c_str());
#else
      return ::mkdir(path.c_str(), 0755);
#endif
    }

    static int mkdir(const std::string & path)
    {
      TRC_INFORMATION("Create: " << PAR(path));
      
      std::string current_level = "";
      if (path.size() > 0 && path[0] == '/') {
        current_level += '/'; // absolute path => leading slash as root dir
      }
      std::string level;
      std::stringstream ss(path);

      // split path using slash as a separator
      while (std::getline(ss, level, '/'))
      {
        if (level.empty()) {
          continue; //just superfuous slash
        }
        current_level += level; // append folder to the current level

        // create current level
        if (!folder_exists(current_level)) {
          int retval = myMkdir(current_level);
          if (retval != 0) {
            TRC_WARNING("mkdir() returned: " << PAR(retval));
            return -1;
          }
        }

        current_level += "/"; // don't forget to append a slash
      }

      return 0;
    }
  };

  ////////////////////////
  class AwsFleetProv::Imp
  {
  private:
    shape::IIdentityProvider* m_iIdentityProvider = nullptr;
    shape::IMqttService* m_iMqttService = nullptr;
    shape::ILaunchService* m_iLaunchService = nullptr;

    std::thread m_runThread;
    bool m_runThreadFlag = false;

    std::mutex m_workMtx;
    std::condition_variable m_workCond;

    mutable std::mutex m_provisioningDataMtx;
    IMqttConnectionParsProvider::ProvisioningData m_provisioningData;
    MqttProvisioningHandlerFunc m_onProvisioned;
    MqttProvisioningHandlerErrorFunc m_onError;

    std::string m_instanceName;
    std::string m_mqttClientId;

    std::string m_brokerAddr;
    std::string m_templateName;

    std::string m_token;
    std::string m_thingName;
    std::string m_certificateId;

    std::string m_bootstrapCertificatesFileName;
    std::string m_bootstrapCertificatePemFileName;
    std::string m_bootstrapPrivatePemFileName;

    std::string m_officialCertStorePath;
    std::string m_officialCertificatesFileName;
    std::string m_officialCertificatePemFileName;
    std::string m_officialPrivatePemFileName;

    std::string m_officialProvisionFileName;

    std::string m_topicPrefix;

  public:
    Imp()
    {
    }

    ~Imp()
    {
    }

    void worker()
    {
      TRC_FUNCTION_ENTER("");

      try
      {
        bool connected = false;

        // onConnect handler invoked when connection succeded
        auto onConnect = [&]()
        {
          TRC_INFORMATION("MQTT connect: " << PAR(m_mqttClientId));
          std::unique_lock<std::mutex> lck(m_workMtx);
          connected = true;
          m_workCond.notify_one();
        };

        std::unique_lock<std::mutex> lck(m_workMtx);

        shape::IMqttService::ConnectionPars cp;
        cp.brokerAddress = m_brokerAddr;
        cp.certificate = m_bootstrapCertificatePemFileName;
        cp.privateKey = m_bootstrapPrivatePemFileName;

        //test cert file presence
        std::ifstream testCertFile(m_bootstrapCertificatePemFileName);
        if (!testCertFile.is_open()) {
          THROW_EXC_TRC_WAR(std::logic_error, "Cert file does not exist: " << PAR(m_bootstrapCertificatePemFileName));
        }
        testCertFile.close();

        //test key file presence
        std::ifstream testKeyFile(m_bootstrapPrivatePemFileName);
        if (!testKeyFile.is_open()) {
          THROW_EXC_TRC_WAR(std::logic_error, "Key file does not exist: " << PAR(m_bootstrapPrivatePemFileName));
        }
        testKeyFile.close();

        std::random_device rd;
        m_mqttClientId = m_instanceName + '_' + std::to_string(rd());

        m_iMqttService->create(m_mqttClientId, cp);
        m_iMqttService->connect(onConnect);

        //wait for connection
        auto status = m_workCond.wait_for(lck, std::chrono::seconds(60), [&]()->bool {return connected; });

        if (connected) {
          makeProvisioning();
          std::lock_guard<std::mutex> lckdata(m_provisioningDataMtx);

          // success
          m_provisioningData.m_isProvisioned = true;

          m_provisioningData.m_connectionPars.brokerAddress = m_brokerAddr;
          m_provisioningData.m_connectionPars.certificate = m_officialCertificatePemFileName;
          m_provisioningData.m_connectionPars.privateKey = m_officialPrivatePemFileName;

          if (m_onProvisioned) m_onProvisioned(m_provisioningData);
        }
        else {
          std::lock_guard<std::mutex> lckdata(m_provisioningDataMtx);

          // failure
          m_provisioningData.m_isProvisioned = false;
          if (m_onError) m_onError("Cannot connect to provisioning");
        }

        m_iMqttService->disconnect();
        m_iMqttService->destroy(m_mqttClientId);

      }
      catch (std::exception &e)
      {
        TRC_ERROR("Unexpected error " << e.what());
        if (m_onError) m_onError(e.what());
      }
      catch (...)
      {
        TRC_ERROR("Unknown error");
        std::cout << "Unknown error\n";
        if (m_onError) m_onError("Unknown error\n");
      }

      TRC_FUNCTION_LEAVE("");
    }

    void makeProvisioning()
    {
      TRC_FUNCTION_ENTER("");

      //TODO
      // repeat just registering if connection closed after keys - sometimes happen => reuse already created keys in pending.activate

      //TODO
      // wait on promise for limited time to allow thread cancel

      std::string topic1;
      std::string topic2;
      std::string topic3;
      std::string topic4;

      // on unsubscribed handler
      auto onUnsubscribed = [&](const std::string& topic, int result)
      {
        TRC_INFORMATION("onUnsubscribed: " << PAR(topic) << PAR(result));
      };

      try {
        std::promise<bool> keysAcceptedSubscribedPromise;
        std::promise<bool> keysRejectedSubscribedPromise;
        std::promise<bool> keysPublishSendPromise;
        std::promise<bool> keysPublishRespondedPromise;

        std::promise<bool> registerAcceptedSubscribePromise;
        std::promise<bool> registerRejectedSubscribePromise;
        std::promise<bool> registerPublishSendPromise;
        std::promise<bool> registerPublishRespondedPromise;

        ////////////////////////////////////////////
        // CreateKeysAndCertificate workflow

        // keys accepted topic subscription
        {
          // on subscribe handler invoked async by mqtt service as a result of subscription workflow
          auto onSubscribe = [&](const std::string& topic, int qos, bool result) {
            TRC_INFORMATION("onSubscribe: " << PAR(topic) << PAR(qos) << PAR(result));
            keysAcceptedSubscribedPromise.set_value(result);
          };

          // on message handler invoked async by mqtt service when msg with the topic is received
          auto onMessage = [&](const std::string& topic, const std::string & msg)
          {
            TRC_FUNCTION_ENTER("onMessage: " << PAR(topic))
              TRC_DEBUG(PAR(msg));

            using namespace rapidjson;

            Document doc;
            if (!msg.empty()) {
              doc.Parse(msg);
              if (doc.HasParseError()) {
                THROW_EXC_TRC_WAR(std::logic_error, "Json parse error in keys accepted: " << NAME_PAR(emsg, doc.GetParseError()) <<
                  NAME_PAR(eoffset, doc.GetErrorOffset()) << PAR(msg));
              }
            }

            std::string cert;
            std::string key;

            GET_JSON_AS_STR(doc, "/certificateId", m_certificateId);
            GET_JSON_AS_STR(doc, "/certificatePem", cert);
            GET_JSON_AS_STR(doc, "/privateKey", key);
            GET_JSON_AS_STR(doc, "/certificateOwnershipToken", m_token);

            TRC_INFORMATION("keysAccepted: "
              << PAR(m_certificateId) << std::endl
              //<< PAR(cert) << std::endl
              //<< PAR(key) << std::endl
              << PAR(m_token) << std::endl
            );

            if (! Mkdir::folder_exists(m_officialCertStorePath)) {
              TRC_INFORMATION("Checking: " << PAR(m_officialCertStorePath) << "folder not exists => to be created");
              Mkdir::mkdir(m_officialCertStorePath);
            }
            else {
              TRC_INFORMATION("Checking: " << PAR(m_officialCertStorePath) << "folder exists");
            }

            // save certificates
            std::ofstream certificatesFile(m_officialCertificatesFileName);
            if (!certificatesFile.is_open()) {
              THROW_EXC_TRC_WAR(std::logic_error, "Cannot open file: " << PAR(m_officialCertificatesFileName));
            }
            std::ofstream certificatePemFile(m_officialCertificatePemFileName);
            if (!certificatePemFile.is_open()) {
              THROW_EXC_TRC_WAR(std::logic_error, "Cannot open file: " << PAR(m_officialCertificatePemFileName));
            }
            std::ofstream privatePemFile(m_officialPrivatePemFileName);
            if (!privatePemFile.is_open()) {
              THROW_EXC_TRC_WAR(std::logic_error, "Cannot open file: " << PAR(m_officialPrivatePemFileName));
            }

            certificatesFile << msg; //save all 
            certificatePemFile << cert;
            privatePemFile << key;

            certificatesFile.close();
            certificatePemFile.close();
            privatePemFile.close();

            try {
              keysPublishRespondedPromise.set_value(true);
            }
            catch (std::exception & e) {
              CATCH_EXC_TRC_WAR(std::exception, e, "probably multiple msg as qos=1");
            }

            TRC_FUNCTION_LEAVE("onMessage: " << PAR(topic))
          };

          std::ostringstream subscribeTopic;
          subscribeTopic << "$aws"
            << "/"
            << "certificates"
            << "/"
            << "create"
            << "/"
            << "json"
            << "/"
            << "accepted";

          topic1 = subscribeTopic.str();
          TRC_INFORMATION("Subscribing to: " << PAR(topic1));
          m_iMqttService->subscribe(topic1, 1, onSubscribe, onMessage);
        }

        // keys rejected topic subscription
        {
          // on subscribe handler invoked async by mqtt service as a result of subscription workflow
          auto onSubscribe = [&](const std::string& topic, int qos, bool result) {
            TRC_INFORMATION("onSubscribe: " << PAR(topic) << PAR(qos) << PAR(result));
            keysRejectedSubscribedPromise.set_value(result);
          };

          // on message handler invoked async by mqtt service when msg with the topic is received
          auto onMessage = [&](const std::string& topic, const std::string & msg)
          {
            TRC_FUNCTION_ENTER("onMessage: " << PAR(topic) << PAR(msg));

            using namespace rapidjson;

            Document doc;
            if (!msg.empty()) {
              doc.Parse(msg);
              if (doc.HasParseError()) {
                THROW_EXC_TRC_WAR(std::logic_error, "Json parse error in keys accepted: " << NAME_PAR(emsg, doc.GetParseError()) <<
                  NAME_PAR(eoffset, doc.GetErrorOffset()) << PAR(msg));
              }
            }

            int statusCode;
            std::string errorCode;
            std::string errorMessage;
            GET_JSON_AS_INT(doc, "/statusCode", statusCode);
            GET_JSON_AS_STR(doc, "/errorCode", errorCode);
            GET_JSON_AS_STR(doc, "/errorMessage", errorMessage);

            TRC_INFORMATION("keys rejected: " << PAR(statusCode) << PAR(errorCode) << PAR(errorMessage));

            try {
              keysPublishRespondedPromise.set_value(false);
            }
            catch (std::exception & e) {
              CATCH_EXC_TRC_WAR(std::exception, e, "probably multiple msg as qos=1");
            }

            TRC_FUNCTION_LEAVE("onMessage: " << PAR(topic) << PAR(msg))
          };

          std::ostringstream subscribeTopic;
          subscribeTopic << "$aws"
            << "/"
            << "certificates"
            << "/"
            << "create"
            << "/"
            << "json"
            << "/"
            << "rejected";

          topic2 = subscribeTopic.str();
          TRC_INFORMATION("Subscribing to: " << PAR(topic2));
          m_iMqttService->subscribe(topic2, 1, onSubscribe, onMessage);
        }

        // waiting for subscriptions
        bool keysAcceptedResult = keysAcceptedSubscribedPromise.get_future().get();
        if (!keysAcceptedResult) {
          THROW_EXC_TRC_WAR(std::logic_error, "Cannot subscribe: " << PAR(keysAcceptedResult));
        }

        bool keysRejectedResult = keysRejectedSubscribedPromise.get_future().get();
        if (!keysRejectedResult) {
          THROW_EXC_TRC_WAR(std::logic_error, "Cannot subscribe: " << PAR(keysRejectedResult));
        }

        // Publishing to CreateKeysAndCertificate topic
        {
          // on send handler invoked async by mqtt service when msg send to server
          auto onSend = [&](const std::string& topic, int qos, int result)
          {
            TRC_INFORMATION("onSend: " << PAR(topic) << PAR(result));
            keysPublishSendPromise.set_value(result != 0);
          };

          // on delivered handler invoked async by mqtt service when msg send to server
          auto onDelivered = [&](const std::string& topic, int qos, int result)
          {
            TRC_INFORMATION("onDelivered: " << PAR(topic) << PAR(result));
          };

          std::ostringstream publishTopic;
          publishTopic << "$aws"
            << "/"
            << "certificates"
            << "/"
            << "create"
            << "/"
            << "json";

          std::string topic = publishTopic.str();
          TRC_INFORMATION("Publishing to: " << PAR(topic));
          m_iMqttService->publish(publishTopic.str(), 1, "{}", onSend, onDelivered);
        }

        // waiting for keyPublish send results
        bool keysPublishSendResult = keysPublishSendPromise.get_future().get();
        if (!keysPublishSendResult) {
          THROW_EXC_TRC_WAR(std::logic_error, "Cannot create keys: " << PAR(keysPublishSendResult));
        }

        // waiting for keyPublish responded results
        bool keysPublishRespondedResult = keysPublishRespondedPromise.get_future().get();
        if (!keysPublishRespondedResult) {
          THROW_EXC_TRC_WAR(std::logic_error, "Cannot create keys: " << PAR(keysPublishRespondedResult));
        }

        //////////////////////////////////////
        // Register thing workflow

        // register thing accepted topic subscription
        {
          // on subscribe handler invoked async by mqtt service as a result of subscription workflow
          auto onSubscribe = [&](const std::string& topic, int qos, bool result) {
            TRC_INFORMATION("onSubscribe: " << PAR(topic) << PAR(qos) << PAR(result));
            registerAcceptedSubscribePromise.set_value(result);
          };

          // on message handler invoked async by mqtt service when msg with the topic is received
          auto onMessage = [&](const std::string& topic, const std::string & msg)
          {
            TRC_FUNCTION_ENTER("onMessage: " << PAR(topic) << PAR(msg));

            //save register result
            std::ofstream provisionFile(m_officialProvisionFileName);
            if (!provisionFile.is_open()) {
              THROW_EXC_TRC_WAR(std::logic_error, "Cannot open file: " << PAR(m_officialProvisionFileName));
            }
            provisionFile << msg;
            provisionFile.close();

            exploreProvisionFile();

            TRC_INFORMATION("register accepted: " << PAR(m_thingName));

            try {
              registerPublishRespondedPromise.set_value(true);
            }
            catch (std::exception & e) {
              CATCH_EXC_TRC_WAR(std::exception, e, "probably multiple msg as qos=1");
            }

            TRC_FUNCTION_LEAVE("onMessage: " << PAR(topic) << PAR(msg))
          };

          std::ostringstream subscribeTopic;
          subscribeTopic << "$aws"
            << "/"
            << "provisioning-templates"
            << "/" << m_templateName << "/"
            << "provision"
            << "/"
            << "json"
            << "/"
            << "accepted";

          topic3 = subscribeTopic.str();
          TRC_INFORMATION("Subscribing to: " << PAR(topic3));
          m_iMqttService->subscribe(topic3, 1, onSubscribe, onMessage);
        }

        // register thing rejected topic subscription
        {
          // on subscribe handler invoked async by mqtt service as a result of subscription workflow
          auto onSubscribe = [&](const std::string& topic, int qos, bool result) {
            TRC_INFORMATION("onSubscribe: " << PAR(topic) << PAR(qos) << PAR(result));
            registerRejectedSubscribePromise.set_value(result);
          };

          // on message handler invoked async by mqtt service when msg with the topic is received
          auto onMessage = [&](const std::string& topic, const std::string & msg)
          {
            TRC_FUNCTION_ENTER("onMessage: " << PAR(topic) << PAR(msg));

            using namespace rapidjson;

            Document doc;
            if (!msg.empty()) {
              doc.Parse(msg);
              if (doc.HasParseError()) {
                THROW_EXC_TRC_WAR(std::logic_error, "Json parse error in keys rejected: " << NAME_PAR(emsg, doc.GetParseError()) <<
                  NAME_PAR(eoffset, doc.GetErrorOffset()) << PAR(msg));
              }
            }

            int statusCode;
            std::string errorCode;
            std::string errorMessage;
            GET_JSON_AS_INT(doc, "/statusCode", statusCode);
            GET_JSON_AS_STR(doc, "/errorCode", errorCode);
            GET_JSON_AS_STR(doc, "/errorMessage", errorMessage);

            TRC_INFORMATION("register rejected: " << PAR(statusCode) << PAR(errorCode) << PAR(errorMessage));

            try {
              registerPublishRespondedPromise.set_value(false);
            }
            catch (std::exception & e) {
              CATCH_EXC_TRC_WAR(std::exception, e, "probably multiple msg as qos=1");
            }

            TRC_FUNCTION_LEAVE("onMessage: " << PAR(topic) << PAR(msg))
          };

          std::ostringstream subscribeTopic;
          subscribeTopic << "$aws"
            << "/"
            << "provisioning-templates"
            << "/" << m_templateName << "/"
            << "provision"
            << "/"
            << "json"
            << "/"
            << "rejected";

          topic4 = subscribeTopic.str();
          TRC_INFORMATION("Subscribing to: " << PAR(topic4));
          m_iMqttService->subscribe(topic4, 1, onSubscribe, onMessage);
        }

        // waiting for subscriptions
        bool registerAcceptedResult = registerAcceptedSubscribePromise.get_future().get();
        bool registerRejectedResult = registerRejectedSubscribePromise.get_future().get();

        if (!registerAcceptedResult) {
          THROW_EXC_TRC_WAR(std::logic_error, "Cannot subscribe: " << PAR(registerAcceptedResult));
        }
        if (!registerRejectedResult) {
          THROW_EXC_TRC_WAR(std::logic_error, "Cannot subscribe: " << PAR(registerRejectedResult));
        }

        // Publishing to RegisterThing topic
        {
          // on send handler invoked async by mqtt service when msg send to server
          auto onSend = [&](const std::string& topic, int qos, int result)
          {
            TRC_INFORMATION("onSend: " << PAR(topic) << PAR(result));
            registerPublishSendPromise.set_value(result != 0);
          };

          // on delivered handler invoked async by mqtt service when msg send to server
          auto onDelivered = [&](const std::string& topic, int qos, int result)
          {
            TRC_INFORMATION("onDelivered: " << PAR(topic) << PAR(result));
            //registerPublishCompletedPromise.set_value(result == 0);
          };

          using namespace rapidjson;

          std::ostringstream publishTopic;
          publishTopic << "$aws"
            << "/"
            << "provisioning-templates"
            << "/" << m_templateName << "/"
            << "provision"
            << "/"
            << "json";

          std::ostringstream thingNameOs;
          thingNameOs << "iqrfcloud-" <<
            m_iIdentityProvider->getParams().m_vendor << '-' <<
            m_iIdentityProvider->getParams().m_product << '-' <<
            m_iIdentityProvider->getParams().m_serialNumber;

          m_thingName = thingNameOs.str();
          
          Document registerDoc;
          // set token
          Pointer("/certificateOwnershipToken").Set(registerDoc, m_token);
          // set params
          Pointer("/parameters/Vendor").Set(registerDoc, m_iIdentityProvider->getParams().m_vendor);
          Pointer("/parameters/Product").Set(registerDoc, m_iIdentityProvider->getParams().m_product);
          Pointer("/parameters/SerialNumber").Set(registerDoc, m_iIdentityProvider->getParams().m_serialNumber);
          Pointer("/parameters/GwType").Set(registerDoc, m_iIdentityProvider->getParams().m_gwType);
          Pointer("/parameters/Application").Set(registerDoc, m_iLaunchService->getAppName());
          Pointer("/parameters/DevStage").Set(registerDoc, m_iIdentityProvider->getParams().m_devStage);
          Pointer("/parameters/CertId").Set(registerDoc, m_certificateId);
          Pointer("/parameters/ThingName").Set(registerDoc, m_thingName);

          rapidjson::StringBuffer buffer;
          rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
          registerDoc.Accept(writer);

          std::string msgToSend = buffer.GetString();

          std::string topic = publishTopic.str();
          TRC_INFORMATION("Publishing to: " << PAR(topic) << std::endl
           << msgToSend
          );
          m_iMqttService->publish(publishTopic.str(), 1, msgToSend, onSend, onDelivered);
        }

        // waiting for registerPublish send result
        bool registerPublishSendResult = registerPublishSendPromise.get_future().get();
        if (!registerPublishSendResult) {
          THROW_EXC_TRC_WAR(std::logic_error, "Cannot register: " << PAR(registerPublishSendResult));
        }

        // waiting for registerResponded result
        bool registerPublishRespondedResult = registerPublishRespondedPromise.get_future().get();
        if (!registerPublishRespondedResult) {
          THROW_EXC_TRC_WAR(std::logic_error, "Cannot register: " << PAR(registerPublishRespondedResult));
        }

        m_iMqttService->unsubscribe(topic4, onUnsubscribed);
        m_iMqttService->unsubscribe(topic3, onUnsubscribed);
        m_iMqttService->unsubscribe(topic2, onUnsubscribed);
        m_iMqttService->unsubscribe(topic1, onUnsubscribed);
      }
      catch (std::exception &e)
      {
        CATCH_EXC_TRC_WAR(std::exception, e, "cancel provisioning");
        m_iMqttService->unsubscribe(topic1, onUnsubscribed);
        m_iMqttService->unsubscribe(topic2, onUnsubscribed);
        m_iMqttService->unsubscribe(topic3, onUnsubscribed);
        m_iMqttService->unsubscribe(topic4, onUnsubscribed);
        throw e;
      }

      TRC_FUNCTION_LEAVE("");
    }

    void exploreProvisionFile()
    {
      TRC_FUNCTION_ENTER("");

      using namespace rapidjson;

      Document doc;
      std::ifstream ifs(m_officialProvisionFileName);
      if (!ifs.is_open()) {
        THROW_EXC_TRC_WAR(std::logic_error, "Cannot open: " << PAR(m_officialProvisionFileName));
      }

      IStreamWrapper isw(ifs);
      doc.ParseStream(isw);

      if (doc.HasParseError()) {
        THROW_EXC_TRC_WAR(std::logic_error, "Json parse error: " << NAME_PAR(emsg, doc.GetParseError()) <<
          NAME_PAR(eoffset, doc.GetErrorOffset()));
      }

      //GET_JSON_AS_STR(doc, "/thingName", m_thingName);

      if (false) {
      //if (m_iIdentityProvider->getParams().m_devStage == "dev4") { //dev4 topic proposal not supported now, maybe dev5
        const rapidjson::Value *pv = rapidjson::Pointer("/deviceConfiguration/provisionedKey").Get(doc);
        if (pv && pv->IsString()) {
          m_provisioningData.m_provisionedKey = pv->GetString();
        }
        else {
          TRC_ERROR("Parsing: " << PAR(m_officialProvisionFileName)
            << NAME_PAR(devStage, m_iIdentityProvider->getParams().m_devStage)
            << " but \"/deviceConfiguration/provisionedKey\" is not presented \n"
            << " => going to temporary debug fallback generating provisionedKey from " << PAR(m_thingName));
          m_provisioningData.m_provisionedKey = m_thingName;
        }
      }
      else {
        const auto & par = m_iIdentityProvider->getParams();
        m_provisioningData.m_provisionedKey = par.m_vendor + '/' + par.m_product + '/' + par.m_serialNumber;
      }

      const auto & par = m_iIdentityProvider->getParams();

      std::ostringstream osTpc;

      osTpc << par.m_topicRoot << '/'
        << par.m_devStage << '/'
        << m_provisioningData.m_provisionedKey;

      m_topicPrefix = osTpc.str();

      TRC_FUNCTION_LEAVE("")
    };

    void launchProvisioning(MqttProvisioningHandlerFunc onProvisioned, MqttProvisioningHandlerErrorFunc onError, bool inThread)
    {
      TRC_FUNCTION_ENTER("");

      TRC_INFORMATION("launched pProvisioning");

      m_onProvisioned = onProvisioned;
      m_onError = onError;

      if (inThread) {
        // stop worker if already running
        if (m_runThreadFlag) {
          m_runThreadFlag = false;
          //m_workCond.notify_all();
          if (m_runThread.joinable())
            m_runThread.join();
        }

        // start worker
        if (!m_runThreadFlag) {
          m_runThreadFlag = true;
          m_runThread = std::thread([&]() {
            worker();
            });
        }
      }
      else {
        worker();
      }

      TRC_FUNCTION_LEAVE("");
    }

    void unregisterProvisioningHandlers() {
      m_onError = nullptr;
      m_onProvisioned = nullptr;
    }

    IMqttConnectionParsProvider::ProvisioningData getProvisioningData() const
    {
      std::lock_guard<std::mutex> lck(m_provisioningDataMtx);
      return m_provisioningData;
    }

    const std::string & getTopicPrefix() const
    {
      if (m_provisioningData.m_provisionedKey.empty()) {
        THROW_EXC_TRC_WAR(std::logic_error, "Provisioning key was not set yet");
      }

      return m_topicPrefix;
    }


    void activate(const shape::Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "AwsFleetProv instance activate" << std::endl <<
        "******************************"
      );

      modify(props);

      std::string dataDir = m_iLaunchService->getDataDir();

      std::string bootstrapCertStorePath = dataDir + "/cert/bootstrap/";

      m_bootstrapCertificatePemFileName = bootstrapCertStorePath + "certificate.pem.crt";
      m_bootstrapPrivatePemFileName = bootstrapCertStorePath + "private.pem.key";

      m_officialCertStorePath = dataDir + "/cert/official/";

      m_officialCertificatesFileName = m_officialCertStorePath + "certificates.json";
      m_officialCertificatePemFileName = m_officialCertStorePath + "certificate.pem.crt";
      m_officialPrivatePemFileName = m_officialCertStorePath + "private.pem.key";
      m_officialProvisionFileName = m_officialCertStorePath + "provision.json";

      // check if official certificates exists
      std::ifstream certificatesFile(m_officialCertificatesFileName);
      std::ifstream certificatePemFile(m_officialCertificatePemFileName);
      std::ifstream privatePemFile(m_officialPrivatePemFileName);
      std::ifstream provisionFile(m_officialProvisionFileName);

      if (certificatesFile.is_open() && certificatePemFile.is_open() && privatePemFile.is_open() && provisionFile.is_open()) {
        
        TRC_INFORMATION("Official provision files exists => provisioning was already done");
        
        std::lock_guard<std::mutex> lck(m_provisioningDataMtx);
        m_provisioningData.m_isProvisioned = true;
        m_provisioningData.m_connectionPars.brokerAddress = m_brokerAddr;
        m_provisioningData.m_connectionPars.certificate = m_officialCertificatePemFileName;
        m_provisioningData.m_connectionPars.privateKey = m_officialPrivatePemFileName;

        //init provisioned data
        exploreProvisionFile();
      }
      else {

        TRC_INFORMATION("Official provision files does not exists => provisioning was not done yeat");

        std::lock_guard<std::mutex> lck(m_provisioningDataMtx);
        m_provisioningData.m_isProvisioned = false;
        m_provisioningData.m_connectionPars.brokerAddress.clear();
        m_provisioningData.m_connectionPars.certificate.clear();
        m_provisioningData.m_connectionPars.privateKey.clear();
      }
      
      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");

      if (m_iMqttService) {
        m_iMqttService->unregisterMessageStrHandler();
        m_iMqttService->unregisterOnConnectHandler();
        m_iMqttService->disconnect();
      }

      m_runThreadFlag = false;
      //m_workCond.notify_all();
      if (m_runThread.joinable())
        m_runThread.join();

      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "AwsFleetProv instance deactivate" << std::endl <<
        "******************************"
      );
      TRC_FUNCTION_LEAVE("")
    }

    void modify(const shape::Properties *props)
    {
      using namespace rapidjson;

      const Document& doc = props->getAsJson();

      {
        const Value* v = Pointer("/instance").Get(doc);
        if (v && v->IsString()) {
          m_instanceName = v->GetString();
        }
      }
      {
        const Value* v = Pointer("/brokerAddr").Get(doc); //aws broker passed during installation
        if (v && v->IsString()) {
          m_brokerAddr = v->GetString();
        }
      }
      {
        const Value* v = Pointer("/templateName").Get(doc); //aws fleet provisioning template name passed during installation
        if (v && v->IsString()) {
          m_templateName = v->GetString();
        }
      }

      TRC_INFORMATION("Configuration: "
        << PAR(m_instanceName)
        << PAR(m_brokerAddr)
        << PAR(m_templateName)
      );

    }

    void attachInterface(shape::IIdentityProvider* iface)
    {
      m_iIdentityProvider = iface;
    }

    void detachInterface(shape::IIdentityProvider* iface)
    {
      if (m_iIdentityProvider == iface) {
        m_iIdentityProvider = nullptr;
      }
    }

    void attachInterface(shape::IMqttService* iface)
    {
      m_iMqttService = iface;
    }

    void detachInterface(shape::IMqttService* iface)
    {
      if (m_iMqttService == iface) {
        m_iMqttService = nullptr;
      }
    }

    void attachInterface(shape::ILaunchService* iface)
    {
      m_iLaunchService = iface;
    }

    void detachInterface(shape::ILaunchService* iface)
    {
      if (m_iLaunchService == iface) {
        m_iLaunchService = nullptr;
      }
    }

  };

  ///////////////////////
  AwsFleetProv::AwsFleetProv()
  {
    m_imp = shape_new Imp();
  }

  AwsFleetProv::~AwsFleetProv()
  {
    delete m_imp;
  }

  void AwsFleetProv::launchProvisioning(MqttProvisioningHandlerFunc onProvisioned, MqttProvisioningHandlerErrorFunc onError, bool inThread)
  {
    m_imp->launchProvisioning(onProvisioned, onError, inThread);
  }

  void AwsFleetProv::unregisterProvisioningHandlers()
  {
    m_imp->unregisterProvisioningHandlers();
  }

  IMqttConnectionParsProvider::ProvisioningData AwsFleetProv::getProvisioningData() const
  {
    return m_imp->getProvisioningData();
  }

  const std::string & AwsFleetProv::getTopicPrefix() const
  {
    return m_imp->getTopicPrefix();
  }

  void AwsFleetProv::activate(const shape::Properties *props)
  {
    m_imp->activate(props);
  }

  void AwsFleetProv::deactivate()
  {
    m_imp->deactivate();
  }

  void AwsFleetProv::modify(const shape::Properties *props)
  {
  }

  void AwsFleetProv::attachInterface(shape::IIdentityProvider* iface)
  {
    m_imp->attachInterface(iface);
  }

  void AwsFleetProv::detachInterface(shape::IIdentityProvider* iface)
  {
    m_imp->detachInterface(iface);
  }

  void AwsFleetProv::attachInterface(shape::IMqttService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void AwsFleetProv::detachInterface(shape::IMqttService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void AwsFleetProv::attachInterface(shape::ILaunchService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void AwsFleetProv::detachInterface(shape::ILaunchService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void AwsFleetProv::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void AwsFleetProv::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

}
