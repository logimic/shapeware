project(CurlRestApiService)

find_package(CURL REQUIRED)
if(WIN32)
  # needed if build from vcpkg
  find_package(ZLIB)
endif()

set(COMPONENT shape::CurlRestApiService)
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
include_directories(${CURL_INCLUDE_DIRS})

if(SHAPE_STATIC_LIBS)
    add_library(${PROJECT_NAME} STATIC ${_HDRFILES} ${_SRCFILES})
else()
    add_library(${PROJECT_NAME} SHARED ${_HDRFILES} ${_SRCFILES})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${CURL_LIBRARIES})

DeployShapeComponent(${PROJECT_NAME})

if(WIN32)
	install(
		FILES
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/libcurl-d.dll"
			#"$<TARGET_FILE_DIR:${PROJECT_NAME}>/SSLEAY32.dll"
			#"$<TARGET_FILE_DIR:${PROJECT_NAME}>/LIBEAY32.dll"
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/zlibd1.dll"
		CONFIGURATIONS Debug
		DESTINATION "\${CMAKE_INSTALL_CONFIG_NAME}/${PROJECT_INSTALL_PREFIX}/bin"
	)

	install(
		FILES
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/libcurl.dll"
			#"$<TARGET_FILE_DIR:${PROJECT_NAME}>/SSLEAY32.dll"
			#"$<TARGET_FILE_DIR:${PROJECT_NAME}>/LIBEAY32.dll"
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/zlib1.dll"
		CONFIGURATIONS Release
		DESTINATION "\${CMAKE_INSTALL_CONFIG_NAME}/${PROJECT_INSTALL_PREFIX}/bin"
	)
endif()
