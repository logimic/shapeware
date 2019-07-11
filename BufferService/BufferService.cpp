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
#include <queue>
#include <fstream>

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
    std::mutex m_mux;
    
    std::string m_fname = "buffer.txt";
    std::fstream m_file;

    std::queue<std::string> m_queue;
    bool m_persistent = false;
    bool m_size = 1024;
    int64_t m_duration = 0;


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
      bool retval = m_queue.empty();
      TRC_FUNCTION_LEAVE("");
      return retval;
    }

    std::size_t size() const
    {
      TRC_FUNCTION_ENTER("");
      auto retval = m_queue.size();
      TRC_FUNCTION_LEAVE(PAR(retval));
      return retval;
    }

    const std::string & front() const
    {
      TRC_FUNCTION_ENTER("");
      const std::string & str = m_queue.front();
      TRC_FUNCTION_LEAVE("");
      return str;
    }

    const std::string & back() const
    {
      TRC_FUNCTION_ENTER("");
      const std::string & str = m_queue.back();
      TRC_FUNCTION_LEAVE("");
      return str;
    }

    void push(const std::string & str)
    {
      TRC_FUNCTION_ENTER("");
      m_queue.push(str);
      TRC_FUNCTION_LEAVE("")
    }

    void pop()
    {
      TRC_FUNCTION_ENTER("");
      m_queue.pop();
      TRC_FUNCTION_LEAVE("")
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

      //TODO parameters
      // persistent
      // max size
      // max duration

      if (m_persistent) {
        TRC_INFORMATION("Loading persistent buffer file: " << PAR(m_fname));
        m_file.open(m_fname, std::fstream::in | std::fstream::out | std::fstream::app);
        if (!m_file.is_open()) {
          THROW_EXC_TRC_WAR(std::logic_error, "Cannot open persistent buffer file: " PAR(m_fname));
        }
      }

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

  const std::string & BufferService::front() const
  {
    return m_imp->front();
  }

  const std::string & BufferService::back() const
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
    (void)props; //silence -Wunused-parameter
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
