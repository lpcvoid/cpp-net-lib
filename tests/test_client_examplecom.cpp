#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../3rdparty/doctest/doctest/doctest.h"
#include "../src/Client.hpp"

TEST_CASE("Client example.com async")
{
  netlib::client client;
  auto connect_future = client.connect_async("example.com",
                                             static_cast<uint16_t>(80),
                                             netlib::AddressFamily::IPv4,
                                             netlib::AddressProtocol::TCP,
                                             std::chrono::milliseconds(10000));

  while (connect_future.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {}

  CHECK_FALSE(connect_future.get().value());

  const std::string basic_get = R"(GET / HTTP/1.1\r\nHost: example.com\r\n\r\n)";

  auto send_future = client.send_async(std::vector<uint8_t>(basic_get.begin(), basic_get.end()),
      std::chrono::milliseconds(1000));

  while (send_future.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {}
  auto send_result = send_future.get();

  CHECK_FALSE(send_result.second);
  CHECK_EQ(send_result.first, basic_get.size());

  auto recv_future = client.recv_async(2048, std::chrono::milliseconds(3000));
  auto recv_result = recv_future.get();

  CHECK_FALSE(recv_result.second);
  CHECK_GT(recv_result.first.size(), basic_get.size());
  std::string website(recv_result.first.begin(), recv_result.first.end());
  CHECK_NE(website.find("HTTP Version Not Supported"), std::string::npos); // 1.1 is way too old

}