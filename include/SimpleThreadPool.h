#pragma once

#include <mutex>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <condition_variable>


// This implementation assumes that only 1 thread "owns" the pool
// So, only the thread that `Start`-ed the pool is allowed to enqueue new jobs, wait for completion or shut it down

// This assumption is respected by the Worker code because only the Receive/Send thread interacts with the pool

class SimpleThreadPool
{
public:
    SimpleThreadPool();
    SimpleThreadPool(int numOfThreads);
    ~SimpleThreadPool();

    bool Start(int numOfThreads);
    bool ShutDown();

    bool AddJob(std::function<void()> func);
    void WaitForJobsToComplete();

private:
    void Executor();


    std::vector<std::thread> _threads;
    std::queue<std::function<void()>> _queueJobs;
    std::mutex _queueMutex;
    std::condition_variable _queueCondVar;
    std::condition_variable _finishJobsCondVar;

    bool _shutDown;
};
