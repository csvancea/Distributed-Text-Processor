#include "SimpleThreadPool.h"


SimpleThreadPool::SimpleThreadPool() : _shutDown(true)
{

}

SimpleThreadPool::SimpleThreadPool(int numOfThreads) : _shutDown(true)
{
    Start(numOfThreads);
}

SimpleThreadPool::~SimpleThreadPool()
{
    ShutDown();
}

bool SimpleThreadPool::Start(int numOfThreads)
{
    if (!_shutDown) {
        return false;
    }

    _threads.resize(numOfThreads);
    _shutDown = false;

    for (auto& thread : _threads) {
        thread = std::thread(&SimpleThreadPool::Executor, this);
    }

    return true;
}

bool SimpleThreadPool::ShutDown()
{
    if (_shutDown) {
        return false;
    }

    _shutDown = true;
    _queueCondVar.notify_all();

    for (auto& thread : _threads) {
        thread.join();
    }

    while (!_queueJobs.empty()) {
        _queueJobs.pop();
    }

    return true;
}

bool SimpleThreadPool::AddJob(std::function<void()> func) 
{
    std::unique_lock<std::mutex> lock(_queueMutex);

    if (_shutDown) {
        return false;
    }

    _queueJobs.push(func);
    _queueCondVar.notify_one();
    return true;
}

void SimpleThreadPool::WaitForJobsToComplete()
{
    std::unique_lock<std::mutex> lock(_queueMutex);

    if (_shutDown) {
        return;
    }

    _finishJobsCondVar.wait(lock, [this]() { return _queueJobs.empty(); });

    // * It doesn't make sense to wait on _shutDown == 1 here because ShutDown()/Destructor can only be called from the main thread (convention)
    // * WaitForJobsToComplete is a blocking function that runs on the main thread
    // Thus => no deadlock
}

void SimpleThreadPool::Executor()
{
    while (!_shutDown) {
        std::function<void()> func;

        {
            std::unique_lock<std::mutex> lock(_queueMutex);
            _queueCondVar.wait(lock, [this]() { return _shutDown || !_queueJobs.empty(); });

            if (_shutDown) {
                break;
            }

            func = _queueJobs.front();
            _queueJobs.pop();
        }

        func();
        _finishJobsCondVar.notify_one();
    }
}
