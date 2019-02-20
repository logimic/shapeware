#define IMessageService_EXPORTS

#include "MqMessageService.h"
#include "TaskQueue.h"
#include "Trace.h"

#ifdef SHAPE_PLATFORM_WINDOWS
#include <windows.h>
typedef HANDLE MQDESCR;
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
typedef mqd_t MQDESCR;
#endif

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(iqrf::MqMessageService);

#include "shape__MqMessageService.hxx"

const unsigned IQRF_MQ_BUFFER_SIZE = 64 * 1024;

#ifndef SHAPE_PLATFORM_WINDOWS
#define GetLastError() errno
#include <string.h>
const int INVALID_HANDLE_VALUE = -1;
#define QUEUE_PERMISSIONS 0644
#define MAX_MESSAGES 32

const std::string MQ_PREFIX("/");

inline MQDESCR openMqRead(const std::string name, unsigned bufsize)
{
  TRC_FUNCTION_ENTER(PAR(name) << PAR(bufsize))
    mqd_t desc;

  struct mq_attr req_attr;

  req_attr.mq_flags = 0;
  req_attr.mq_maxmsg = MAX_MESSAGES;
  req_attr.mq_msgsize = bufsize / MAX_MESSAGES;
  req_attr.mq_curmsgs = 0;

  TRC_DEBUG("required attributes" << PAR(req_attr.mq_maxmsg) << PAR(req_attr.mq_msgsize))
    desc = mq_open(name.c_str(), O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &req_attr);

  if (desc > 0) {

    struct mq_attr act_attr;
    int res = mq_getattr(desc, &act_attr);
    if (res == 0) {
      TRC_DEBUG("actual attributes: " << PAR(res) << PAR(act_attr.mq_maxmsg) << PAR(act_attr.mq_msgsize))

        if (act_attr.mq_maxmsg != req_attr.mq_maxmsg || act_attr.mq_msgsize != req_attr.mq_msgsize) {
          res = mq_unlink(name.c_str());
          if (res == 0 || errno == ENOENT) {
            desc = mq_open(name.c_str(), O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &req_attr);
            if (desc < 0) {
              TRC_WARNING("mq_open() after mq_unlink() failed:" << PAR(name) << PAR(desc))
            }
          }
          else {
            TRC_WARNING("mq_unlink() failed:" << PAR(name) << PAR(desc))
          }
        }
    }
    else {
      TRC_WARNING("mq_getattr() failed:" << PAR(name) << PAR(res))
    }
  }
  else {
    TRC_WARNING("mq_open() failed:" << PAR(name) << PAR(desc))
  }

  TRC_FUNCTION_LEAVE(PAR(desc));
  return desc;
}

inline MQDESCR openMqWrite(const std::string name, unsigned bufsize)
{
  TRC_FUNCTION_ENTER(PAR(name))

    struct mq_attr attr;

  attr.mq_flags = 0;
  attr.mq_maxmsg = MAX_MESSAGES;
  attr.mq_msgsize = bufsize / MAX_MESSAGES;
  attr.mq_curmsgs = 0;

  TRC_DEBUG("explicit attributes" << PAR(attr.mq_maxmsg) << PAR(attr.mq_msgsize))
    mqd_t retval = mq_open(name.c_str(), O_WRONLY);

  if (retval > 0) {
    struct mq_attr nwattr;
    int nwretval = mq_getattr(retval, &nwattr);
    TRC_DEBUG("set attributes" << PAR(nwretval) << PAR(nwattr.mq_maxmsg) << PAR(nwattr.mq_msgsize))
  }

  TRC_FUNCTION_LEAVE(PAR(retval));
  return retval;
}

inline void closeMq(MQDESCR mqDescr)
{
  mq_close(mqDescr);
}

inline bool readMq(MQDESCR mqDescr, unsigned char* rx, unsigned long bufSize, unsigned long& numOfBytes)
{
  bool ret = true;

  ssize_t numBytes = mq_receive(mqDescr, (char*)rx, bufSize, NULL);

  if (numBytes <= 0) {
    ret = false;
    numOfBytes = 0;
  }
  else
    numOfBytes = numBytes;
  return ret;
}

inline bool writeMq(MQDESCR mqDescr, const unsigned char* tx, unsigned long toWrite, unsigned long& written)
{
  TRC_FUNCTION_ENTER(PAR(toWrite))

    written = toWrite;
  int res = mq_send(mqDescr, (const char*)tx, toWrite, 0);
  bool retval = res == 0;

  TRC_FUNCTION_LEAVE(PAR(retval))
    return retval;
}

#else

const std::string MQ_PREFIX("\\\\.\\pipe\\");

inline MQDESCR openMqRead(const std::string name, unsigned bufsize)
{
  return CreateNamedPipe(name.c_str(), PIPE_ACCESS_INBOUND,
    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
    PIPE_UNLIMITED_INSTANCES, bufsize, bufsize, 0, NULL);
}

