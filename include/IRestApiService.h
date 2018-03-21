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

#ifdef IRestApiService_EXPORTS
#define IRestApiService_DECLSPEC SHAPE_ABI_EXPORT
#else
#define IRestApiService_DECLSPEC SHAPE_ABI_IMPORT
#endif

namespace shape {
  class IRestApiService_DECLSPEC IRestApiService
  {
  public:
    typedef std::function<void(int statusCode, const std::string & data)> DataHandlerFunc;

    virtual void registerDataHandler(DataHandlerFunc hndl) = 0;
    virtual void unregisterDataHandler() = 0;
    virtual void getData(const std::string & url) = 0;
    virtual ~IRestApiService() {};
  };
}
