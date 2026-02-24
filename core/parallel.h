// avs_lib - Portable Advanced Visualization Studio library
// NOT part of original AVS - parallel rendering utility
// Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace avs {

// Persistent thread pool for parallel row processing.
// Created once on first use, lives for program lifetime.
// Workers sleep on a condition variable when idle (zero CPU).
class ThreadPool {
public:
    static ThreadPool& instance() {
        static ThreadPool pool;
        return pool;
    }

    // Split [0, height) into strips and dispatch across threads.
    // Blocks until all strips are complete.
    void for_rows(int height, const std::function<void(int, int)>& func) {
        if (height <= 0) return;

        int n = num_threads_;
        if (n <= 1 || height < n) {
            // Single-threaded: just call directly
            func(0, height);
            return;
        }

        // Set up work for this dispatch
        {
            std::lock_guard<std::mutex> lock(mutex_);
            func_ = &func;
            height_ = height;
            strips_ = n;
            pending_.store(n, std::memory_order_release);
            generation_++;
        }

        // Wake all workers
        work_ready_.notify_all();

        // Wait for all strips to complete
        {
            std::unique_lock<std::mutex> lock(done_mutex_);
            done_cv_.wait(lock, [this] {
                return pending_.load(std::memory_order_acquire) == 0;
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        work_ready_.notify_all();
        for (auto& t : threads_) {
            if (t.joinable()) t.join();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    ThreadPool() {
        unsigned hw = std::thread::hardware_concurrency();
        num_threads_ = (hw > 1) ? static_cast<int>(hw) : 1;

        if (num_threads_ <= 1) return;  // No threads needed

        for (int i = 0; i < num_threads_; i++) {
            threads_.emplace_back([this, i] { worker(i); });
        }
    }

    void worker(int thread_id) {
        uint64_t my_gen = 0;
        while (true) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                work_ready_.wait(lock, [this, &my_gen] {
                    return shutdown_ || generation_ > my_gen;
                });
                if (shutdown_) return;
                my_gen = generation_;
            }

            // Compute this thread's strip
            int n = strips_;
            int h = height_;
            int y_start = (thread_id * h) / n;
            int y_end = ((thread_id + 1) * h) / n;

            if (y_start < y_end) {
                (*func_)(y_start, y_end);
            }

            // Signal completion
            if (pending_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                // Last thread done — wake the caller.
                // Must hold done_mutex_ to prevent lost wake-up:
                // without it, notify can fire between the caller's
                // predicate check and its entry into wait().
                std::lock_guard<std::mutex> lock(done_mutex_);
                done_cv_.notify_one();
            }
        }
    }

    int num_threads_ = 1;
    std::vector<std::thread> threads_;

    std::mutex mutex_;
    std::condition_variable work_ready_;

    std::mutex done_mutex_;
    std::condition_variable done_cv_;

    // Per-dispatch state (protected by mutex_ for setup, then read-only during execution)
    const std::function<void(int, int)>* func_ = nullptr;
    int height_ = 0;
    int strips_ = 0;
    std::atomic<int> pending_{0};
    uint64_t generation_ = 0;
    bool shutdown_ = false;
};

// Convenience function: split [0, height) across threads.
// func(y_start, y_end) is called for each strip.
inline void parallel_for_rows(int height, const std::function<void(int, int)>& func) {
    ThreadPool::instance().for_rows(height, func);
}

} // namespace avs
