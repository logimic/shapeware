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

#include "ZeroMqService.h"
#include "Trace.h"
#include <thread>
#include <zmq.hpp>

#include "shape__ZeroMqService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::ZeroMqService);

namespace shape {

  class ZeroMqService::Imp
  {
  private:
    zmq::context_t m_context;
    std::unique_ptr<zmq::socket_t> m_socket;
    std::string m_uri;
    std::string m_server;
    std::string m_error_reason;

    std::thread m_thd;
    bool m_runListen = true;

    MessageHandlerFunc m_messageHandlerFunc;
    MessageStrHandlerFunc m_messageStrHandlerFunc;
    OpenHandlerFunc m_openHandlerFunc;
    CloseHandlerFunc m_closeHandlerFunc;

    /*
    void on_message(connection_hdl hdl, WsClient::message_ptr msg)
    {
      //TRC_FUNCTION_ENTER("");
      
      (void)hdl; //silence -Wunused-parameter

      if (m_messageStrHandlerFunc) {
        m_messageStrHandlerFunc(msg->get_payload());
      }

      if (m_messageHandlerFunc) {
        uint8_t* buf = (uint8_t*)msg->get_payload().data();
        std::vector<uint8_t> vmsg(buf, buf + msg->get_payload().size());
        m_messageHandlerFunc(vmsg);
      }

      //TRC_FUNCTION_LEAVE("");
    }

    void on_fail(connection_hdl hdl)
    {
      TRC_FUNCTION_ENTER("");

      std::unique_lock<std::mutex> lck(m_connectedMux);
      m_connected = false;
      //std::cout << ">>> ZeroMqService on_fail" << std::endl;
      m_server = m_client.get_con_from_hdl(hdl)->get_response_header("Server");
      m_error_reason = m_client.get_con_from_hdl(hdl)->get_ec().message();
      m_connectedCondition.notify_all();
      TRC_WARNING("Error: " << PAR(m_error_reason));

      TRC_FUNCTION_LEAVE("");
    }

    void on_close(connection_hdl hdl)
    {
      TRC_FUNCTION_ENTER("");

      std::unique_lock<std::mutex> lck(m_connectedMux);
      m_connected = false;
      
      std::stringstream s;
      auto con = m_client.get_con_from_hdl(hdl);
      s << "close code: " << con->get_remote_close_code() << " ("
        << websocketpp::close::status::get_string(con->get_remote_close_code())
        << "), close reason: " << con->get_remote_close_reason();
      m_error_reason = s.str();

      //std::cout << ">>> ZeroMqService CloseRemote" << std::endl;
      m_connectedCondition.notify_all();

      if (m_closeHandlerFunc) {
        m_closeHandlerFunc();
      }

      TRC_FUNCTION_LEAVE("");
    }
    */
    ///////////////////////////////

  public:
    Imp()
    {
    }

    ~Imp()
    {
    }

    void sendMessage(const std::vector<uint8_t> & msg)
    {
      TRC_FUNCTION_ENTER("");

      zmq::message_t request(msg.data(), msg.data() + msg.size());
      //m_socket->send(request, zmq::send_flags::none);
      m_socket->send(&request, 0);

      TRC_FUNCTION_LEAVE("");
    }

    void sendMessage(const std::string & msg)
    {
      TRC_FUNCTION_ENTER("");

      zmq::message_t request(msg.data(), msg.data() + msg.size());
      //m_socket->send(request, zmq::send_flags::none);
      m_socket->send(&request, 0);

      TRC_FUNCTION_LEAVE("");
    }

    void connect(const std::string & uri)
    {
      TRC_FUNCTION_ENTER(PAR(uri));

      //m_socket->connect("tcp://localhost:5555");
      m_socket->connect(uri);

      TRC_FUNCTION_LEAVE("");
    }

    void close()
    {
      TRC_FUNCTION_ENTER("");

      //m_socket->disconnect(uri);

      TRC_FUNCTION_LEAVE("");
    }

    bool isConnected() const
    {
      return m_socket->connected();
    }

