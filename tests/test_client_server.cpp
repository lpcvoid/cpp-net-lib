#include "../doctest/doctest/doctest.h"
#include "../src/netlib.hpp"
#include <iostream>
#include <random>
#include <sstream>

using namespace std::chrono_literals;

static const std::string hello_msg = "hello!";
static const std::vector<uint8_t> client_message = {1,2,3};
static const std::vector<uint8_t> server_response_message = {1, 3, 3, 7};

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> distr(10000, 65000);

static uint16_t test_port = 7777;//distr(gen);

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

  std::cout << "****Starting failing test" << std::endl;

  server.register_callback_on_connect(
      [&](netlib::client_endpoint endpoint) -> netlib::server_response {
        client_was_connected = true;
        return {.answer = {hello_msg.begin(), hello_msg.end()}};
      });

  server.register_callback_on_recv(
      [&](netlib::client_endpoint endpoint,
          const std::vector<uint8_t> &data) -> netlib::server_response {
        client_has_sent = true;
        return {.answer = server_response_message};
      });

  server.register_callback_on_error(
      [&](netlib::client_endpoint endpoint, std::error_condition ec) -> void {
          std::cerr << "error on server: " << ec.message() << std::endl;
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

  std::this_thread::sleep_for(1000ms);

  std::cout << "***********Starting failing segment" << std::endl;

  auto client_recv = client.recv(1024, 5000ms);
  CHECK_FALSE(client_recv.second);
  if (client_recv.second) {
    std::cout << "test recv error: " << client_recv.second.message() << std::endl;
  }
  CHECK(client_recv.first == server_response_message);

  std::cout << "***********Ending failing segment" << std::endl;
}

