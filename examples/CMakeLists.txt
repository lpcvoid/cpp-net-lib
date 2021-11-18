set(EXAMPLE_SOURCES echo_server.cpp daytime_client.cpp)

foreach (examplesource ${EXAMPLE_SOURCES})
    string(REPLACE ".cpp" "" examplename ${examplesource})
    add_executable(${examplename} ${examplesource})
    target_link_libraries(${examplename} PRIVATE netcpp)
endforeach (examplesource ${EXAMPLE_SOURCES})