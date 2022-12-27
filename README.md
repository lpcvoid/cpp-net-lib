# cpp-net-lib

[![All platform build test](https://github.com/lpcvoid/cpp-net-lib/actions/workflows/test_matrix.yml/badge.svg?branch=master)](https://github.com/lpcvoid/cpp-net-lib/actions/workflows/test_matrix.yml)

Modern, header-only, compact and cross-platform C++ network/sockets library. 
Don't mind the crappy name, I suck at naming things.

### Why?

I needed a small, portable, and easy to use networking library. 
For years, I just used the POSIX socket api directly, since it's
easy and portable (some small platform differences notwithstanding).
Since I kept reinventing the wheel, I decided to write this library 
after trying ASIO, which I found way to heavy for my tastes.

### How?

You can just add this repo as a git submodule, which at this point is
probably (IMO) the best way to handle C++ dependencies.

```shell
git submodule add https://github.com/lpcvoid/cpp-net-lib.git extern
```

This will check out the lib as a submodule within your project. Now just `#include "extern/cpp-net-lib/src/netlib.hpp"` somewhere.

Alternatively, you can run the examples and tests like you would any other CMake based
project: 
```shell
cmake -B build
cmake --build build
```

There are some cmake flags you can use:

| Option             | Default  | Description                                                                       |
|--------------------|----------|-----------------------------------------------------------------------------------|
| __BUILD_TESTS__    | __ON__   | Builds tests using `doctest`,which is then introduced as a dependency.            |
| __BUILD_EXAMPLES__ | __ON__   | Builds some small example programs.                                               |
| __WITH_HTTP__      | __ON__   | Builds library with HTTP support                                                  |

### Short introduction

The library consists of only a few user facing classes:

`netlib::client` is a network client implementation that offers async and blocking capabilities. Async is done efficiently via a threadpool.
 
`netlib::server` is a network server which can handle client requests in an easy manner via callbacks. It uses a threadpool internally. 
See the examples for some usecases, it's really self explaining.

`netlib::endpoint_accessor` is used to retrieve information like IP and port from an incoming client connection. See examples.

`netlib::thread_pool` is the threadpool implementation used throughout this lib. You can use it for other stuff too, 
if you like.

`netlib::socket` is a platform independent socket wrapper over the POSIX socket api.

`netlib::server_response` is a struct that you can return in your server callbacks, which instructs the server how to handle your
response. You can pass it some data to relay to clients, or instruct server to terminate the connection (after sending data, if any).

`netlib::http::client` is an HTTP client, which can currently only GET http responses (No other Verbs, and no TLS).

### Examples

In all of these examples, I assume you have `using namespace std::chrono_literals` 
somewhere. If you don't want to "pollute" the `std` namespace, you need to
instead use something like `std::chrono::milliseconds(n)`.

#### Synchronous client (blocking)

```c++
netlib::client client;
std::error_condition connect_result = 
client.connect("example.com",                  // can also be an ip
               "http",                         // can also be a uint16_t port
               netlib::AddressFamily::IPv4,    // use IPv4
               netlib::AddressProtocol::TCP,   // use TCP since we are interested in http
               1000ms);                        // timeout

if (connect_result) {
    std::cerr << "Connection failed! Error: " << connect_result.message() << std::endl;
    return;
}

static const std::string basic_get = R"(GET / HTTP/1.1\r\nHost: example.com\r\n\r\n)";
std::pair<std::size_t, std::error_condition> send_res = 
                            client.send({basic_get.begin(), basic_get.end()}, 1000ms);
if (send_res.second) {
    std::cerr << "Sending failed! Error: " << send_res.second.message() << std::endl;
    return;
}

std::pair<std::vector<uint8_t>, std::error_condition>  recv_res = client.recv(2048, 3000ms);
if (recv_res.second) {
    std::cerr << "Recv failed! Error: " << recv_res.second.message() << std::endl;
    return;
}
std::string website(recv_res.first.begin(), recv_res.first.end());
std::cout << website << std::endl;
```
#### Asynchronous client (non-blocking)

```c++
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

static const std::string basic_get = R"(GET / HTTP/1.1\r\nHost: example.com\r\n\r\n)";
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
```
#### Echo server

This code sample creates a TCP server on localhost:1337, which will echo whatever you send to it. Each of the callbacks
will be executed by a random thread internally. 

You may want to disconnect each client after they get their echo - this can be done by setting `.terminate` to `true` in
the `server_response` struct you return from within your callback. Leaving the `answer` vector empty will not send any data.

```c++
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
std::error_condition server_create_res = server.create("localhost", 
                                                       static_cast<uint16_t>(1337), 
                                                       netlib::AddressFamily::IPv4,
                                                       netlib::AddressProtocol::TCP);
if (server_create_res) {
    std::cerr << "Error initializing server: " << server_create_res.message() << std::endl;
}
```

#### HTTP client (GET request)

```C++
netlib::http::http_client client;
auto res = client.get("http://example.com");

if (res.second) {
    std::cerr << "Error: " << res.second.message() << std::endl;
    exit(1);
}

std::cout << "Got HTTP response: " << res.first->response_code <<
    ", version " << res.first->version.first << "." << res.first->version.second << std::endl;
std::cout << "Header entries:" << std::endl;
std::for_each(res.first->headers.begin(), res.first->headers.end(), [](auto header_entry) {
   std::cout << header_entry.first << " = " << header_entry.second << std::endl;
});
std::cout << "Body:" << std::endl;
std::cout << res.first.value().body << std::endl;
```