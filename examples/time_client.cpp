#include "../src/netlib.hpp"
#include <csignal>
#include <iomanip>
#include <iostream>

using namespace std::chrono_literals;

void exit_handler(int s){
  std::cout << "Goodbye!" << std::endl;
  exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {

  signal(SIGINT, exit_handler);
  uint16_t port = 37;
  std::string time_host = "time.nist.gov"; //https://tf.nist.gov/tf-cgi/servers.cgi#
  if (argc == 3) {
    time_host = argv[1];
    port = std::atol(argv[2]);
  }

  std::cout << "Connecting to " << time_host << " on port " << port << std::endl;
  netlib::client client;

  std::error_condition client_create_res =
      client.connect(time_host, port, netlib::AddressFamily::IPv4,
                     netlib::AddressProtocol::UDP, 1000ms);

  if (client_create_res) {
    std::cerr << "Error connecting to host, error : " << client_create_res.message() << std::endl;
    exit(EXIT_FAILURE);
  }

  //since time uses udp, we need to send some random udp datagram to server,
  //since it has no way of knowing that we want the datetime
  //(udp is connectionless)
  //Interesting that the send payload can be lower than the 4 expected bytes -
  //this could potentially be used in an udp reflection attack
  std::vector<uint8_t> dummy_payload(4, 0);
  auto send_res = client.send(dummy_payload, 1000ms);
  if (send_res.second) {
    std::cout << "Error sending byte in udp mode for triggering response: " <<
        send_res.second.message() << std::endl;
    exit(EXIT_FAILURE);
  }

  auto time_res = client.recv(1024, 1000ms);
  if (time_res.second) {
    std::cerr << "Failed to get UDP datagram. Error: " << time_res.second.message() << std::endl;
    exit(EXIT_FAILURE);
  }

  if (time_res.first.size() != 4) {
    std::cerr << "Did not recieve 32 bits - instead got " << time_res.first.size() << std::endl;
    exit(EXIT_FAILURE);
  }

  uint32_t time_result = (time_res.first[0] << 24) |
                         (time_res.first[1] << 16) |
                         (time_res.first[2] << 8) |
                         (time_res.first[3]);

  std::cout << "Value from server: " << time_result << std::endl;
  //print out actual time
  std::time_t temp = time_result - 2208988800;
  std::cout << std::put_time(std::gmtime(&temp), "Formatted: %Y-%m-%d %I:%M:%S %p") << std::endl;
}