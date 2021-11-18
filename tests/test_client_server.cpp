#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../3rdparty/doctest/doctest/doctest.h"
#include "../src/Client.hpp"
#include "../src/Server.hpp"
#include "../src/endpoint_accessor.hpp"

using namespace std::chrono_literals;

static const std::string hello_msg = "hello!";
static const std::vector<uint8_t> client_message = {1,2,3};
static const std::vector<uint8_t> server_response_message = {1, 3, 3, 7};
static const uint16_t test_port = 8888;

std::string to_hex_array(const std::vector<uint8_t> &data) {
  std::stringstream stream;
  stream << std::hex;
  for (uint8_t b : data) {
    stream << b << " ";
  }
  return stream.str();
};

TEST_CASE("Test server with callbacks") {
  netlib::server server;
  netlib::client client;
  bool client_was_connected = false;
  bool client_has_sent = false;

  server.register_callback_on_connect(
      [&](netlib::client_endpoint endpoint) -> std::vector<uint8_t> {
        client_was_connected = true;
        return {hello_msg.begin(), hello_msg.end()};
      });

  // std::vector<uint8_t>(client_endpoint, std::vector<uint8_t>)
  server.register_callback_on_recv(
      [&](netlib::client_endpoint endpoint,
          const std::vector<uint8_t> &data) -> std::vector<uint8_t> {
        client_has_sent = true;
        return server_response_message;
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
  CHECK_EQ(hello_message_res.first,
           std::vector<uint8_t>(hello_msg.begin(), hello_msg.end()));

  std::pair<std::size_t, std::error_condition> send_res = client.send(client_message, 1000ms);
  CHECK_FALSE(send_res.second);
  CHECK_EQ(send_res.first, client_message.size());

  auto client_recv = client.recv(1024, 1000ms);
  CHECK_FALSE(client_recv.second);
  CHECK(client_recv.first == server_response_message);
}
