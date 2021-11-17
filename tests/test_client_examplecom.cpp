#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../3rdparty/doctest/doctest/doctest.h"
#include "../src/Client.hpp"

static const std::string basic_get = R"(GET / HTTP/1.1\r\nHost: example.com\r\n\r\n)";

using namespace std::chrono_literals;

TEST_CASE("Client example.com async")
{
  netlib::client client;
  auto connect_future = client.connect_async("example.com",
                                             static_cast<uint16_t>(80),
                                             netlib::AddressFamily::IPv4,
                                             netlib::AddressProtocol::TCP,
                                             10000ms);

  while (connect_future.wait_for(10ms) != std::future_status::ready) {}

  CHECK_FALSE(connect_future.get().value());

  auto send_future = client.send_async({basic_get.begin(), basic_get.end()},1000ms);

  while (send_future.wait_for(10ms) != std::future_status::ready) {}
  auto send_result = send_future.get();

  CHECK_FALSE(send_result.second);
  CHECK_EQ(send_result.first, basic_get.size());

  auto recv_future = client.recv_async(2048, 3000ms);
  auto recv_result = recv_future.get();

  CHECK_FALSE(recv_result.second);
  CHECK_GT(recv_result.first.size(), basic_get.size());
  std::string website(recv_result.first.begin(), recv_result.first.end());
  CHECK_NE(website.find("HTTP Version Not Supported"), std::string::npos); // 1.1 is way too old

}

TEST_CASE("Client example.com") {
  netlib::client client;
  std::error_condition connect_result = client.connect("example.com",
                                             "http",
                                             netlib::AddressFamily::IPv4,
                                             netlib::AddressProtocol::TCP,
                                             1000ms);

  CHECK_FALSE(connect_result);

  std::pair<std::size_t, std::error_condition> send_result = client.send(
      {basic_get.begin(), basic_get.end()},1000ms);

  CHECK_FALSE(send_result.second);
  CHECK_EQ(send_result.first, basic_get.size());

  std::pair<std::vector<uint8_t>, std::error_condition>  recv_result = client.recv(2048, 3000ms);

  CHECK_FALSE(recv_result.second);
  CHECK_GT(recv_result.first.size(), basic_get.size());
  std::string website(recv_result.first.begin(), recv_result.first.end());
  CHECK_NE(website.find("HTTP Version Not Supported"), std::string::npos); // 1.1 is way too old
}

TEST_CASE("test") {
  netlib::client client;
  auto connect_future = client.connect_async("example.com",
                                             static_cast<uint16_t>(80),
                                             netlib::AddressFamily::IPv4,
                                             netlib::AddressProtocol::TCP,
                                             10000ms);

  while (connect_future.wait_for(10ms) != std::future_status::ready) {
    // do something cool while waiting for connect to finish
  }

  std::error_condition connect_error = connect_future.get();
  if (connect_error) {
    std::cerr << "Connection failed, error: " << connect_error.message() << std::endl;
    return;
  }

  auto send_future = client.send_async({basic_get.begin(), basic_get.end()},1000ms);

  while (send_future.wait_for(10ms) != std::future_status::ready) {
    // do something cool while waiting for send to finish
  }

  std::pair<std::size_t, std::error_condition> send_result = send_future.get();

  if (send_result.second) {
    std::cerr << "Send error, error: " << send_result.second.message() << std::endl;
    return;
  }

  std::cout << "We sent " << send_result.first << " bytes!" << std::endl;

  auto recv_future = client.recv_async(2048, 3000ms);
  while (recv_future.wait_for(10ms) != std::future_status::ready) {
    // do something cool while waiting for data to come in
  }

  auto recv_result = recv_future.get();

  std::cout << "We got " << recv_result.first.size() << " bytes!" << std::endl;

  std::string website(recv_result.first.begin(), recv_result.first.end());
  std::cout << website << std::endl;
}