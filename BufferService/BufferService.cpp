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

#define IBufferService_EXPORTS

#include "BufferService.h"
#include "Trace.h"

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/pointer.h"

#include <queue>
#include <fstream>
#include <mutex>
#include <cmath>
#include <algorithm>

#include "shape__BufferService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::BufferService);

namespace shape {

  class BufferService::Imp
  {
  private:
    mutable std::mutex m_mux;
    
    ILaunchService* m_iLaunchService = nullptr;

    bool m_persistent = true;
    bool m_maxSize = 2^30; //GB
    int64_t m_maxDuration = 0;
    
    std::string m_instance;
    std::string m_cacheDir;
    std::string m_fname;

    std::queue<IBufferService::Record> m_queue;
    std::condition_variable m_cond;
    bool m_pushed;
    bool m_runWorker;
    std::thread m_worker;

    ProcessFunc m_processFunc;


  ///////////////////////////////
  public:
    Imp()
    {
    }

    ~Imp()
    {
      stop();
    }

    void registerProcessFunc(IBufferService::ProcessFunc func)
    {
      TRC_FUNCTION_ENTER("");
      std::unique_lock<std::mutex> lck(m_mux);
      if (!m_processFunc) {
        m_processFunc = func;
      }
      TRC_FUNCTION_LEAVE("");
    }

    void unregisterProcessFunc()
    {
      TRC_FUNCTION_ENTER("");
      m_processFunc = nullptr;
      TRC_FUNCTION_LEAVE("");
    }

    void start()
    {
      TRC_FUNCTION_ENTER("");
      m_pushed = false;
      m_runWorker = true;
      m_worker = std::thread([&]() { worker(); });
      TRC_FUNCTION_LEAVE("");
    }

    void stop()
    {
      TRC_FUNCTION_ENTER("");
      {
        std::unique_lock<std::mutex> lck(m_mux);
        m_runWorker = false;
        m_pushed = true;
      }
      m_cond.notify_all();

      if (m_worker.joinable())
        m_worker.join();

      TRC_FUNCTION_LEAVE("");
    }

    void dump()
    {
      //TODO
    }

    /// Worker thread function
    void worker()
    {
      TRC_FUNCTION_ENTER("");
      std::unique_lock<std::mutex> lck(m_mux, std::defer_lock);

      while (m_runWorker) {

        //wait for something in the queue
        lck.lock();
        m_cond.wait(lck, [&] { return m_pushed; }); //lock is released in wait
        //lock is reacquired here
        m_pushed = false;

        while (m_runWorker) {
          if (!m_queue.empty()) {
            auto rec = m_queue.front();
            m_queue.pop();
            lck.unlock();
            m_processFunc(rec);
          }
          else {
            lck.unlock();
            break;
          }
          lck.lock(); //lock for next iteration
        }
      }
      TRC_FUNCTION_LEAVE("");
    }


    bool empty()
    {
      TRC_FUNCTION_ENTER("");
      std::unique_lock<std::mutex> lck(m_mux);
      bool retval = m_queue.empty();
      TRC_FUNCTION_LEAVE("");
      return retval;
    }

    std::size_t size() const
    {
      TRC_FUNCTION_ENTER("");
      std::unique_lock<std::mutex> lck(m_mux);
      auto retval = m_queue.size();
      TRC_FUNCTION_LEAVE(PAR(retval));
      return retval;
    }

    IBufferService::Record front() const
    {
      TRC_FUNCTION_ENTER("");
      std::unique_lock<std::mutex> lck(m_mux);
      IBufferService::Record rec = m_queue.front();
      TRC_FUNCTION_LEAVE("");
      return rec;
    }

    IBufferService::Record back() const
    {
      TRC_FUNCTION_ENTER("");
      std::unique_lock<std::mutex> lck(m_mux);
      IBufferService::Record rec = m_queue.back();
      TRC_FUNCTION_LEAVE("");
      return rec;
    }

