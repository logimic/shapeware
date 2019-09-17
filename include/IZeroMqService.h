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
#include <vector>
#include <string>
#include <functional>

namespace shape {
  class IZeroMqService
  {
  public:
    /// Incoming message handler functional type
    typedef std::function<void(const std::string&)> OnMessageFunc;
    typedef std::function<void()> OnReqTimeoutFunc;

    virtual void registerOnMessage(OnMessageFunc fc) = 0;
    virtual void registerOnReqTimeout(OnReqTimeoutFunc fc) = 0;

    /// \brief Unregister message handler
    /// \details
    /// If the handler is not required anymore, it is possible to unregister via this method.
    virtual void unregisterOnMessage() = 0;
    virtual void unregisterOnDisconnect() = 0;

    /// \brief send message
    /// \param [in] msg message to be sent 
    /// \details
    /// The message is send outside
    virtual void sendMessage(const std::string& msg) = 0;
    
    virtual void open() = 0;
    virtual void close() = 0;
    virtual bool isConnected() const = 0;

    inline virtual ~IZeroMqService() {};

  };
}