inline MQDESCR openMqWrite(const std::string name, unsigned bufsize)
{
  return CreateFile(name.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
}

inline void closeMq(MQDESCR mqDescr)
{
  CloseHandle(mqDescr);
}

inline bool readMq(MQDESCR mqDescr, unsigned char* rx, unsigned long bufSize, unsigned long& numOfBytes)
{
  return ReadFile(mqDescr, rx, bufSize, &numOfBytes, NULL);
}

inline bool writeMq(MQDESCR mqDescr, const unsigned char* tx, unsigned long toWrite, unsigned long& written)
{
  return WriteFile(mqDescr, tx, toWrite, &written, NULL);
}

#endif

namespace shape {
  class MqMessageService::Imp
  {
  private:
    TaskQueue<std::vector<uint8_t>>* m_toMqMessageQueue = nullptr;

    std::string m_localMqName = "iqrf-daemon-100";
    std::string m_remoteMqName = "iqrf-daemon-110";

    IMessageService::MessageHandlerFunc m_messageHandlerFunc;

    std::atomic_bool m_connected;
    bool m_runListenThread = true;
    std::thread m_listenThread;
    std::mutex m_connectMtx;

    MQDESCR m_localMqHandle = INVALID_HANDLE_VALUE;
    MQDESCR m_remoteMqHandle = INVALID_HANDLE_VALUE;

    unsigned char* m_rx;
    unsigned m_bufsize = IQRF_MQ_BUFFER_SIZE;
    bool m_server = false;
    bool m_state = false;

  public:
    Imp()
    {
    }

    ~Imp()
    {
    }

    void registerMessageHandler(MessageHandlerFunc hndl)
    {
      TRC_FUNCTION_ENTER("");
      m_messageHandlerFunc = hndl;
      TRC_FUNCTION_LEAVE("")
    }

    void unregisterMessageHandler()
    {
      TRC_FUNCTION_ENTER("");
      m_messageHandlerFunc = IMessageService::MessageHandlerFunc();
      TRC_FUNCTION_LEAVE("")
    }

    void sendMessage(const std::vector<uint8_t> & msg)
    {
      TRC_FUNCTION_ENTER("");
      TRC_DEBUG(MEM_HEX_CHAR(msg.data(), msg.size()));
      m_toMqMessageQueue->pushToQueue(msg);
      TRC_FUNCTION_LEAVE("")
    }

    void start()
    {
      TRC_FUNCTION_ENTER("");
      TRC_FUNCTION_LEAVE("")
    }

    void stop()
    {
      TRC_FUNCTION_ENTER("");
      TRC_FUNCTION_LEAVE("")
    }

    bool isReady() const
    {
      return m_state;
    }

    void activate(const shape::Properties *props)
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "MqMessageService instance activate" << std::endl <<
        "******************************"
      );

      props->getMemberAsString("LocalMqName", m_localMqName);
      props->getMemberAsString("RemoteMqName", m_remoteMqName);

      m_connected = false;
      m_rx = shape_new unsigned char[m_bufsize];
      memset(m_rx, 0, m_bufsize);

      m_localMqName = MQ_PREFIX + m_localMqName;
      m_remoteMqName = MQ_PREFIX + m_remoteMqName;

      TRC_INFORMATION(PAR(m_localMqName) << PAR(m_remoteMqName));

      m_listenThread = std::thread(&MqMessageService::Imp::listen, this);

      m_toMqMessageQueue = shape_new TaskQueue<std::vector<uint8_t>>([&](const std::vector<uint8_t>& msg) {
        sendTo(msg);
      });

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");

      TRC_DEBUG("joining Mq listening thread");
      m_runListenThread = false;
#ifndef SHAPE_PLATFORM_WINDOWS
      //seems the only way to stop the thread here
      pthread_cancel(m_listenThread.native_handle());
      closeMq(m_remoteMqHandle);
      closeMq(m_localMqHandle);
#else
      //seems the only way to stop the thread here
      TerminateThread(m_listenThread.native_handle(), 0);
      closeMq(m_remoteMqHandle);
      closeMq(m_localMqHandle);
#endif

      if (m_listenThread.joinable())
        m_listenThread.join();
      TRC_DEBUG("listening thread joined");

      delete[] m_rx;

      delete m_toMqMessageQueue;

      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "MqMessageService instance deactivate" << std::endl <<
        "******************************"
      );
      TRC_FUNCTION_LEAVE("")
    }

    void modify(const shape::Properties *props)
    {
      (void)props; //silence -Wunused-parameter
    }

  private:
    int handleMessageFromMq(const std::vector<uint8_t> & msg)
    {
      TRC_DEBUG("==================================" << std::endl <<
        "Received from MQ: " << std::endl << MEM_HEX_CHAR(msg.data(), msg.size()));

      if (m_messageHandlerFunc)
        m_messageHandlerFunc(msg);

      return 0;
    }

    void listen()
    {
      TRC_FUNCTION_ENTER("thread starts");

      try {
        while (m_runListenThread) {

          unsigned long cbBytesRead = 0;
          bool fSuccess(false);

          m_localMqHandle = openMqRead(m_localMqName, m_bufsize);
          if (m_localMqHandle == INVALID_HANDLE_VALUE) {
            THROW_EXC_TRC_WAR(std::logic_error, "openMqRead() failed: " << NAME_PAR(GetLastError, GetLastError()));
          }
          TRC_INFORMATION("openMqRead() opened: " << PAR(m_localMqName));

#ifdef SHAPE_PLATFORM_WINDOWS
          // Wait to connect from cient
          fSuccess = ConnectNamedPipe(m_localMqHandle, NULL);
          if (!fSuccess) {
            THROW_EXC_TRC_WAR(std::logic_error, "ConnectNamedPipe() failed: " << NAME_PAR(GetLastError, GetLastError()));
          }
          TRC_DEBUG("ConnectNamedPipe() connected: " << PAR(m_localMqName));
#endif

          // Loop for reading
          while (m_runListenThread) {
            m_state = true;
            cbBytesRead = 0;
            fSuccess = readMq(m_localMqHandle, m_rx, m_bufsize, cbBytesRead);
            if (!fSuccess || cbBytesRead == 0) {
              if (m_server) { // listen again
                closeMq(m_localMqHandle);
                m_connected = false; // connect again
                TRC_ERROR("readMq() failed: " << NAME_PAR(GetLastError, GetLastError()));
                break;
              }
              else {
                std::string brokenMsg("Remote broken");
                sendTo(std::vector<uint8_t>((const uint8_t*)brokenMsg.data(), (const uint8_t*)brokenMsg.data() + brokenMsg.size()));
                THROW_EXC_TRC_WAR(std::logic_error, "readMq() failed: " << NAME_PAR(GetLastError, GetLastError()));
              }
            }

            if (m_messageHandlerFunc) {
              std::vector<uint8_t> message(m_rx, m_rx + cbBytesRead);
              m_messageHandlerFunc(message);
            }
            else {
              TRC_WARNING("Message handler not registered");
            }
          }
        }
      }
      catch (std::logic_error& e) {
        CATCH_EXC_TRC_WAR(MqChannelException, e, "listening thread finished");
        m_runListenThread = false;
      }
      catch (std::exception& e) {
        CATCH_EXC_TRC_WAR(std::exception, e, "listening thread finished");
        m_runListenThread = false;
      }
      m_state = false;
      TRC_FUNCTION_LEAVE("thread stopped");
    }

    void connect()
    {
      if (!m_connected) {

        std::lock_guard<std::mutex> lck(m_connectMtx);

        closeMq(m_remoteMqHandle);

        // Open write channel to client
        m_remoteMqHandle = openMqWrite(m_remoteMqName, m_bufsize);
        if (m_remoteMqHandle == INVALID_HANDLE_VALUE) {
          TRC_WARNING("openMqWrite() failed: " << NAME_PAR(GetLastError, GetLastError()));
          //if (GetLastError() != ERROR_PIPE_BUSY)
        }
        else {
          TRC_INFORMATION("openMqWrite() opened: " << PAR(m_remoteMqName));
          m_connected = true;
        }
      }
    }

    void sendTo(const std::vector<uint8_t>& message)
    {
      TRC_INFORMATION("Send to MQ: " << std::endl << MEM_HEX_CHAR(message.data(), message.size()));

      unsigned long toWrite = static_cast<unsigned long>(message.size());
      unsigned long written = 0;
      //bool reconnect = false;
      bool fSuccess;

      connect(); //open write channel if not connected yet

      fSuccess = writeMq(m_remoteMqHandle, message.data(), toWrite, written);
      if (!fSuccess || toWrite != written) {
        TRC_WARNING("writeMq() failed: " << NAME_PAR(GetLastError, GetLastError()));
        m_connected = false;
      }
    }

  };

  ///////////////////////////////////////
  MqMessageService::MqMessageService()
  {
    m_imp = shape_new Imp();
  }

  MqMessageService::~MqMessageService()
  {
    delete m_imp;
  }

  void MqMessageService::registerMessageHandler(MessageHandlerFunc hndl)
  {
    m_imp->registerMessageHandler(hndl);
  }

  void MqMessageService::unregisterMessageHandler()
  {
    m_imp->unregisterMessageHandler();
  }

  void MqMessageService::sendMessage(const std::vector<uint8_t> & msg)
  {
    m_imp->sendMessage(msg);
  }

  void MqMessageService::start()
  {
    m_imp->start();
  }

  void MqMessageService::stop()
  {
    m_imp->stop();
  }

  bool MqMessageService::isReady() const
  {
    return m_imp->isReady();
  }

  void MqMessageService::activate(const shape::Properties *props)
  {
    m_imp->activate(props);
  }

  void MqMessageService::deactivate()
  {
    m_imp->deactivate();
  }

  void MqMessageService::modify(const shape::Properties *props)
  {
    m_imp->modify(props);
  }

  void MqMessageService::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void MqMessageService::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

}
