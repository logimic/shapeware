project(shaparts)

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

#######################
# step 0
find_path(LIBWEBSOCKETS_INCLUDE_DIR libwebsockets.h ${CMAKE_CURRENT_SOURCE_DIR}/external/libwebsockets/build/include)

if((NOT LIBWEBSOCKETS_INCLUDE_DIR) OR (NOT EXISTS ${LIBWEBSOCKETS_INCLUDE_DIR}))
    # we couldn't find the header files for Libwebsockets or they don't exist
    message("Unable to find Libwebsockets")

    # we have a submodule setup for foo, assume it is under external/foo
    # now we need to clone this submodule
    #execute_process(COMMAND git submodule update --init -- external/foo
    #                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    set (LWS_WITH_SSL false)
	add_subdirectory(external/libwebsockets EXCLUDE_FROM_ALL)	
	
    # set LIBWEBSOCKETS_INCLUDE_DIR
    set(LIBWEBSOCKETS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/libwebsockets/build/include
        CACHE PATH "libwebsockets include directory")

    # also install it
    #install(DIRECTORY ${FOO_INCLUDE_DIR}/foo DESTINATION ${some_dest})

    # for convenience setup a target
    add_library(libwebsockets INTERFACE)
    target_include_directories(libwebsockets INTERFACE
                               $<BUILD_INTERFACE:${LIBWEBSOCKETS_INCLUDE_DIR}>)
    #                           $<INSTALL_INTERFACE:${some_dest}>)

    # need to export target as well
    #install(TARGETS foo EXPORT my_export_set DESTINATION ${some_dest})
	
else()
    # see above, setup target as well
endif()

#######################

include_directories(${shape_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(include)
add_subdirectory(WebsocketService)
add_subdirectory(tests/TestWebsocketService)
add_subdirectory(startup)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake @ONLY)
