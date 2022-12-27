#pragma once

#include "socket.hpp"
#include <array>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace netlib {
class operations {
private:
    static constexpr std::size_t MAX_CHUNK_SIZE = 4096;

public:

    static inline std::pair<std::size_t, std::error_condition> send(const netlib::socket &sock, const std::vector<uint8_t> &data,
                                                                    std::chrono::milliseconds timeout)
    {
        std::size_t total_sent_size = 0;
        const std::size_t chunk_size = (data.size() > MAX_CHUNK_SIZE) ? MAX_CHUNK_SIZE : data.size();
        const char *data_pointer = reinterpret_cast<const char *>(data.data());
        while (true) {
            auto wait_res = netlib::operations::wait_for_operation(sock.get_raw().value(), OperationClass::write, timeout);
            timeout -= wait_res.second;
            if (wait_res.first) {
                return {total_sent_size, wait_res.first};
            }

            std::size_t remaining_bytes = data.size() - total_sent_size;
            std::size_t actual_chunk_size = (remaining_bytes > chunk_size) ? chunk_size : remaining_bytes;
            ssize_t send_res = ::send(sock.get_raw().value(), data_pointer, static_cast<int32_t>(actual_chunk_size), MSG_NOSIGNAL);
            if (send_res > 0) {
                total_sent_size += static_cast<std::size_t>(send_res);
                data_pointer += send_res;
                if (total_sent_size == data.size()) {
                    return {static_cast<std::size_t>(total_sent_size), {}};
                }
            } else if (send_res == 0) {
                return {total_sent_size, std::errc::connection_aborted};
            } else {
                std::error_condition send_error = socket_get_last_error();
                if ((send_error != std::errc::resource_unavailable_try_again) && (send_error != std::errc::operation_would_block)) {
                    return {total_sent_size, send_error};
                }
            }
        }
    }

    static inline std::pair<std::vector<uint8_t>, std::error_condition> recv(const netlib::socket &sock, std::size_t byte_count)
    {
        const std::size_t read_chunk_size =
            (byte_count == 0) ? MAX_CHUNK_SIZE : (byte_count > MAX_CHUNK_SIZE ? MAX_CHUNK_SIZE : byte_count);
        std::size_t total_recv_size = 0;
        std::vector<uint8_t> data;
        std::array<uint8_t, MAX_CHUNK_SIZE> temp_recv_buffer{};
        while (true) {
            ssize_t recv_res = ::recv(sock.get_raw().value(), reinterpret_cast<char *>(temp_recv_buffer.data()), read_chunk_size, 0);
            if (recv_res > 0) {
                data.insert(data.end(), temp_recv_buffer.begin(), temp_recv_buffer.begin() + recv_res);
                total_recv_size += recv_res;
                if (total_recv_size == byte_count) {
                    data.resize(total_recv_size);
                    // we got exactly the amount of data that user wanted/expected
                    return {data, {}};
                }
            } else if (recv_res == 0) {
                data.resize(total_recv_size);
                return {data, std::errc::connection_aborted};
            } else if (recv_res < 0) {
                std::error_condition recv_error = socket_get_last_error();
                if ((recv_error == std::errc::resource_unavailable_try_again) || (recv_error == std::errc::operation_would_block)) {
                    // we got all pending data
                    data.resize(total_recv_size);
                    return {data, std::errc::operation_would_block};
                }
            }
        }
    }

    static inline std::pair<std::vector<uint8_t>, std::error_condition> recv(const netlib::socket &sock, std::size_t byte_count,
                                                                             std::chrono::milliseconds timeout)
    {

        // if user wants all data, we use max chunk size. Also, if he wants more
        // than chunk size, we limit it too.
        const std::size_t read_chunk_size =
            (byte_count == 0) ? MAX_CHUNK_SIZE : (byte_count > MAX_CHUNK_SIZE ? MAX_CHUNK_SIZE : byte_count);
        std::vector<uint8_t> data;
        while (true) {
            auto wait_res = wait_for_operation(sock.get_raw().value(), OperationClass::read, timeout);
            if (wait_res.first) {
                return {data, wait_res.first}; // return the data we have
            }
            timeout -= wait_res.second;
            auto recv_result = recv(sock, read_chunk_size);
            data.insert(data.end(), recv_result.first.begin(), recv_result.first.end());
            if (data.size() == byte_count) {
                // we got exactly the amount of data that user wanted/expected
                return {data, {}};
            }
            if (timeout.count() <= 0) {
                // we timed out
                return {data, std::errc::timed_out}; // timeout error
            }

            if (recv_result.second && recv_result.second == std::errc::connection_aborted) {
                return {data, std::errc::connection_aborted};
            }
        }
    }

    static inline std::pair<std::error_condition, std::chrono::milliseconds> wait_for_operation(socket_t sock, OperationClass op_class,
                                                                                                std::chrono::milliseconds timeout)
    {

        if (timeout.count() < 0) {
            return {std::errc::timed_out, std::chrono::milliseconds(0)};
        }

        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);
        timeval tv{.tv_sec = static_cast<int32_t>(timeout.count() / 1000),
                   .tv_usec = static_cast<int32_t>((timeout.count() % 1000) * 1000)};
        fd_set *fdset_ptr_read = ((op_class == OperationClass::read) || (op_class == OperationClass::both)) ? &fdset : nullptr;
        fd_set *fdset_ptr_write = ((op_class == OperationClass::write) || (op_class == OperationClass::both)) ? &fdset : nullptr;
        std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
        int32_t select_res = select(sock + 1, fdset_ptr_read, fdset_ptr_write, nullptr, &tv);
        std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();
        std::chrono::milliseconds ms_taken = std::chrono::duration_cast<std::chrono::milliseconds>((end - start));
        if (select_res == 1) {
            return {{}, ms_taken};
        } else if (select_res == 0) {
            // timeout
            return {std::errc::timed_out, ms_taken};
        } else {
            // error
            return {socket_get_last_error(), ms_taken};
        }
    }
};
} // namespace netlib