    void push(const IBufferService::Record & rec)
    {
      TRC_FUNCTION_ENTER("");
      std::unique_lock<std::mutex> lck(m_mux);
      m_queue.push(rec);
      
      //TODO check stale in front() and possibly pop it

      //int retval = 0;
      //{
      //  std::unique_lock<std::mutex> lck(m_mux);
      //  m_queue.push(rec);
      //  //retval = static_cast<uint8_t>(m_taskQueue.size());
      //  m_pushed = true;
      //}
      //m_cond.notify_all();
      //return retval;
      
      TRC_FUNCTION_LEAVE("")
    }

    void pop()
    {
      TRC_FUNCTION_ENTER("");
      std::unique_lock<std::mutex> lck(m_mux);
      m_queue.pop();
      TRC_FUNCTION_LEAVE("")
    }

    void load()
    {
      TRC_FUNCTION_ENTER("");
      if (m_persistent) {
        // load persistent data from file
        std::unique_lock<std::mutex> lck(m_mux);
        std::ifstream ifs;
        TRC_INFORMATION("Loading persistent buffer file: " << PAR(m_fname));
        ifs.open(m_fname, std::fstream::binary);
        if (!ifs.is_open()) {
          THROW_EXC_TRC_WAR(std::logic_error, "Cannot open persistent buffer file: " PAR(m_fname));
        }

        //TODO get length of file:
        //int flen = m_file.tellg();
        //m_file.seekg(0, m_file.beg);
        //if (flen < sizeof(size_t)) {
        //}

        // allocate buffer
        size_t rbufSz = 0xffff;
        std::unique_ptr<char[]> rbuf(shape_new char[rbufSz]);

        while (ifs)
        {
          IBufferService::Record rec;

          // read timestamp
          long long tst;
          ifs.read((char*)(&tst), sizeof(tst));
          rec.timestamp = tst;

          if (!ifs) {
            // eof, break here
            break;
          }

          // read address
          size_t sza;
          ifs.read((char*)(&sza), sizeof(sza));
         
          if (sza > rbufSz) {
            // reallocate buffer
            rbufSz = sza;
            rbuf.reset(shape_new char[rbufSz]);
          }

          ifs.read(rbuf.get(), sza);
          rec.address = std::string(rbuf.get(), sza);

          // read content
          size_t szc;
          ifs.read((char*)(&szc), sizeof(szc));
          if (szc > rbufSz) {
            // reallocate buffer
            rbufSz = szc;
            rbuf.reset(shape_new char[rbufSz]);
          }
          ifs.read(rbuf.get(), szc);
          rec.content = std::vector<uint8_t>((uint8_t*)rbuf.get(), (uint8_t*)rbuf.get() + szc);

          if (ifs.bad()) {
            THROW_EXC_TRC_WAR(std::logic_error, "Cannot write persistent buffer file: " PAR(m_fname));
          }

          
          m_queue.push(rec);
        }

        ifs.close();
      }
      TRC_FUNCTION_LEAVE("")
    }

    void save()
    {
      TRC_FUNCTION_ENTER("");

      if (m_persistent) {
        // store pushed data to file
        std::unique_lock<std::mutex> lck(m_mux);
        std::ofstream ofs;
        TRC_INFORMATION("Saving persistent buffer to file: " << PAR(m_fname));
        ofs.open(m_fname, std::fstream::trunc | std::fstream::binary);
        if (!ofs.is_open()) {
          THROW_EXC_TRC_WAR(std::logic_error, "Cannot open persistent buffer file: " PAR(m_fname));
        }
        
        while (!m_queue.empty())
        {
          const IBufferService::Record & rec = m_queue.front();
          ofs.write((const char*)(&rec.timestamp), sizeof(rec.timestamp));

          size_t sza = rec.address.size();
          ofs.write((const char*)(&sza), sizeof(sza));
          ofs.write(rec.address.data(), sza);

          size_t szc = rec.content.size();
          ofs.write((const char*)(&szc), sizeof(szc));
          ofs.write((const char*)rec.content.data(), szc);

          if (ofs.bad()) {
            THROW_EXC_TRC_WAR(std::logic_error, "Cannot write persistent buffer file: " PAR(m_fname));
          }

          m_queue.pop();
        }
        ofs.close();
      }

      TRC_FUNCTION_LEAVE("")
    }

