# cpp-net-lib

Modern, compact and cross plattform C++ network/sockets library. 
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

### Examples

The library consists of only two user facing classes, `netlib::client` 
and `netlib::server`.

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

std::string website(recv_result.first.begin(), recv_result.first.end());
std::cout << website << std::endl;
```
    


