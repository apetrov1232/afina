#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <chrono>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

    private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Vector of actual threads that perorm execution
     */
    size_t cnt_threads = 0;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;

    size_t low_watermark, hight_watermark, max_queue_size, idle_time;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    void perform(){
        while(1){
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex);
                while ((tasks.size()==0) && (state == State::kRun)){
                    auto wait_res = empty_condition.wait_for(lock, std::chrono::milliseconds(idle_time));
                    if ((wait_res == std::cv_status::timeout) && (cnt_threads > low_watermark)){ 
                        cnt_threads--;
                        return;
                    }
                }
                if ((state != State::kRun) && (tasks.size()==0)){
                    cnt_threads--;
                    if (cnt_threads == 0){
                        state = State::kStopped;
                        empty_condition.notify_one();
                    }
                    return;
                }
                task = std::move(tasks.front());
                tasks.pop_front();
            }
            task();
        }
    };

    public:

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */

    void Stop(bool await = false){
        std::unique_lock<std::mutex> lock(this->mutex);
        if (cnt_threads==0)
            state = State::kStopped;
        else
            state = State::kStopping;

        if (await){
            while (cnt_threads != 0)
                empty_condition.wait(lock);
        }
    };

    Executor(std::string name="", size_t low_watermark=1, size_t hight_watermark=2, 
            size_t max_queue_size=10, size_t idle_time=100) : low_watermark(low_watermark),
            hight_watermark(hight_watermark), max_queue_size(max_queue_size),
            idle_time(idle_time){};
    ~Executor(){
        Stop(true);
    }
    
    void Start(){
        std::lock_guard<std::mutex> lock(mutex);
        state = State::kRun;
        for (int i = 0; i < cnt_threads; i++)
            std::thread(&Executor::perform, this).detach();
    };

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::lock_guard<std::mutex> lock(this->mutex);
        if ((state != State::kRun) || (tasks.size() == max_queue_size))
            return false;
        

        if ((tasks.size() == cnt_threads) && (cnt_threads < hight_watermark)){
            std::thread(&Executor::perform, this).detach();
            cnt_threads++;
        }
        

        // Enqueue new task
        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
    }
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
