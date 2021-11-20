set(EXAMPLE_SOURCES echo_server.cpp daytime_client.cpp threadpool.cpp)

foreach (examplesource ${EXAMPLE_SOURCES})
    string(REPLACE ".cpp" "" examplename ${examplesource})
    add_executable(${examplename} ${examplesource})
endforeach (examplesource ${EXAMPLE_SOURCES})