#include "../doctest/doctest/doctest.h"
#include "../src/netlib.hpp"
#include <atomic>

using namespace std::chrono_literals;
extern uint16_t test_port;

TEST_CASE("RAII test for client and server")
{

    netlib::server server;
    std::atomic<bool> client_was_connected = false;

    server.register_callback_on_connect([&](netlib::client_endpoint endpoint) -> netlib::server_response {
        client_was_connected = true;
        return {};
    });

    std::error_condition server_create_res =
        server.create("localhost", test_port, netlib::AddressFamily::IPv4, netlib::AddressProtocol::TCP);

    CHECK_FALSE(server_create_res);

    {
        netlib::client client;
        std::error_condition client_create_res =
            client.connect("localhost",
                           test_port,
                           netlib::AddressFamily::IPv4,
                           netlib::AddressProtocol::TCP, 1000ms);
        CHECK_FALSE(client_create_res);
        std::this_thread::sleep_for(1000ms);
        CHECK(client_was_connected);
        CHECK_EQ(server.get_client_count(), 1);
    }

    std::this_thread::sleep_for(1000ms);
    CHECK_EQ(server.get_client_count(), 0);
}