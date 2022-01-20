#include "../doctest/doctest/doctest.h"
#include "../src/netlib.hpp"
#include <random>

using namespace std::chrono_literals;

extern uint16_t test_port;

TEST_CASE("Large data transfer")
{
    std::vector<uint8_t> large_data_buffer(1024 * 1024, 0);
    uint8_t overflow_counter = 0;
    std::for_each(large_data_buffer.begin(), large_data_buffer.end(), [&](uint8_t &b) {
        b = overflow_counter++;
    });

    netlib::server server;

    std::vector<netlib::client> clients(2);
    std::vector<netlib::client_endpoint> server_client_endpoints;

    server.register_callback_on_connect([&](netlib::client_endpoint endpoint) -> netlib::server_response {
        if (std::find_if(server_client_endpoints.begin(), server_client_endpoints.end(), [&](const netlib::client_endpoint &ep) {
                return ep == endpoint;
            }) == server_client_endpoints.end()) {
            // new client, add to list of server_client_endpoints
            server_client_endpoints.push_back(endpoint);
        }
        return {};
    });

    server.register_callback_on_recv([&](netlib::client_endpoint endpoint, const std::vector<uint8_t> &data) -> netlib::server_response {
        // figure out which client this is
        if (endpoint == server_client_endpoints.front()) {
            // we want to send to the other one
            server.send_data(data, {server_client_endpoints.back()});
        } else {
            server.send_data(data, {server_client_endpoints.front()});
        }
        return {};
    });

    server.register_callback_on_error([&](netlib::client_endpoint endpoint, std::error_condition ec) -> void {
        std::cerr << "error on server: " << ec.message() << std::endl;
    });

    std::error_condition server_create_res =
        server.create("localhost", test_port, netlib::AddressFamily::IPv4, netlib::AddressProtocol::TCP);

    CHECK_FALSE(server_create_res);

    /*
     * Create two clients that both connect to the server
     */
    std::for_each(clients.begin(), clients.end(), [](netlib::client &c) {
        std::error_condition client_create_res =
            c.connect("localhost", test_port, netlib::AddressFamily::IPv4, netlib::AddressProtocol::TCP, 1000ms);
        CHECK_FALSE(client_create_res);
    });

    std::this_thread::sleep_for(250ms);

    // We now expect the server to know about two clients
    CHECK_EQ(server_client_endpoints.size(), clients.size());

    /* we send data and receive it instantly via async method calls.
     * each client sends data, and receives the data from the other one.
     * each client should such receive and send the same amount of data.
     */

    auto send_res_front = clients.front().send_async(large_data_buffer, 5000ms);
    auto send_res_back = clients.back().send_async(large_data_buffer, 5000ms);

    auto recv_res_front = clients.front().recv_async(large_data_buffer.size(), 5000ms);
    auto recv_res_back = clients.back().recv_async(large_data_buffer.size(), 5000ms);

    // wait for all of this to be done

    while (true) {
        if ((recv_res_front.wait_for(10ms) == std::future_status::ready) && (recv_res_back.wait_for(10ms) == std::future_status::ready) &&
            (send_res_front.wait_for(10ms) == std::future_status::ready) && (send_res_back.wait_for(10ms) == std::future_status::ready)) {
            break;
        }
    }

    // check that all went well
    auto send_data_front = send_res_front.get();
    auto send_data_back = send_res_back.get();

    CHECK_FALSE(send_data_front.second);
    CHECK_FALSE(send_data_back.second);
    CHECK(send_data_front.first == large_data_buffer.size());
    CHECK(send_data_back.first == large_data_buffer.size());

    auto recv_data_front = recv_res_front.get();
    auto recv_data_back = recv_res_back.get();

    CHECK_FALSE(recv_data_front.second);
    CHECK_FALSE(recv_data_back.second);
    CHECK(recv_data_front.first == large_data_buffer);
    CHECK(recv_data_back.first == large_data_buffer);
}