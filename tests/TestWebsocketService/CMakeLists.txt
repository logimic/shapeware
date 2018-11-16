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
file(GLOB_RECURSE _SRCCONFIG ${CMAKE_CURRENT_SOURCE_DIR}/configuration/*)

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

DeployShapeComponent(${PROJECT_NAME})

DeployShapeConfiguration(${PROJECT_NAME}
	${CMAKE_CURRENT_SOURCE_DIR}/configuration configuration
)

DeployShapeConfigurationStartUp("${PROJECT_INSTALL_PREFIX}/bin/startup" ${PROJECT_NAME})

ConfigureMsvcProject("${PROJECT_INSTALL_PREFIX}/bin/startup" ${PROJECT_NAME})
