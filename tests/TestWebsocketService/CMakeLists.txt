project(TestWebsocketService)

find_package(GoogleTest REQUIRED)

set(COMPONENT shape::TestWebsocketService)
DeclareShapeComponent(${COMPONENT})
AddShapeRequiredInterface(${COMPONENT} shape::IWebsocketClientService MANDATORY MULTIPLE)
AddShapeRequiredInterface(${COMPONENT} shape::IWebsocketService MANDATORY MULTIPLE)
AddShapeRequiredInterface(${COMPONENT} shape::ILaunchService MANDATORY SINGLE)
AddShapeRequiredInterface(${COMPONENT} shape::ITraceService MANDATORY MULTIPLE)
ConfigureShapeComponent(${COMPONENT} COMPONENT_HXX)

if(WIN32)
  set(_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/configurationWin)
else()
  set(_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/configurationLin)
endif()

file(GLOB_RECURSE _HDRFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h  ${COMPONENT_HXX})
file(GLOB_RECURSE _SRCFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE _SRCCONFIG ${_CONFIG}/* ${CMAKE_CURRENT_SOURCE_DIR}/configuration/*)

source_group("Header Files" FILES ${_HDRFILES})
source_group("Source Files" FILES ${_SRCFILES})
source_group("Configuration" FILES ${_SRCCONFIG})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${GoogleTest_INCLUDE_DIR})

if(SHAPE_STATIC_LIBS)
    add_library(${PROJECT_NAME} STATIC ${_HDRFILES} ${_SRCFILES} ${_SRCCONFIG})
else()
    add_library(${PROJECT_NAME} SHARED ${_HDRFILES} ${_SRCFILES} ${_SRCCONFIG})
endif()

set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER ${TESTS_FOLDER})

target_link_libraries(${PROJECT_NAME} GTestStaticRunner ${GoogleTest_LIBRARIES})

# do the copying
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy_directory ${_CONFIG} "${CMAKE_CURRENT_BINARY_DIR}/configuration"
)

# do the copying
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/configuration "${CMAKE_CURRENT_BINARY_DIR}/configuration"
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
	ARCHIVE DESTINATION lib
)

install(
    DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/configuration
    DESTINATION configurations/tests/${PROJECT_NAME}
)
