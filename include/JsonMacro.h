#pragma once

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/pointer.h"

#define GET_JSON_AS_NUM(jsonVal, pointerPath, value) \
{ \
  const rapidjson::Value *pv = rapidjson::Pointer(pointerPath).Get(jsonVal); \
  if (!(pv && pv->IsNumber())) { \
    THROW_EXC_TRC_WAR(std::logic_error, "Missing or bad type: \"" << pointerPath << "\"") \
  } \
  value = pv->GetDouble(); \
}

#define GET_JSON_AS_INT(jsonVal, pointerPath, value) \
{ \
  const rapidjson::Value *pv = rapidjson::Pointer(pointerPath).Get(jsonVal); \
  if (!(pv && pv->IsNumber())) { \
    THROW_EXC_TRC_WAR(std::logic_error, "Missing or bad type: \"" << pointerPath << "\"") \
  } \
  value = static_cast<int>(pv->GetDouble()); \
}

#define GET_JSON_AS_UINT(jsonVal, pointerPath, value) \
{ \
  const rapidjson::Value *pv = rapidjson::Pointer(pointerPath).Get(jsonVal); \
  if (!(pv && pv->IsUint())) { \
    THROW_EXC_TRC_WAR(std::logic_error, "Missing or bad type: \"" << pointerPath << "\"") \
  } \
  value = pv->GetUint(); \
}

#define GET_JSON_AS_BOOL(jsonVal, pointerPath, value) \
{ \
  const rapidjson::Value *pv = rapidjson::Pointer(pointerPath).Get(jsonVal); \
  if (!(pv && pv->IsBool())) { \
    THROW_EXC_TRC_WAR(std::logic_error, "Missing or bad type: \"" << pointerPath << "\"") \
  } \
  value = pv->GetBool(); \
}

#define GET_JSON_AS_STR(jsonVal, pointerPath, value) \
{ \
  const rapidjson::Value *pv = rapidjson::Pointer(pointerPath).Get(jsonVal); \
  if (!(pv && pv->IsString())) { \
    THROW_EXC_TRC_WAR(std::logic_error, "Missing or bad type: \"" << pointerPath << "\"") \
  } \
  value = pv->GetString(); \
}

#define GET_JSON_AS_OBJECT(jsonVal, pointerPath, value) \
{ \
  const rapidjson::Value *pv = rapidjson::Pointer(pointerPath).Get(jsonVal); \
  if (!(pv && pv->IsObject())) { \
    THROW_EXC_TRC_WAR(std::logic_error, "Missing or not object: \"" << pointerPath << "\"") \
  } \
  value = pv; \
}

#define GET_JSON_AS_ARRAY(jsonVal, pointerPath, value) \
{ \
  const rapidjson::Value *pv = rapidjson::Pointer(pointerPath).Get(jsonVal); \
  if (!(pv && pv->IsArray())) { \
    THROW_EXC_TRC_WAR(std::logic_error, "Missing or not array: \"" << pointerPath << "\"") \
  } \
  value = pv; \
}