    void modify(const shape::Properties *props)
    {
      using namespace rapidjson;

      const Document& doc = props->getAsJson();

      {
        const Value* v = Pointer("/instance").Get(doc);
        if (v && v->IsString()) {
          m_instance = v->GetString();
        }
      }

      {
        const Value* v = Pointer("/persistent").Get(doc);
        if (v && v->IsBool()) {
          m_persistent = v->GetBool();
        }
      }

      {
        const Value* v = Pointer("/maxSize").Get(doc);
        if (v && v->IsInt()) {
          m_maxSize = v->GetInt();
        }
      }

      {
        const Value* v = Pointer("/maxDuration").Get(doc);
        if (v && v->IsInt()) {
          m_maxDuration = v->GetInt();
        }
      }

      m_cacheDir = m_iLaunchService->getCacheDir();
      
      std::ostringstream os;
      os << m_cacheDir << '/';

      //mingle name as instance name allows any char not possible in file name
      for (auto c : m_instance) {
        if (c == '\\' || c == '/' || c == '<' || c == '>' || c == '|' || c == '\"' || c == ':' || c == '?' || c == '*') {
          // cannot be in file name => replace by num representation
          os << std::setfill('0') << std::hex << std::setw(2) << (int)c;
        }
        else {
          os << c;
        }
      }
      os << '_' << m_instance.size() << ".cache";
      m_fname = os.str();

    }

    void attachInterface(ILaunchService* iface)
    {
      m_iLaunchService = iface;
    }

    void detachInterface(ILaunchService* iface)
    {
      if (m_iLaunchService == iface) {
        m_iLaunchService = nullptr;
      }
    }

    void activate(const shape::Properties *props)
    {
      (void)props; //silence -Wunused-parameter
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "BufferService instance activate" << std::endl <<
        "******************************"
      );

      //TODO use parameters
      // maxSize
      // maxDuration
      modify(props);

      TRC_FUNCTION_LEAVE("")
    }

    void deactivate()
    {
      TRC_FUNCTION_ENTER("");
      TRC_INFORMATION(std::endl <<
        "******************************" << std::endl <<
        "BufferService instance deactivate" << std::endl <<
        "******************************"
      );

      TRC_FUNCTION_LEAVE("")
    }

  };

  ///////////////////////////////////////
  BufferService::BufferService()
  {
    m_imp = shape_new Imp();
  }

  BufferService::~BufferService()
  {
    delete m_imp;
  }

  void BufferService::registerProcessFunc(IBufferService::ProcessFunc func)
  {
    m_imp->registerProcessFunc(func);
  }

  void BufferService::unregisterProcessFunc()
  {
    m_imp->unregisterProcessFunc();
  }

  void BufferService::start()
  {
    m_imp->start();
  }
  
  void BufferService::stop()
  {
    m_imp->stop();
  }

  void BufferService::dump()
  {
    m_imp->dump();
  }

  bool BufferService::empty()
  {
    return m_imp->empty();
  }

  std::size_t BufferService::size() const
  {
    return m_imp->size();
  }

  IBufferService::Record BufferService::front() const
  {
    return m_imp->front();
  }

  IBufferService::Record BufferService::back() const
  {
    return m_imp->back();
  }

  void BufferService::push(const Record & rec)
  {
    m_imp->push(rec);
  }

  void BufferService::pop()
  {
    m_imp->pop();
  }

  void BufferService::load()
  {
    m_imp->load();
  }

  void BufferService::save()
  {
    m_imp->save();
  }
  
  void BufferService::activate(const shape::Properties *props)
  {
    m_imp->activate(props);
  }

  void BufferService::deactivate()
  {
    m_imp->deactivate();
  }

  void BufferService::modify(const shape::Properties *props)
  {
    m_imp->modify(props);
  }

  void BufferService::attachInterface(ILaunchService* iface)
  {
    m_imp->attachInterface(iface);
  }

  void BufferService::detachInterface(ILaunchService* iface)
  {
    m_imp->detachInterface(iface);
  }

  void BufferService::attachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().addTracerService(iface);
  }

  void BufferService::detachInterface(shape::ITraceService* iface)
  {
    shape::Tracer::get().removeTracerService(iface);
  }

}
