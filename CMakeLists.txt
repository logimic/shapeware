project(shapeware)

cmake_minimum_required(VERSION 3.0)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

enable_testing()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
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

############# warnings
#set(CPPFLAGS "-Wdate-time -D_FORTIFY_SOURCE=2")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O2 -fstack-protector-strong -Wformat -Werror=format-security -Wall -Wextra ${CPPFLAGS}")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2 -fstack-protector-strong -Wformat -Werror=format-security -Wall -Wextra ${CPPFLAGS}")
#############

FIND_PACKAGE(shape REQUIRED)

set(PROJECT_INSTALL_PREFIX ${PROJECT_NAME})
set(CMAKE_INSTALL_PREFIX ${shape_DEPLOY})

message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
include(${CMAKE_MODULE_PATH}/ShapeComponentDeclaration.cmake)

set_directory_properties(PROPERTIES COMPILE_DEFINITIONS $<$<CONFIG:Debug>:_DEBUG>)

include_directories(${shape_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(include)
#add_subdirectory(CppRestService)
#add_subdirectory(BufferService)
add_subdirectory(CurlRestApiService)
add_subdirectory(WebsocketCppService)
add_subdirectory(WebsocketCppClientService)
add_subdirectory(MqMessageService)
add_subdirectory(MqttService)
add_subdirectory(CommandService)
add_subdirectory(CommandLineService)
add_subdirectory(startup)

if(${BUILD_ZEROMQ})
  add_subdirectory(ZeroMqService)
endif()

if(${BUILD_TESTING})
  add_subdirectory(tests)
endif()

if(${BUILD_EXAMPLES})
  add_subdirectory(examples)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake @ONLY)
