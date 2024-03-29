project(MqttService)

if (WIN32) 
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
    FIND_PACKAGE(paho-mqtt REQUIRED)
    set(paho_INCLUDE_DIRS ${paho-mqtt_INCLUDE_DIRS})
    set(paho_FOUND ${paho-mqtt_FOUND})
else()
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")
    FIND_PACKAGE(paho REQUIRED)
endif()

message(STATUS "Found paho: ${paho_FOUND}")
message(STATUS "paho_INCLUDE_DIRS: ${paho_INCLUDE_DIRS}")

set(COMPONENT shape::MqttService)
DeclareShapeComponent(${COMPONENT})
AddShapeProvidedInterface(${COMPONENT} shape::IMqttService)
AddShapeRequiredInterface(${COMPONENT} shape::ILaunchService MANDATORY SINGLE)
AddShapeRequiredInterface(${COMPONENT} shape::ITraceService MANDATORY MULTIPLE)
ConfigureShapeComponent(${COMPONENT} COMPONENT_HXX)

file(GLOB_RECURSE _HDRFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h  ${COMPONENT_HXX})
file(GLOB_RECURSE _SRCFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

source_group("Header Files" FILES ${_HDRFILES})
source_group("Source Files" FILES ${_SRCFILES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${paho_INCLUDE_DIRS})

if (WIN32) 
	link_directories(${PAHO_LIBRARY_DIRS})
endif()

if(SHAPE_STATIC_LIBS)
    add_library(${PROJECT_NAME} STATIC ${_HDRFILES} ${_SRCFILES})
else()
    add_library(${PROJECT_NAME} SHARED ${_HDRFILES} ${_SRCFILES})
endif()

if (WIN32) 
target_link_libraries(${PROJECT_NAME}
	${paho-mqtt_LIBRARY}
)
else()
target_link_libraries(${PROJECT_NAME}
	paho-mqtt3as
)
endif()

DeployShapeComponent(${PROJECT_NAME})

if(WIN32)
	install(
		FILES
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/paho-mqtt3as.dll"
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/libcrypto-1_1-x64.dll"
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/libssl-1_1-x64.dll"
		CONFIGURATIONS Debug
		DESTINATION "\${CMAKE_INSTALL_CONFIG_NAME}/${PROJECT_INSTALL_PREFIX}/bin"
	)

	install(
		FILES
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/paho-mqtt3as.dll"
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/libcrypto-1_1-x64.dll"
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/libssl-1_1-x64.dll"
		CONFIGURATIONS Release
		DESTINATION "\${CMAKE_INSTALL_CONFIG_NAME}/${PROJECT_INSTALL_PREFIX}/bin"
	)
endif()
