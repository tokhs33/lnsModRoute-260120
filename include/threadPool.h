#ifndef _INC_THREAD_POOL_HDR
#define _INC_THREAD_POOL_HDR

#include <vector>
#include <deque>
#include <queue>
#include <thread>
#include <future>
#include <functional>
#include <stdexcept>
#include <mutex>
#include <condition_variable>
#include <type_traits>

class CThreadPool {
public:
    CThreadPool(size_t numThread);
    ~CThreadPool();

    // 작업 제출을 위한 템플릿 함수
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using returnType = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<returnType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<returnType> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex);

            // 작업 큐에 추가합니다.
            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

private:
    // 워커 스레드를 유지하는 벡터
    std::vector<std::thread> workers;
    // 작업 큐
    std::queue<std::function<void()>> tasks;
    // 동기화를 위한 멤버
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};

template<class T>
class CTaskQueue {
public:
    CTaskQueue() {}
    ~CTaskQueue() {}

    void push(T task) {
        std::lock_guard<std::mutex> lock(queueMutex);
        tasks.push_back(task);
    }

    T pop() {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (tasks.empty()) {
            return nullptr;
        }
        T task = tasks.front();
        tasks.pop_front();
        return task;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(queueMutex);
        return tasks.empty();
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(queueMutex);
        return tasks.size();
    }

private:
    std::deque<T> tasks;
    std::mutex queueMutex;
};

#endif // _INC_THREAD_POOL_HDR
