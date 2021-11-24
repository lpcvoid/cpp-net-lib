set(TEST_SOURCES test_client_examplecom.cpp test_client_server.cpp test_client_server_disconnect.cpp)

enable_testing()
add_executable(test_libnetcpp test_main.cpp ${TEST_SOURCES})
target_link_libraries(test_libnetcpp ${CMAKE_THREAD_LIBS_INIT})
add_test(test_libnetcpp test_libnetcpp)