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
#include <string>
#include <memory>

#ifdef ICommandService_EXPORTS
#define ICommandService_DECLSPEC SHAPE_ABI_EXPORT
#else
#define ICommandService_DECLSPEC SHAPE_ABI_IMPORT
#endif

namespace shape {
  class ICommand
  {
  public:
    virtual std::string doCmd(std::istringstream & params) = 0;
    virtual std::string getHelp() = 0;
    virtual ~ICommand() {}
  };

  class ICommandService_DECLSPEC ICommandService
  {
  public:
    virtual void addCommand(const std::string & cmdStr, std::shared_ptr<ICommand> cmd) = 0;
    virtual void removeCommand(const std::string & cmdStr) = 0;
    virtual std::shared_ptr<ICommand> findCommand(const std::string & cmdStr) = 0;
    virtual std::shared_ptr<ICommand> getDefaultCommand() = 0;
    virtual ~ICommandService() {}
  };
}
