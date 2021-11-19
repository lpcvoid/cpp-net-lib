#include "../src/client.hpp"
#include "../src/server.hpp"
#include <csignal>
#include <iostream>

using namespace std::chrono_literals;

void exit_handler(int s){
  std::cout << "Goodbye!" << std::endl;
  exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {

  signal(SIGINT, exit_handler);

  uint16_t port = 13;
  std::string time_host = "time-b-b.nist.gov";
  if (argc == 3) {
    time_host = argv[1];
    port = std::atol(argv[2]);
  }

  std::cout << "Connecting to " << time_host << " on port " << port << std::endl;
  netlib::client client;

  std::error_condition client_create_res =
      client.connect(time_host, port, netlib::AddressFamily::IPv4,
                     netlib::AddressProtocol::TCP, 1000ms);

  if (client_create_res) {
    std::cerr << "Error connecting to host, error : " << client_create_res.message() << std::endl;
    exit(EXIT_FAILURE);
  }

  uint32_t error_count = 0;
  while (true) {
    auto daytime_res = client.recv(1024, 1000ms);
    if (daytime_res.second) {
      if (error_count++ == 3) {
        std::cerr << "Attempted to read three times, all failed, exiting. Error: " <<
            daytime_res.second.message() << std::endl;
        exit(EXIT_FAILURE);
      }
      std::this_thread::sleep_for(1000ms);
    } else {
      std::string daytime_string(daytime_res.first.begin(), daytime_res.first.end());
      std::cout << daytime_string << std::endl;
      exit(EXIT_SUCCESS);
    }

  }

}