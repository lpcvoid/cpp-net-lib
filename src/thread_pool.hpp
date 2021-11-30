#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <vector>
namespace netlib {
class thread_pool {
private:
    std::vector<std::thread> _thread_pool;
    std::condition_variable _cv_new_job;
    std::mutex _mutex;
    std::queue<std::function<void()>> _task_queue;
    std::atomic<bool> _active = false;
    std::size_t _max_threads = 1;
    void add_thread()
    {
        auto thread_worker_fun = [this]() {
            while (_active) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _cv_new_job.wait(lock, [this] {
                        // we wait either until there is at least one task
                        // in the queue, or until we shutdown this thing
                        return (!_task_queue.empty()) || (!_active);
                    });

                    if (!_active) {
                        break; // terminate thread
                    }

                    task = std::move(_task_queue.front());
                    _task_queue.pop();
                }
                // actually execute the task
                task();
            }
        };
        _thread_pool.emplace_back(std::thread(thread_worker_fun));
    }
    void create_threads(std::size_t thread_count)
    {
        _thread_pool.reserve(thread_count + _thread_pool.size());
        for (uint32_t i = 0; i < thread_count; ++i) {
            add_thread();
        }
    }

private:
    thread_pool(std::size_t start_threads, std::size_t max_threads)
    {
        _active = true;
        _max_threads = max_threads;
        create_threads(start_threads);
    }

public:
    thread_pool()
    {
        _active = true;
        _max_threads = std::thread::hardware_concurrency();
        create_threads(1);
    }

    // only way to influence the max threads is via template
    // instantiation, since we can assert correctness at compile time
    template <typename std::size_t StartT, typename std::size_t MaxT> static thread_pool create()
    {
        static_assert(MaxT > 0, "Max threads shall be positive");
        static_assert((StartT <= MaxT), "Max threads shall be higher or equal to the starting thread count");
        return {StartT, MaxT};
    }

    ~thread_pool()
    {
        _active = false;
        _cv_new_job.notify_all();
        for (auto &thread : _thread_pool) {
            thread.join();
        }
    }

    std::size_t get_thread_count()
    {
        return _thread_pool.size();
    }
    std::size_t get_task_count()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _task_queue.size();
    }
    // https://stackoverflow.com/a/31078143
    template <typename FUNCTION, typename... FUNCARGS> auto add_task(FUNCTION &&function, FUNCARGS &&...args)
    {
        // first, check if we even have a free thread
        // if not, we add one up to a max allowed
        std::size_t free_threads = (get_thread_count() - get_task_count());
        if (!free_threads) {
            if (_thread_pool.size() < _max_threads) {
                create_threads(1);
            }
        }
        // we need the return type of the task we are given (C++17)
        using return_type = std::invoke_result_t<FUNCTION, FUNCARGS...>;
        // crate ptr which we then use to get a future later
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            // execute function with provided arg reference via forward call wrapper
            std::bind(std::forward<FUNCTION>(function), std::forward<FUNCARGS>(args)...));

        {
            std::unique_lock<std::mutex> lock(_mutex);
            // we actually emplace a lambda which then executes the function with args
            // we got passed here. Function is encoded in the std::packaged_task
            _task_queue.emplace([task]() -> void {
                // deref shared_ptr and execute function
                (*task)();
            });
        }
        // notify a waiting thread that there's a new task
        _cv_new_job.notify_one();
        // call get_future of the packaged_task
        return task->get_future();
    }
};
} // namespace netlib
