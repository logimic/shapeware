project(shapeware)

cmake_minimum_required(VERSION 3.0)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TOOL MATCHES "(msdev|devenv|nmake|MSBuild)")
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
  CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
  if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=c++11 -pthread")
  elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=c++0x -pthread")
  else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
  endif()
endif()

FIND_PACKAGE(shape REQUIRED)

message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
include(${CMAKE_MODULE_PATH}/ShapeComponentDeclaration.cmake)


include_directories(${shape_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/include)

# external/libwebsockets
add_subdirectory(external/libwebsockets EXCLUDE_FROM_ALL)	

add_subdirectory(include)
add_subdirectory(CppRestService)
add_subdirectory(WebsocketService)
add_subdirectory(tests/TestWebsocketService)
add_subdirectory(tests/TestCppRestService)
add_subdirectory(startup)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake @ONLY)
