#include <threadPool.h>

CThreadPool::CThreadPool(size_t numThread) : stop(false)
{
    for (size_t i = 0; i < numThread; ++i) {
        workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                    if (this->stop && this->tasks.empty()) {
                        return;
                    }
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}

CThreadPool::~CThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread& worker : workers) {
        worker.join();
    }
}
