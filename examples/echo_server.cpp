#include "../src/netlib.hpp"
#include <csignal>
#include <iostream>

void exit_handler(int s){
  std::cout << "Goodbye!" << std::endl;
  exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {

  signal (SIGINT, exit_handler);

  uint16_t port = 8888;
  std::string bind_host = "localhost";
  if (argc == 3) {
    bind_host = argv[1];
    port = std::atol(argv[2]);
  }

  std::cout << "Creating echo server on " << bind_host << ":" << port << std::endl;
  std::cout << "CTRL+C shuts down the server." << std::endl;

  netlib::server server;
  server.register_callback_on_connect(
      [&](netlib::client_endpoint endpoint) -> netlib::server_response {
        std::string ip = netlib::endpoint_accessor::ip_to_string(endpoint.addr, endpoint.addr_len).value();
        std::cout << "Client connected! IP: " << ip << std::endl;
        return {};
      });
  server.register_callback_on_recv(
      [&](netlib::client_endpoint endpoint,
          const std::vector<uint8_t> &data) -> netlib::server_response{
        std::cout << "Client sent some data, echoing it back!" << std::endl;
        return {.answer = data};
      });
  std::error_condition server_create_res = server.create(bind_host,
                                                         static_cast<uint16_t>(port),
                                                         netlib::AddressFamily::IPv4,
                                                         netlib::AddressProtocol::TCP);
  if (server_create_res) {
    std::cerr << "Error initializing server: " << server_create_res.message() << std::endl;
  }

  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

}

