
#include "../src/thread_pool.hpp"
#include <cstdint>
#include <iostream>
#include <optional>
#include <future>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
  netlib::thread_pool thread_pool;

  auto do_something = [&](uint32_t some_var) -> std::string {
    std::stringstream ss;
    ss << std::this_thread::get_id() << "," << some_var;
    //simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds (100));
    return ss.str();
  };

  std::vector<std::future<std::string>> futures;
  for (int i = 0; i < 1000; ++i) {
    futures.emplace_back(thread_pool.add_task(do_something, i));
  }

  uint32_t index = 0;
  while (index != futures.size()) {
    for (auto& fut : futures) {
      if (fut.valid()) {
        std::future_status fs = fut.wait_for(std::chrono::microseconds (50000));
        if (fs == std::future_status::ready) {
          std::cout << index << " finished, latest: " << fut.get() << std::endl;
          index++;
        }
      }
    }
    std::cout << "Looped, tasks remaining: " << thread_pool.get_task_count() << std::endl;
  }
}