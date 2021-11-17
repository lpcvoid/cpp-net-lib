set(TEST_SOURCES test_client_examplecom.cpp test_client_server.cpp)

foreach (testsource ${TEST_SOURCES})
    string(REPLACE ".cpp" "" testname ${testsource})
    add_executable(${testname} ${testsource})
    target_link_libraries(${testname} PRIVATE netcpp)
endforeach (testsource ${TEST_SOURCES})
