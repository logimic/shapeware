project (startup)

if(WIN32)
  set(_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/configurationWin)
else()
  set(_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/configurationLin)
endif()

file(GLOB_RECURSE _HDRFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE _SRCFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE _SRCCONFIG ${_CONFIG}/* ${CMAKE_CURRENT_SOURCE_DIR}/configuration/*)

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

#copy configuration
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

#DeployShapeTarget(${PROJECT_SUBDIR} ${PROJECT_NAME})

INSTALL(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
	ARCHIVE DESTINATION lib
)


