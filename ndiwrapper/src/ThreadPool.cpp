#include "ThreadPool.hpp"

// Constructor: spawn `numThreads` worker threads
ThreadPool::ThreadPool(size_t numThreads) : numThreads_(numThreads), stop_(false) {
    for (size_t i = 0; i < numThreads_; ++i) {
        threads_.emplace_back(&ThreadPool::worker, this);
    }
}

// Destructor: join all threads after setting the stopping flag
ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (std::thread& thread : threads_) {
        thread.join();
    }
}

// Add a new job to the queue
void ThreadPool::enqueue(std::function<void()> job) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        jobs_.push(std::move(job));
    }
    cv_.notify_one();
}

// The worker function to run in each thread
void ThreadPool::worker() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !jobs_.empty() || stop_; });
            if (stop_ && jobs_.empty()) return;
            job = std::move(jobs_.front());
            jobs_.pop();
        }
        job();
    }
}