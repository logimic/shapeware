project(CppRestService)

if (UNIX)
# Use custom Findcpprestsdk.cmake path
  set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
  find_package(Boost REQUIRED)
  find_package(cpprestsdk REQUIRED)
else()
  message(STATUS "cpprestsdk find process controlled by vcpkg on Win")
endif()

message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

find_package(Boost REQUIRED)
find_package(cpprestsdk REQUIRED)

set(COMPONENT shape::CppRestService)
DeclareShapeComponent(${COMPONENT})
AddShapeProvidedInterface(${COMPONENT} shape::IRestApiService)
AddShapeRequiredInterface(${COMPONENT} shape::ITraceService MANDATORY MULTIPLE)
ConfigureShapeComponent(${COMPONENT} COMPONENT_HXX)

file(GLOB_RECURSE _HDRFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h  ${COMPONENT_HXX})
file(GLOB_RECURSE _SRCFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

source_group("Header Files" FILES ${_HDRFILES})
source_group("Source Files" FILES ${_SRCFILES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories (${LIBWEBSOCKETS_INCLUDE_DIRS})

if(SHAPE_STATIC_LIBS)
    add_library(${PROJECT_NAME} STATIC ${_HDRFILES} ${_SRCFILES})
else()
    #add_library(${PROJECT_NAME} SHARED ${_HDRFILES} ${_SRCFILES})
	add_executable(${PROJECT_NAME} ${_HDRFILES} ${_SRCFILES})
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE cpprestsdk::cpprest)
else()
    target_link_libraries(${PROJECT_NAME} boost_system crypto ssl cpprest)
endif()
