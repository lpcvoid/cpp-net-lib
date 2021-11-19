#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../3rdparty/doctest/doctest/doctest.h"
#include "../src/client.hpp"
#include "../src/endpoint_accessor.hpp"
#include "../src/server.hpp"

using namespace std::chrono_literals;
static const uint16_t test_port = 8888;

TEST_CASE("Test disconnect handling") {
  netlib::server server;
  netlib::client client;
  bool client_was_connected = false;
  bool client_was_disconnected = false;

  server.register_callback_on_connect(
      [&](netlib::client_endpoint endpoint) -> std::vector<uint8_t> {
        client_was_connected = true;
        return {};
      });

  server.register_callback_on_error(
      [&](netlib::client_endpoint endpoint, std::error_condition ec) -> void {
        if (ec == std::errc::connection_aborted) {
          client_was_disconnected = true;
        }
      });

  std::error_condition server_create_res =
      server.create("localhost", test_port, netlib::AddressFamily::IPv4,
                    netlib::AddressProtocol::TCP);

  CHECK_EQ(server.get_client_count(), 0);

  CHECK_FALSE(server_create_res);
  std::error_condition client_create_res =
      client.connect("localhost", test_port, netlib::AddressFamily::IPv4,
                     netlib::AddressProtocol::TCP, 1000ms);

  CHECK_FALSE(client_create_res);
  std::this_thread::sleep_for(100ms);
  CHECK(client_was_connected);
  CHECK_EQ(server.get_client_count(), 1);

  CHECK_FALSE(client.disconnect());
  std::this_thread::sleep_for(100ms);
  CHECK_EQ(server.get_client_count(), 0);
  CHECK(client_was_disconnected);
}