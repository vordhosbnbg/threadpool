#pragma once
#include <thread>
#include <vector>
#include <future>
#include <functional>
#include <condition_variable>
#include <queue>
#include <utility>
#include <atomic>
#include <chrono>

class ThreadPool
{
public:
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) : nbThreads(numThreads)
    {
    }

    ~ThreadPool()
    {
        waitUntilIdle();
    }


    void start()
    {
        for(size_t i=0; i < nbThreads; ++i)
        {
            threads.emplace_back(&ThreadPool::worker, this);
        }
    }

    void waitUntilIdle()
    {
        acceptsWork = false;
        for(std::thread& thread : threads)
        {
            thread.join();
        }
        threads.clear();
        acceptsWork = true;
    }

    void worker()
    {
        while(true)
        {
            std::function<void()> operation;
            {
                using namespace std::chrono_literals;

                std::unique_lock<std::mutex> lock(mx);
                cv.wait_for(lock, 10ms, [this]()
                {
                    return !queue.empty() || !acceptsWork;
                });
                if(!queue.empty())
                {
                    operation = queue.front();
                    queue.pop();
                }
                else if(!acceptsWork)
                {
                    return;
                }
            }
            if(operation)
                operation();
        }
    }

    void push(std::function<void()>&& f)
    {
        if(acceptsWork)
        {
            {
                std::unique_lock<std::mutex> lock(mx);
                queue.push(f);
            }
            cv.notify_one();
        }
    }


private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> queue;
    std::condition_variable cv;
    std::mutex mx;
    std::atomic<bool> acceptsWork{true};
    size_t nbThreads;
};
