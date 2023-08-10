#define NETLIB_USE_HTTP

#include "../src/netlib.hpp"
#include <csignal>
#include <iomanip>
#include <iostream>


void exit_handler(int s){
    std::cout << "Goodbye!" << std::endl;
    exit(EXIT_SUCCESS);
}

int main(int argc, char** argv)
{
    netlib::http::http_client client;
    auto res = client.get("http://example.com");

    if (res.second) {
        std::cerr << "Error: " << res.second.message() << std::endl;
        exit(1);
    }

    std::cout << "Got HTTP response: " << res.first->response_code <<
        ", version " << res.first->version.first << "." << res.first->version.second << std::endl;
    std::cout << "Header entries:" << std::endl;
    std::for_each(res.first->headers.begin(), res.first->headers.end(), [](auto header_entry) {
       std::cout << header_entry.first << " = " << header_entry.second << std::endl;
    });
    std::cout << "Body:" << std::endl;
    std::cout << res.first.value().body << std::endl;

    signal(SIGINT, exit_handler);
}