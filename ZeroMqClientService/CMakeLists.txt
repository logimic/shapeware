project(ZeroMqClientService)

set(COMPONENT shape::ZeroMqClientService)
DeclareShapeComponent(${COMPONENT})
AddShapeProvidedInterface(${COMPONENT} shape::IZeroMqClientService)
AddShapeRequiredInterface(${COMPONENT} shape::ITraceService MANDATORY MULTIPLE)
ConfigureShapeComponent(${COMPONENT} COMPONENT_HXX)

find_package(ZeroMQ CONFIG REQUIRED)
find_path(CPPZMQ_INCLUDE_DIR NAMES zmq.hpp)

file(GLOB_RECURSE _HDRFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h  ${COMPONENT_HXX})
file(GLOB_RECURSE _SRCFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

source_group("Header Files" FILES ${_HDRFILES})
source_group("Source Files" FILES ${_SRCFILES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CPPZMQ_INCLUDE_DIR})
message(STATUS "ZeroMQ_STATIC_LIBRARY: ${ZeroMQ_STATIC_LIBRARY}")

if(SHAPE_STATIC_LIBS)
    add_library(${PROJECT_NAME} STATIC ${_HDRFILES} ${_SRCFILES})
else()
    add_library(${PROJECT_NAME} SHARED ${_HDRFILES} ${_SRCFILES})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE libzmq)

DeployShapeComponent(${PROJECT_NAME})
