set(EXAMPLE_SOURCES echo_server.cpp daytime_client.cpp threadpool.cpp time_client.cpp)

if (WITH_HTTP)
    set(EXAMPLE_SOURCES ${EXAMPLE_SOURCES} http_client.cpp)
endif()

foreach (examplesource ${EXAMPLE_SOURCES})
    string(REPLACE ".cpp" "" examplename ${examplesource})
    add_executable(${examplename} ${examplesource})
    target_link_libraries(${examplename} ${CMAKE_THREAD_LIBS_INIT})
endforeach (examplesource ${EXAMPLE_SOURCES})