#include "../doctest/doctest/doctest.h"
#include "../src/http/http.hpp"
#include "../src/netlib.hpp"
#include <atomic>

using namespace std::chrono_literals;
extern uint16_t test_port;

TEST_CASE("Test HTTP response parser")
{
    netlib::http::http_response resp;

    std::string raw_response = "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=UTF-8\r\n"
    "Content-Length: 1256\r\n"
    "\r\n\r\n"
    "TEST";

    CHECK_FALSE(resp.from_raw_response(raw_response));
    CHECK_EQ(resp.version.first, 1);
    CHECK_EQ(resp.version.second, 1);
    CHECK_EQ(resp.response_code, 200);
    CHECK_EQ(resp.headers.size(), 2);
    CHECK_EQ(resp.headers[0].first, "Content-Type");
    CHECK_EQ(resp.headers[1].first, "Content-Length");
    CHECK_EQ(resp.headers[0].second, " text/html; charset=UTF-8");
    CHECK_EQ(resp.headers[1].second, " 1256");
    CHECK_EQ(resp.body, "TEST");

}