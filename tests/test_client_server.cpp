#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../3rdparty/doctest/doctest/doctest.h"
#include "../src/Client.hpp"
#include "../src/Server.hpp"

using namespace std::chrono_literals;

static const std::string hello_msg = "hello!";
static const uint16_t test_port = 8888;

TEST_CASE("Test server with callbacks") {
  netlib::server server;
  netlib::client client;
  bool client_was_connected = false;

  server.register_callback_on_connect(
      [&](netlib::client_endpoint endpoint) -> std::vector<uint8_t> {
        std::cout << "Client connected!" << std::endl;
        client_was_connected = true;
        return {hello_msg.begin(), hello_msg.end()};
      });

  std::error_condition server_create_res =
      server.create("localhost", test_port, netlib::AddressFamily::IPv4,
                    netlib::AddressProtocol::TCP);

  CHECK_FALSE(server_create_res);

  std::error_condition client_create_res =
      client.connect("localhost", test_port, netlib::AddressFamily::IPv4,
                     netlib::AddressProtocol::TCP, 1000ms);

  CHECK_FALSE(client_create_res);
  std::this_thread::sleep_for(100ms);
  CHECK(client_was_connected);
  auto hello_message_res = client.recv(hello_msg.size(), 1000ms);
  CHECK_FALSE(hello_message_res.second);
  CHECK_EQ(hello_message_res.first, std::vector<uint8_t>(hello_msg.begin(), hello_msg.end()));
}