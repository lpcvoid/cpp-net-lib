set(TEST_SOURCES test_client_examplecom.cpp test_client_server.cpp test_client_server_disconnect.cpp)

foreach (testsource ${TEST_SOURCES})
    string(REPLACE ".cpp" "" testname ${testsource})
    add_executable(${testname} ${testsource})
endforeach (testsource ${TEST_SOURCES})