    void registerMessageHandler(MessageHandlerFunc hndl)
    {
      m_messageHandlerFunc = hndl;
    }

    void registerMessageStrHandler(MessageStrHandlerFunc hndl)
    {
      m_messageStrHandlerFunc = hndl;
    }

    void registerOpenHandler(OpenHandlerFunc hndl)
    {
      m_openHandlerFunc = hndl;
    }

    void registerCloseHandler(CloseHandlerFunc hndl)
    {
      m_closeHandlerFunc = hndl;
    }

    void unregisterMessageHandler()
    {
      m_messageHandlerFunc = nullptr;
    }

    void unregisterMessageStrHandler()
    {
      m_messageStrHandlerFunc = nullptr;
    }

    void unregisterOpenHandler()
    {
      m_openHandlerFunc = nullptr;
    }

    void unregisterCloseHandler()
    {
      m_closeHandlerFunc = nullptr;
    }

    void listen()
    {
      TRC_FUNCTION_ENTER("");
      while (m_runListen) {
        zmq::message_t reply;
        //m_socket->recv(reply, zmq::recv_flags::none);
        m_socket->recv(&reply, 0);
      }
      TRC_FUNCTION_LEAVE("")
    }

    void activate(const shape::Properties *props)
    {
      (void)props; //silence -Wunused-parameter
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "ZeroMqService instance activate" << std::endl <<
        "******************************"
      );

      m_socket.reset(shape_new zmq::socket_t(m_context, ZMQ_REQ));

      m_thd = std::thread([&]() { listen(); });


      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "ZeroMqService instance deactivate" << std::endl <<
        "******************************"
      );

      m_runListen = false;
      m_socket->close();
      if (m_thd.joinable())
        m_thd.join();


      TRC_FUNCTION_LEAVE("")
    }

  };

  ///////////////////////////////////////
  ZeroMqService::ZeroMqService()
  {
    m_imp = shape_new Imp();
  }

  ZeroMqService::~ZeroMqService()
  {
    delete m_imp;
  }

  void ZeroMqService::sendMessage(const std::vector<uint8_t> & msg)
  {
    m_imp->sendMessage(msg);
  }

  void ZeroMqService::sendMessage(const std::string & msg)
  {
    m_imp->sendMessage(msg);
  }

  void ZeroMqService::connect(const std::string & uri)
  {
    m_imp->connect(uri);
  }

  void ZeroMqService::close()
  {
    m_imp->close();
  }

  bool ZeroMqService::isConnected() const
  {
    return m_imp->isConnected();
  }

  void ZeroMqService::registerMessageHandler(MessageHandlerFunc hndl)
  {
    m_imp->registerMessageHandler(hndl);
  }

  void ZeroMqService::registerMessageStrHandler(MessageStrHandlerFunc hndl)
  {
    m_imp->registerMessageStrHandler(hndl);
  }

  void ZeroMqService::registerOpenHandler(OpenHandlerFunc hndl)
  {
    m_imp->registerOpenHandler(hndl);
  }

  void ZeroMqService::registerCloseHandler(CloseHandlerFunc hndl)
  {
    m_imp->registerCloseHandler(hndl);
  }

  void ZeroMqService::unregisterMessageHandler()
  {
    m_imp->unregisterMessageHandler();
  }

  void ZeroMqService::unregisterMessageStrHandler()
  {
    m_imp->unregisterMessageStrHandler();
  }

  void ZeroMqService::unregisterOpenHandler()
  {
    m_imp->unregisterOpenHandler();
  }

  void ZeroMqService::unregisterCloseHandler()
  {
    m_imp->unregisterCloseHandler();
  }

  void ZeroMqService::activate(const shape::Properties *props)
  {
    m_imp->activate(props);
  }

  void ZeroMqService::deactivate()
  {
    m_imp->deactivate();
  }

  void ZeroMqService::modify(const shape::Properties *props)
  {
    (void)props; //silence -Wunused-parameter
  }

  void ZeroMqService::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void ZeroMqService::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }


}
