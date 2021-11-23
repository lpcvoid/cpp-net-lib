#pragma once

#include "service_resolver.hpp"
#include "socket.hpp"
#include "thread_pool.hpp"
#include <atomic>
#include <cassert>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace netlib {

    struct client_endpoint {
      netlib::socket socket;
      sockaddr addr{};
      socklen_t addr_len = sizeof(sockaddr);
    };

    struct server_response {
      std::vector<uint8_t> answer{};
      bool terminate = false;
    };

    using callback_connect_t = std::function<server_response(client_endpoint)>;
    using callback_recv_t = std::function<server_response(client_endpoint, std::vector<uint8_t>)>;
    using callback_error_t = std::function<void(client_endpoint, std::error_condition)>;

    class server {
    private:
        std::optional<netlib::socket> _listener_sock;
        int32_t _accept_queue_size = 10;
        std::vector<client_endpoint> _clients;
        std::mutex _mutex;
        std::atomic<bool> _server_active = false;
        callback_connect_t _cb_onconnect{};
        callback_recv_t _cb_on_recv{};
        callback_error_t _cb_on_error{};
        std::thread _accept_thread;
        std::thread _processor_thread;
        netlib::thread_pool _thread_pool;

        inline void processing_func() {
          while (_server_active) {
            fd_set fdset;
            socket_t highest_fd = 0;
            FD_ZERO(&fdset);
            std::vector<client_endpoint> local_clients;
            {
              std::lock_guard<std::mutex> lock(_mutex);
              local_clients = _clients;
              for (auto& client : local_clients) {
                socket_t fd = client.socket.get_raw().value();
                if (highest_fd < fd){
                  highest_fd = fd;
                }
                FD_SET(fd, &fdset);
              }
            }
            //we want the timeout to be fairly low, so that we avoid situations where
            //we have a new client in _clients, but are not monitoring it yet - that
            //would mean we have a "hardcoded" delay in servicing a new clients packets
            timeval tv{.tv_sec = 0, .tv_usec= 50 * 1000}; //50ms
            int32_t select_res = ::select(highest_fd + 1, &fdset, nullptr, nullptr, &tv);
            if (select_res > 0) {
              std::vector<client_endpoint> client_refs(select_res);
              uint32_t index = 0;
              {
                for (auto& client : local_clients) {
                  socket_t fd = client.socket.get_raw().value();
                  if (FD_ISSET(fd, &fdset)){
                    client_refs[index++] = client;
                  }
                }
              }
              assert(index == select_res);
              //add callback tasks to threadpool for processing
              for (auto& client_to_recv : client_refs) {
                _thread_pool.add_task([&](client_endpoint ce){
                  this->handle_client(ce);
                }, client_to_recv);
              }

            } else if (select_res == 0) {
              //nothing interesting happened before timeout
            } else {
              //error was returned
              std::cerr << "server select error: " << socket_get_last_error().message() << std::endl;
            }
          }
        }

        inline void accept_func() {
          client_endpoint new_endpoint;
          while (_server_active) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            new_endpoint.addr_len = sizeof(addrinfo);
            int32_t status = ::accept(_listener_sock->get_raw().value(), &new_endpoint.addr, &new_endpoint.addr_len);
            if (status > 0) {
              new_endpoint.socket.set_raw(status);
              new_endpoint.socket.set_nonblocking(true);
              if (_cb_onconnect) {
                netlib::server_response greeting = _cb_onconnect(new_endpoint);
                if (!greeting.answer.empty()) {
                  ssize_t send_result = ::send(new_endpoint.socket.get_raw().value(),
                                               greeting.answer.data(),
                                               greeting.answer.size(),
                                               0);
                  if ((send_result != greeting.answer.size()) && (_cb_on_error)) {
                    _cb_on_error(new_endpoint, socket_get_last_error());
                  }
                }
                if (greeting.terminate) {
                  if (_cb_on_error) {
                    _cb_on_error(new_endpoint, std::errc::connection_aborted);
                  }
                  new_endpoint.socket.close();
                  std::cout << "server kicked client accept" << std::endl;
                  continue;
                }
              }
              if (new_endpoint.socket.is_valid()) {
                std::lock_guard<std::mutex> lock(_mutex);
                _clients.push_back(new_endpoint);
              }
            }
          }
        }

        inline std::error_condition handle_client(client_endpoint endpoint) {
          std::vector<uint8_t> total_buffer;
          std::array<uint8_t, 2048> buffer{};
          ssize_t recv_res = 0;
          ssize_t recv_res_cycle = 0;
          while ((recv_res_cycle = ::recv(endpoint.socket.get_raw().value(), buffer.data(), buffer.size(), MSG_WAITALL)) > 0) {
            total_buffer.insert(total_buffer.end(), buffer.begin(), buffer.begin() + recv_res_cycle);
            recv_res += recv_res_cycle;
          }
          if (recv_res == 0){
            std::cout << "server recv_res == 0" << std::endl;
            if (_cb_on_error) {
              _cb_on_error(endpoint, std::errc::connection_aborted);
            }
            remove_client(endpoint.socket.get_raw().value());
            endpoint.socket.close();
            std::cout << "server kicked client recv 0" << std::endl;
            return std::errc::connection_aborted;
          } else if (recv_res < 0) {
            //error
            std::cout << "server recv_res == -1" << std::endl;
            std::error_condition recv_error = socket_get_last_error();
            //we do not want to spam callback with wouldblock messages
            //for portability we shall check both EAGAIN and EWOULDBLOCK
            if ((recv_error != std::errc::resource_unavailable_try_again) &&
                  (recv_error != std::errc::operation_would_block)) {
              if (_cb_on_error) {
                _cb_on_error(endpoint, recv_error);
              }
            }
            return recv_error;
          } else {
            std::cout << "server recv_res > 0: " << recv_res << " " << total_buffer.size() << std::endl;
            //we got data
            if (_cb_on_recv) {
              netlib::server_response response = _cb_on_recv(endpoint, total_buffer);
              if (!response.answer.empty()) {
                ssize_t send_result = ::send(endpoint.socket.get_raw().value(),
                                             response.answer.data(),
                                             response.answer.size(),
                                             0);
                if ((send_result != response.answer.size()) && (_cb_on_error)) {
                  _cb_on_error(endpoint, socket_get_last_error());
                }
              }
              if (response.terminate) {
                if (_cb_on_error) {
                  _cb_on_error(endpoint, std::errc::connection_aborted);
                }
                remove_client(endpoint.socket.get_raw().value());
                endpoint.socket.close();
                std::cout << "server kicked client because wanted" << std::endl;
              }

            }
            return {};
          }

        }

        bool remove_client(socket_t socket_id) {
          //the remove_if-> erase idiom is perhaps my most hated part about std containers
          std::lock_guard<std::mutex> lock(_mutex);
          return _clients.erase(
              std::remove_if(_clients.begin(), _clients.end(), [&](const client_endpoint& ce) {
                return ce.socket.get_raw() == socket_id;
              }),_clients.end()) != _clients.end();
        }

      public:
        server() = default;
        virtual ~server() { stop();
        }
        inline std::error_condition create(const std::string& bind_host,
                                    const std::variant<std::string,uint16_t>& service,
                                    AddressFamily address_family,
                                    AddressProtocol address_protocol) {
          if (_listener_sock.has_value()) {
            this->stop();
          }

          const std::string service_string = std::holds_alternative<uint16_t>(service) ? std::to_string(std::get<uint16_t>(service)) : std::get<std::string>(service);
          std::pair<addrinfo*, std::error_condition> addrinfo_result = service_resolver::get_addrinfo(std::nullopt, service_string, address_family, address_protocol, AI_PASSIVE);

          if (addrinfo_result.first == nullptr) {
            return addrinfo_result.second;
          }

          auto close_and_free = [&](){
            this->stop();
            freeaddrinfo(addrinfo_result.first);
          };

          for (addrinfo* res_addrinfo = addrinfo_result.first; res_addrinfo != nullptr; res_addrinfo = res_addrinfo->ai_next) {
            _listener_sock = netlib::socket();
            std::error_condition s_create_error = _listener_sock->create(res_addrinfo->ai_family, res_addrinfo->ai_socktype, res_addrinfo->ai_protocol);
            if (s_create_error) {
              close_and_free();
              continue;
            }
            _listener_sock->set_nonblocking(true);
            //_listener_sock->set_reuseaddr(true);

            int32_t res = ::bind(_listener_sock->get_raw().value(), res_addrinfo->ai_addr, res_addrinfo->ai_addrlen);
            if (res < 0) {
              close_and_free();
              continue;
            }
            if (address_protocol == AddressProtocol::TCP) {
              res = ::listen(_listener_sock->get_raw().value(), _accept_queue_size);
              if (res < 0) {
                close_and_free();
                continue;
              }
            }
            //all went well
            break;
          }
          if (_listener_sock) {
            _server_active = true;
            _accept_thread = std::thread(&server::accept_func, this);
            _processor_thread = std::thread(&server::processing_func, this);
            return {};
          }
          return socket_get_last_error();
        }
        inline void register_callback_on_connect(callback_connect_t onconnect) {_cb_onconnect = std::move(onconnect);};
        inline void register_callback_on_recv(callback_recv_t onrecv) {_cb_on_recv = std::move(onrecv);};
        inline void register_callback_on_error(callback_error_t onerror) {_cb_on_error = std::move(onerror);};

        inline void stop(){
          _server_active = false;
          if (_accept_thread.joinable()) {
            _accept_thread.join();
          }
          if (_processor_thread.joinable()){
            _processor_thread.join();
          }
          if (_listener_sock.has_value()) {
            _listener_sock->close();
            _listener_sock.reset();
          }
          std::cout << "server stopped" << std::endl;
        }

        inline std::size_t get_client_count() {
          std::lock_guard<std::mutex> lock(_mutex);
          return _clients.size();
        }
    };
}

