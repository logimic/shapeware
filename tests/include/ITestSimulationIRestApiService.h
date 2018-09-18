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
#include "IRestApiService.h"
#include <string>

#ifdef ITestSimulationIRestApiService_EXPORTS
#define ITestSimulationIRestApiService_DECLSPEC SHAPE_ABI_EXPORT
#else
#define ITestSimulationIRestApiService_DECLSPEC SHAPE_ABI_IMPORT
#endif

namespace shape {

  class ITestSimulationIRestApiService_DECLSPEC ITestSimulationIRestApiService : public IRestApiService
  {
  public:
    virtual std::string popIncomingRequest(unsigned millisToWait) = 0;
    virtual void setResourceDirectory(const std::string& dir) = 0;
    virtual ~ITestSimulationIRestApiService() {};
  };
}
