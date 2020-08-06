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

#pragma once

#include "ShapeDefines.h"
#include <string>
#include <vector>
#include <functional>

namespace shape {
  //FIFO buffer (queue)
  class IBufferService
  {
  public:
    class Record {
    public:
      Record()
      {}
      virtual ~Record()
      {}
      Record(const std::string & adr, const std::vector<uint8_t> & cont, long long tst = 0)
        : timestamp(tst)
        , address(adr)
        , content(cont)
      {}
      Record(const std::string & adr, const std::string & cont, long long tst = 0)
        : timestamp(tst)
        , address(adr)
        , content((uint8_t*)cont.data(), (uint8_t*)cont.data() + cont.size())
      {}
      long long timestamp = 0;
      std::string address;
      std::vector<uint8_t> content;
    };

    typedef std::function<bool(const Record &)> ProcessFunc;
    
    virtual void registerProcessFunc(ProcessFunc func) = 0;
    virtual void unregisterProcessFunc() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

    virtual void suspend() = 0;
    virtual void recover() = 0;

    //Test whether buffer is empty
    virtual bool empty() = 0;
    //Return size
    virtual std::size_t size() const = 0;
    //Access next element
    virtual Record front() const = 0;
    //Access last element
    virtual Record back() const = 0;
    //Insert element
    virtual void push(const Record & rec) = 0;
    //Remove next element
    virtual void pop() = 0;
    //persistent load
    virtual void load() = 0;
    //persistent save
    virtual void save() = 0;
  };
}
