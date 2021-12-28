set(TEST_SOURCES
        test_client_examplecom.cpp
        test_client_server.cpp
        test_client_server_disconnect.cpp
        test_large_data_transfer.cpp
        test_raii.cpp)

enable_testing()
add_executable(test_libnetcpp main.cpp ${TEST_SOURCES})
target_link_libraries(test_libnetcpp ${CMAKE_THREAD_LIBS_INIT})
add_test(test_libnetcpp test_libnetcpp)