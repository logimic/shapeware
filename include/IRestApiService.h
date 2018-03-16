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
    typedef std::function<void(const std::string & data)> DataHandlerFunc;

    virtual void registerDataHandler(DataHandlerFunc hndl) = 0;
    virtual void unregisterDataHandler() = 0;
    virtual void getData(const std::string & url) = 0;
    virtual ~IRestApiService() {};
  };
}
