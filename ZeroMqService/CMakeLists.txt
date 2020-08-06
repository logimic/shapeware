project(ZeroMqService)

set(COMPONENT shape::ZeroMqService)
DeclareShapeComponent(${COMPONENT})
AddShapeProvidedInterface(${COMPONENT} shape::IZeroMqService)
AddShapeRequiredInterface(${COMPONENT} shape::ITraceService MANDATORY MULTIPLE)
ConfigureShapeComponent(${COMPONENT} COMPONENT_HXX)

if(WIN32) 
    find_package(ZeroMQ CONFIG REQUIRED)
else()
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")
    find_package(ZeroMQ REQUIRED)
endif()

#find_path(CPPZMQ_INCLUDE_DIR NAMES zmq.hpp)

file(GLOB_RECURSE _HDRFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h  ${COMPONENT_HXX})
file(GLOB_RECURSE _SRCFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

source_group("Header Files" FILES ${_HDRFILES})
source_group("Source Files" FILES ${_SRCFILES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories (${CMAKE_SOURCE_DIR}/external/cppzmq)

if(SHAPE_STATIC_LIBS)
    add_library(${PROJECT_NAME} STATIC ${_HDRFILES} ${_SRCFILES})
else()
    add_library(${PROJECT_NAME} SHARED ${_HDRFILES} ${_SRCFILES})
endif()

if(WIN32) 
    target_link_libraries(${PROJECT_NAME} PRIVATE libzmq)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE ${ZeroMQ_LIBRARY})
endif()

DeployShapeComponent(${PROJECT_NAME})
