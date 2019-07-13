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

#include "shape__BufferService.hxx"

#ifdef TRC_CHANNEL
#undef TRC_CHANNEL
#endif
#define TRC_CHANNEL 0

TRC_INIT_MODULE(shape::BufferService);

namespace shape {
  //class Record {
  //  Record(const std::string & str)

  //  {}
  //private:
  //  const std::string & str
  //};

  class BufferService::Imp
  {
  private:
    mutable std::mutex m_mux;
    
    ILaunchService* m_iLaunchService = nullptr;

    std::queue<std::string> m_queue;

    bool m_persistent = true;
    bool m_maxSize = 2^30; //GB
    int64_t m_maxDuration = 0;
    
    std::string m_instance;
    std::string m_cacheDir;
    std::string m_fname;


  ///////////////////////////////
  public:
    Imp()
    {
    }

    ~Imp()
    {
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

    std::string front() const
    {
      TRC_FUNCTION_ENTER("");
      std::unique_lock<std::mutex> lck(m_mux);
      std::string str = m_queue.front();
      TRC_FUNCTION_LEAVE("");
      return str;
    }

    std::string back() const
    {
      TRC_FUNCTION_ENTER("");
      std::unique_lock<std::mutex> lck(m_mux);
      std::string str = m_queue.back();
      TRC_FUNCTION_LEAVE("");
      return str;
    }

    void push(const std::string & str)
    {
      TRC_FUNCTION_ENTER("");
      std::unique_lock<std::mutex> lck(m_mux);
      m_queue.push(str);
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

        //// get length of file:
        //int flen = m_file.tellg();
        //m_file.seekg(0, m_file.beg);

        //if (flen < sizeof(size_t)) {

        //}

        // allocate buffer
        size_t rbufSz = 0xffff;
        std::unique_ptr<char[]> rbuf(shape_new char[rbufSz]);

        while (ifs)
        {
          size_t sz;
          ifs.read((char*)(&sz), sizeof(sz));
          if (!ifs) {
            break;
          }
          if (ifs.bad()) {
            THROW_EXC_TRC_WAR(std::logic_error, "Cannot read persistent buffer file: " PAR(m_fname));
          }

          if (sz > rbufSz) {
            // reallocate buffer
            rbufSz = sz;
            rbuf.reset(shape_new char[rbufSz]);
          }

          ifs.read(rbuf.get(), sz);
          if (!ifs) {
            break;
          }
          if (ifs.bad()) {
            THROW_EXC_TRC_WAR(std::logic_error, "Cannot write persistent buffer file: " PAR(m_fname));
          }

          m_queue.push(std::string(rbuf.get(), sz));
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
          const std::string & str = m_queue.front();
          size_t sz = str.size();
          ofs.write((const char*)(&sz), sizeof(sz));
          if (ofs.bad()) {
            THROW_EXC_TRC_WAR(std::logic_error, "Cannot write persistent buffer file: " PAR(m_fname));
          }
          ofs.write(str.data(), str.size());
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
      
      //mingle name as instance name allows any char not possible in file name
      m_fname = m_instance;
      // hash name
      unsigned hash = 0;
      for (size_t i = 0; i < m_instance.length(); ++i) {
        hash += (unsigned)(m_instance[i] * pow(31, i));
      }
      // replace not allowed chars
      std::string toreplace("\/<>|\":?*");
      for (auto c : toreplace) {
        std::replace(m_instance.begin(), m_instance.end(), c, '_');
      }
      //mingle as combination of original instance name and its hash
      std::ostringstream os;
      os << m_cacheDir << '/' << m_instance << '_' << hash << ".cache";
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

  bool BufferService::empty()
  {
    return m_imp->empty();
  }

  std::size_t BufferService::size() const
  {
    return m_imp->size();
  }

  std::string BufferService::front() const
  {
    return m_imp->front();
  }

  std::string BufferService::back() const
  {
    return m_imp->back();
  }

  void BufferService::push(const std::string & str)
  {
    m_imp->push(str);
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
