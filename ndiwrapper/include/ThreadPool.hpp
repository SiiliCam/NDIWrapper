#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#pragma once
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();

    void enqueue(std::function<void()> job);

private:
    // Worker function for each thread
    void worker();

    // Number of worker threads
    size_t numThreads_;

    // Task queue
    std::queue<std::function<void()>> jobs_;

    // Thread pool
    std::vector<std::thread> threads_;

    // Mutex for synchronizing access to jobs_
    std::mutex mutex_;

    // Condition variable to notify worker threads
    std::condition_variable cv_;

    // Flag to indicate whether the thread pool is stopping
    bool stop_;
};