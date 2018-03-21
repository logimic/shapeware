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

#pragma once

#include "ShapeDefines.h"
#include <vector>
#include <string>
#include <functional>

#ifdef IWebsocketService_EXPORTS
#define IWebsocket_DECLSPEC SHAPE_ABI_EXPORT
#else
#define IWebsocket_DECLSPEC SHAPE_ABI_IMPORT
#endif

namespace shape {
  class IWebsocket_DECLSPEC IWebsocketService
  {
  public:
    /// Incoming message handler functional type
    typedef std::function<void(const std::vector<uint8_t> &)> MessageHandlerFunc;

    /// \brief Register message handler
    /// \param [in] hndl registering handler function
    /// \details
    /// Whenever a message is received it is passed to the handler function. It is possible to register 
    /// just one handler
    virtual void registerMessageHandler(MessageHandlerFunc hndl) = 0;

    /// \brief Unregister message handler
    /// \details
    /// If the handler is not required anymore, it is possible to unregister via this method.
    virtual void unregisterMessageHandler() = 0;

    /// \brief send message
    /// \param [in] msg message to be sent 
    /// \details
    /// The message is send outside
    virtual void sendMessage(const std::vector<uint8_t> & msg) = 0;

    inline virtual ~IWebsocketService() {};

    //virtual void run() = 0;
    //virtual void send(const std::string& msg) = 0;
    //// remote message
    //typedef std::function<void(const std::string& msg)> MessageHandlerFunc;
    //virtual void registerMessageHandler(MessageHandlerFunc messageHandlerFunc) = 0;
    //virtual ~IWebsocketService() {}
  };
}
