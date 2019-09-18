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
#include "zmq.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>

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

    enum class SocketState {
      open,
      work,
      close
    };
  
  private:
    zmq::context_t m_context;
    mutable std::mutex m_mux;
    std::condition_variable m_cvar;
    std::mutex m_muxReady;
    std::condition_variable m_cvarReady;
    bool m_socketReady = false;
    std::string m_socketAdr;
    std::string m_socketTypeStr;
    zmq::socket_type m_socketType;
    SocketState m_requireSocketState = SocketState::close;

    std::thread m_thd;
    bool m_runThd = true;
    bool m_sendMessage = false;
    int m_replyWaitMillis = 2000;

    std::string m_messageToSend;

    OnMessageFunc m_onMessageFunc;
    OnReqTimeoutFunc m_onReqTimeoutFunc;

  public:
    Imp()
      :m_context(1)
    {
    }

    ~Imp()
    {
    }

    void sendMessage(const std::string & msg)
    {
      TRC_FUNCTION_ENTER("");

      {
        std::unique_lock<std::mutex> lck(m_mux);
        m_messageToSend = msg;
        m_sendMessage = true;
      }
      m_cvar.notify_all();

      TRC_FUNCTION_LEAVE("");
    }

    void open()
    {
      TRC_FUNCTION_ENTER("");

      {
        TRC_DEBUG("lock1");
        std::unique_lock<std::mutex> lck(m_mux);
        TRC_DEBUG("lock2");
        m_socketReady = false;
        m_requireSocketState = SocketState::open;
        m_cvar.notify_all();
        TRC_DEBUG("lock3");
      }

      {
        TRC_DEBUG("lock4");
        std::unique_lock<std::mutex> lck(m_muxReady);
        TRC_DEBUG("lock5");
        m_cvarReady.wait(lck, [&] { return m_socketReady; });
        TRC_DEBUG("lock6");
      }

      TRC_FUNCTION_LEAVE("");
    }

    void close()
    {
      TRC_FUNCTION_ENTER("");

      {
        std::unique_lock<std::mutex> lck(m_mux);
        m_requireSocketState = SocketState::close;
      }
      m_cvar.notify_all();

      {
        std::unique_lock<std::mutex> lck(m_muxReady);
        m_cvarReady.wait(lck);
      }

      TRC_FUNCTION_LEAVE("");
    }

    bool isOpen() const
    {
      std::unique_lock<std::mutex> lck(m_mux);
      return m_requireSocketState == SocketState::open;
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

    void workerReq()
    {
      TRC_FUNCTION_ENTER("");
      
      std::unique_ptr<zmq::socket_t> m_socket;
      bool resetSocket = false;

      while (m_runThd) {
        //wait idle to open
        std::unique_lock<std::mutex> lck(m_mux);
        m_cvar.wait(lck, [&] { return m_requireSocketState == SocketState::open || !m_runThd; });

        if (!m_runThd) {
          break; //finish thread
        }

        try {
          TRC_DEBUG("open socket1");
          m_socket.reset(shape_new zmq::socket_t(m_context, m_socketType));
          int linger = 5;
          m_socket->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
          TRC_DEBUG("open socket2");
          m_socket->connect(m_socketAdr);
          TRC_DEBUG("open socket3");
          resetSocket = false;
          {
            std::unique_lock<std::mutex> lck(m_muxReady);
            m_socketReady = true;
            m_cvarReady.notify_all();
            TRC_DEBUG("open socket4");
          }

          while (true) {
            //wait idle till request to send
            m_cvar.wait(lck, [&] { return m_sendMessage || m_requireSocketState == SocketState::close || !m_runThd; });

            if (m_sendMessage) {

              m_sendMessage = false;
              zmq::message_t request(m_messageToSend.data(), m_messageToSend.data() + m_messageToSend.size());
              m_socket->send(request, zmq::send_flags::none);

              //  wait in poll socket for a reply, with timeout
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
                }
              }
              else {
                TRC_WARNING("No reply for " << PAR(m_replyWaitMillis));

                if (m_onReqTimeoutFunc) {
                  m_onReqTimeoutFunc();
                }
                  
                resetSocket = true;
              }
            }
            
            if (resetSocket || m_requireSocketState == SocketState::close || !m_runThd) {
              m_socket->disconnect(m_socketAdr);
              m_socket->close();
              m_cvarReady.notify_all();
              break; //go to wait to open again
            }
          }
        }
        catch (zmq::error_t &e) {
          CATCH_EXC_TRC_WAR(zmq::error_t, e, PAR(e.num()) << NAME_PAR(socketType,m_socketTypeStr));
        }
      }

      TRC_FUNCTION_LEAVE("")
    }

    void workerRep()
    {
      TRC_FUNCTION_ENTER("");

      std::unique_ptr<zmq::socket_t> m_socket;

      while (m_runThd) {
        //wait idle to open
        std::unique_lock<std::mutex> lck(m_mux);
        m_cvar.wait(lck, [&] { return m_requireSocketState == SocketState::open || !m_runThd; });

        if (!m_runThd) {
          break; //finish thread
        }

        try {
          m_socket.reset(shape_new zmq::socket_t(m_context, m_socketType));
          m_socket->bind(m_socketAdr);
          m_cvarReady.notify_all();

          while (true) {

            lck.unlock();

            //  wait in poll socket for a request, with timeout
            zmq::pollitem_t items[] = { { static_cast<void*>(*(m_socket.get())), 0, ZMQ_POLLIN, 0 } };
            zmq::poll(&items[0], 1, 2000); //min is 1000

            lck.lock();

            if (items[0].revents & ZMQ_POLLIN) {
              //  We got a request from a client
              zmq::message_t request;
              auto res = m_socket->recv(request);
                
              if (res) {
                if (m_onMessageFunc) {
                  m_onMessageFunc(std::string((char*)request.data(), (char*)request.data() + request.size()));
                }
              }
              
              m_sendMessage = false;
              //wait for async sent reply
              m_cvar.wait(lck, [&] { return m_sendMessage || m_requireSocketState == SocketState::close || !m_runThd; });

              if (m_sendMessage) {
                m_sendMessage = false;
                zmq::message_t reply(m_messageToSend.data(), m_messageToSend.data() + m_messageToSend.size());
                m_socket->send(reply, zmq::send_flags::none);
              }
            }

            if (m_requireSocketState == SocketState::close || !m_runThd) {
              m_socket->unbind(m_socketAdr);
              m_socket->close();
              m_cvarReady.notify_all();
              break; //go to wait to open again
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
      auto res = props->getMemberAsInt("replyWaitMillis", m_replyWaitMillis);
      if (res != shape::Properties::Result::ok) {
        TRC_INFORMATION("using default: " << PAR(m_replyWaitMillis));
      }

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

      switch (m_socketType) {

      case zmq::socket_type::rep:
      {
        m_thd = std::thread([&]() { workerRep(); });
        break;
      }

      case zmq::socket_type::req:
      {
        m_thd = std::thread([&]() { workerReq(); });
        break;
      }

      default:;
      }

      //open();

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

      //close();

      m_runThd = false;
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

  bool ZeroMqService::isOpen() const
  {
    return m_imp->isOpen();
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
