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

namespace shape {
  //FIFO buffer (queue)
  class IBufferService
  {
  public:
    //Test whether buffer is empty
    virtual bool empty() = 0;
    //Return size
    virtual std::size_t size() const = 0;
    //Access next element
    virtual std::string front() const = 0;
    //Access last element
    virtual std::string back() const = 0;
    //Insert element
    virtual void push(const std::string & str) = 0;
    //Remove next element
    virtual void pop() = 0;
    //persistent load
    virtual void load() = 0;
    //persistent save
    virtual void save() = 0;
  };
}
