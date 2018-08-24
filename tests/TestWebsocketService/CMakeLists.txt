project(TestWebsocketService)

find_package(GoogleTest REQUIRED)

set(COMPONENT shape::TestWebsocketService)
DeclareShapeComponent(${COMPONENT})
AddShapeRequiredInterface(${COMPONENT} shape::IWebsocketClientService MANDATORY MULTIPLE)
AddShapeRequiredInterface(${COMPONENT} shape::IWebsocketService MANDATORY MULTIPLE)
AddShapeRequiredInterface(${COMPONENT} shape::ILaunchService MANDATORY SINGLE)
AddShapeRequiredInterface(${COMPONENT} shape::ITraceService MANDATORY MULTIPLE)
ConfigureShapeComponent(${COMPONENT} COMPONENT_HXX)

file(GLOB_RECURSE _HDRFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h  ${COMPONENT_HXX})
file(GLOB_RECURSE _SRCFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

source_group("Header Files" FILES ${_HDRFILES})
source_group("Source Files" FILES ${_SRCFILES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${GoogleTest_INCLUDE_DIR})

add_definitions(-DGTEST_LINKED_AS_SHARED_LIBRARY=1)

if(SHAPE_STATIC_LIBS)
    add_library(${PROJECT_NAME} STATIC ${_HDRFILES} ${_SRCFILES})
else()
    add_library(${PROJECT_NAME} SHARED ${_HDRFILES} ${_SRCFILES})
endif()

target_link_libraries(${PROJECT_NAME} ${GoogleTest_LIBRARIES})
target_compile_definitions(${PROJECT_NAME} PRIVATE GTEST_LINKED_AS_SHARED_LIBRARY=1)

