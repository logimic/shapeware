project (startup)

file(GLOB_RECURSE _HDRFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE _SRCFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE _SRCCONFIG ${CMAKE_CURRENT_SOURCE_DIR}/configuration/*)

source_group("Header Files" FILES ${_HDRFILES})
source_group("Source Files" FILES ${_SRCFILES})
source_group("Configuration" FILES ${_SRCCONFIG})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${_HDRFILES} ${_SRCFILES} ${_SRCCONFIG} )

target_link_libraries(${PROJECT_NAME} 
  launcher
)

if(UNIX)
  target_link_libraries(${PROJECT_NAME} 
    dl
    pthread
  )
endif()

DeployShapeComponent(${PROJECT_NAME})

DeployShapeConfiguration(${PROJECT_NAME}
	${CMAKE_CURRENT_SOURCE_DIR}/configuration configuration
)

ConfigureMsvcProject("${PROJECT_INSTALL_PREFIX}/bin/startup" ${PROJECT_NAME})
