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
  
    //TODO just req, rep supported for now
    class SocketTypeConvertTable
    {
    public:
      static const std::vector<std::pair<zmq::socket_type, std::string>>& table()
      {
        static std::vector <std::pair<zmq::socket_type, std::string>> table = {
          { zmq::socket_type::req, "req" },
          { zmq::socket_type::rep, "rep" },
        };
        return table;
      }
      static zmq::socket_type defaultEnum()
      {
        return zmq::socket_type::req;
      }
      static const std::string& defaultStr()
      {
        static std::string u("Undef");
        return u;
      }
    };

    typedef shape::EnumStringConvertor<zmq::socket_type, SocketTypeConvertTable> SocketTypeConvert;

  
  
  private:
    zmq::context_t m_context;
    std::unique_ptr<zmq::socket_t> m_socket;
    std::mutex m_mux;
    std::condition_variable m_cvar;
    std::string m_socketAdr;
    std::string m_socketTypeStr;
    zmq::socket_type m_socketType;

    std::thread m_thd;
    bool m_runThd = true;
    bool m_listen = false;
    bool m_sentMessage = false;
    int m_replyWaitMillis = 2000; //TODO cfg

    OnMessageFunc m_onMessageFunc;
    OnReqTimeoutFunc m_onReqTimeoutFunc;

  public:
    Imp()
    {
    }

    ~Imp()
    {
    }

    void sendMessage(const std::string & msg)
    {
      TRC_FUNCTION_ENTER("");

      try {
        switch (m_socketType) {

        case zmq::socket_type::req:
        case zmq::socket_type::rep:
        {
          std::unique_lock<std::mutex> lck(m_mux);
          zmq::message_t request(msg.data(), msg.data() + msg.size());
          m_socket->send(request, zmq::send_flags::none);
          m_sentMessage = true;
          m_cvar.notify_one();
        }

        default:;
        }
      }
      catch (zmq::error_t &e) {
        CATCH_EXC_TRC_WAR(zmq::error_t, e, PAR(e.num()) << NAME_PAR(socketType, m_socketTypeStr));
      }

      TRC_FUNCTION_LEAVE("");
    }

    void open()
    {
      TRC_FUNCTION_ENTER("");

      std::unique_lock<std::mutex> lck(m_mux);
      m_socket.reset(shape_new zmq::socket_t(m_context, m_socketType));

      try {
        switch (m_socketType) {

        case zmq::socket_type::rep:
        {
          m_socket->bind(m_socketAdr);
          m_listen = true;
          m_cvar.notify_all();
          break;
        }

        case zmq::socket_type::req:
        {
          int linger = 5;
          m_socket->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
          m_socket->connect(m_socketAdr);
          break;
        }

        default:;
        }
      }
      catch (zmq::error_t &e) {
        CATCH_EXC_TRC_WAR(zmq::error_t, e, PAR(e.num()) << NAME_PAR(socketType, m_socketTypeStr));
      }

      TRC_FUNCTION_LEAVE("");
    }

    void close()
    {
      TRC_FUNCTION_ENTER("");

      if (m_socket) {
        m_listen = false;
        std::unique_lock<std::mutex> lck(m_mux);

        switch (m_socketType) {

        case zmq::socket_type::rep:
        {
          m_socket->unbind(m_socketAdr);
          std::this_thread::sleep_for(std::chrono::milliseconds(100)); //put some latency to reuse bind address
          break;
        }

        case zmq::socket_type::req:
        {
          m_socket->disconnect(m_socketAdr);
          break;
        }

        default:;
        }

        m_socket->close();
        m_socket.release();
      }
      TRC_FUNCTION_LEAVE("");
    }

    bool isConnected() const
    {
      return m_socket && m_socket->connected();
    }

    void registerOnMessage(OnMessageFunc fc)
    {
      m_onMessageFunc = fc;
    }

    void registerOnReqTimeout(OnReqTimeoutFunc fc)
    {
      m_onReqTimeoutFunc = fc;
    }

    void unregisterOnMessage()
    {
      m_onMessageFunc = nullptr;
    }

    void unregisterOnReqTimeout()
    {
      m_onReqTimeoutFunc = nullptr;
    }

    void listenReq()
    {
      TRC_FUNCTION_ENTER("");
      
      while (m_runThd) {
        try {
          //wait idle till expect reply
          std::unique_lock<std::mutex> lck(m_mux);
          m_cvar.wait(lck, [&] { return m_sentMessage; });
          
          //request sent => receive reply
          m_sentMessage = false;

          while (m_socket && m_runThd) {
            //  Poll socket for a reply, with timeout
            zmq::pollitem_t items[] = { { static_cast<void*>(*(m_socket.get())), 0, ZMQ_POLLIN, 0 } };
            zmq::poll(&items[0], 1, m_replyWaitMillis);

            //  If we got a reply, process it
            if (items[0].revents & ZMQ_POLLIN) {
              //  We got a reply from the server
              zmq::message_t reply;
              auto res = m_socket->recv(reply);
              if (res) {
                if (m_onMessageFunc) {
                  m_onMessageFunc(std::string((char*)reply.data(), (char*)reply.data() + reply.size()));
                }
                break; //on message
              }
            }
            else {
              TRC_WARNING("No reply for " << PAR(m_replyWaitMillis));
              
              //reset socket
              m_socket.reset(shape_new zmq::socket_t(m_context, m_socketType));
              int linger = 0;
              m_socket->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
              m_socket->connect(m_socketAdr);

              if (m_onReqTimeoutFunc) {
                m_onReqTimeoutFunc();
              }
              break; //on timeout
            }
          }
        }
        catch (zmq::error_t &e) {
          CATCH_EXC_TRC_WAR(zmq::error_t, e, PAR(e.num()) << NAME_PAR(socketType,m_socketTypeStr));
        }
      }
      TRC_FUNCTION_LEAVE("")
    }

    void listenRep()
    {
      TRC_FUNCTION_ENTER("");
      while (m_runThd) {
        try {
          //wait idle till m_socket ready for listen
          std::unique_lock<std::mutex> lck(m_mux);
          //TRC_DEBUG("wait1");
          m_cvar.wait(lck, [&] { return m_listen; });
          //TRC_DEBUG("out wait1");

          while (m_socket && m_runThd && m_listen) {
            //  Poll socket for a reply, with timeout
            zmq::pollitem_t items[] = { { static_cast<void*>(*(m_socket.get())), 0, ZMQ_POLLIN, 0 } };
            //TRC_DEBUG("poll");
            zmq::poll(&items[0], 1, 1500); //must be >1000 according example in http://zguide.zeromq.org/cpp:lpclient
            //TRC_DEBUG("out poll");

            //  If we got a request, process it
            if (items[0].revents & ZMQ_POLLIN) {
              //  We got a request from a client
              zmq::message_t request;
              auto res = m_socket->recv(request);
              if (res) {
                if (m_onMessageFunc) {
                  m_onMessageFunc(std::string((char*)request.data(), (char*)request.data() + request.size()));
                }
                m_sentMessage = false;
                //wait for async sent reply
                //TRC_DEBUG("wait2");
                m_cvar.wait(lck, [&] { return m_sentMessage; });
                //TRC_DEBUG("out wait2");
              }
            }
          }
        }
        catch (zmq::error_t &e) {
          CATCH_EXC_TRC_WAR(zmq::error_t, e, PAR(e.num()) << NAME_PAR(socketType, m_socketTypeStr));
        }
      }
      TRC_FUNCTION_LEAVE("")
    }

    void modify(const shape::Properties *props)
    {
      TRC_FUNCTION_ENTER("");

      props->getMemberAsString("socketAdr", m_socketAdr);
      props->getMemberAsString("socketType", m_socketTypeStr);

      m_socketType = SocketTypeConvert::str2enum(m_socketTypeStr);
      TRC_INFORMATION("Create socket: " << NAME_PAR(requiredSocketType, m_socketTypeStr))

      TRC_FUNCTION_LEAVE("");
    }

    void activate(const shape::Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "ZeroMqService instance activate" << std::endl <<
        "******************************"
      );

      modify(props);

      open();

      /*
      switch (m_socketType) {

      case zmq::socket_type::rep:
      {
        m_thd = std::thread([&]() { listenRep(); });
        break;
      }

      case zmq::socket_type::req:
      {
        m_thd = std::thread([&]() { listenReq(); });
        break;
      }

      default:;
      }
      */


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

      close();

      m_runThd = false;
      m_sentMessage = true;
      m_listen = true;
      m_cvar.notify_all();

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

  void ZeroMqService::sendMessage(const std::string & msg)
  {
    m_imp->sendMessage(msg);
  }

  void ZeroMqService::open()
  {
    m_imp->open();
  }

  void ZeroMqService::close()
  {
    m_imp->close();
  }

  bool ZeroMqService::isConnected() const
  {
    return m_imp->isConnected();
  }

  void ZeroMqService::registerOnMessage(OnMessageFunc fc)
  {
    m_imp->registerOnMessage(fc);
  }

  void ZeroMqService::registerOnReqTimeout(OnReqTimeoutFunc fc)
  {
    m_imp->registerOnReqTimeout(fc);
  }


  void ZeroMqService::unregisterOnMessage()
  {
    m_imp->unregisterOnMessage();
  }

  void ZeroMqService::unregisterOnReqTimeout()
  {
    m_imp->unregisterOnReqTimeout();
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
    m_imp->modify(props);
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
