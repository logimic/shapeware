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

#include "LogStream.h"
#include "Trace.h"

namespace shape {
  int LogStream::overflow(int ch)
  {
    buffer.push_back((char)ch);
    if (ch == '\n') {
      //trace as called by TRC_INFORMATION("Websocketpp: " << buffer) but override function name as "overflow" seems like an error ;
      if (shape::Tracer::get().isValid((int)shape::TraceLevel::Information, TRC_CHANNEL)) {
        std::ostringstream _ostrmsg;
        _ostrmsg << "Websocketpp: " << buffer << std::endl;
        shape::Tracer::get().writeMsg((int)shape::TraceLevel::Information, TRC_CHANNEL, TRC_MNAME, __FILE__, __LINE__, "Websocketpp log override", _ostrmsg.str());
      }
      buffer.clear();
    }
    return ch;
  }
}
