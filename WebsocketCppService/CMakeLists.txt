project(WebsocketCppService)

#workaround to cope with c1128 number of sections exceeded object file format limit
#if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
#  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /bigobj")
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
#endif ()

if(${WS_WITHOUT_TLS})
  add_definitions(-DWS_WITHOUT_TLS)
else()
  find_package(OpenSSL REQUIRED)
endif()

set(COMPONENT shape::WebsocketCppService)
DeclareShapeComponent(${COMPONENT})
AddShapeProvidedInterface(${COMPONENT} shape::IWebsocketService)
AddShapeRequiredInterface(${COMPONENT} shape::ILaunchService MANDATORY SINGLE)
AddShapeRequiredInterface(${COMPONENT} shape::ITraceService MANDATORY MULTIPLE)
ConfigureShapeComponent(${COMPONENT} COMPONENT_HXX)

file(GLOB_RECURSE _HDRFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h  ${COMPONENT_HXX})
file(GLOB_RECURSE _SRCFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

source_group("Header Files" FILES ${_HDRFILES})
source_group("Source Files" FILES ${_SRCFILES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories (${CMAKE_SOURCE_DIR}/external/websocketpp)
include_directories (${CMAKE_SOURCE_DIR}/external/asio/asio/include)

if(SHAPE_STATIC_LIBS)
    add_library(${PROJECT_NAME} STATIC ${_HDRFILES} ${_SRCFILES})
else()
    add_library(${PROJECT_NAME} SHARED ${_HDRFILES} ${_SRCFILES})
endif()

if(${WS_WITHOUT_TLS})
  message(STATUS "Building without TLS")
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

DeployShapeComponent(${PROJECT_NAME})
