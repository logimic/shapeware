set(TESTS_FOLDER tests)

add_subdirectory(include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(TestWebsocketService)
add_subdirectory(TestCurlRestApiService)
add_subdirectory(TestMqttService)
if(${BUILD_ZEROMQ})
  add_subdirectory(TestZeroMqService)
endif()
#add_subdirectory(TestBufferService)
#add_subdirectory(TestCppRestService)
add_subdirectory(TestSimulationIRestApiService)
