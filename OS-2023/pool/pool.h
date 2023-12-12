#ifndef POOL_H_
#include <string>
#include <pthread.h>
#include <queue>
#include <map>

void* run_task(void* arg);

class Task {
public:
    // when the Task gets assigned a name hold it here
    std::string taskname;

    // default functions given to us for task
    Task();
    virtual ~Task();
    virtual void Run() = 0;  // implemented by subclass
};

class ThreadPool {
public:
    // function pointer passed to pthread_create can take "this" as an argument - https://stackoverflow.com/questions/1151582/pthread-function-from-a-class?noredirect=1&lq=1
    // which is a pointer to the object being constructed. Now you have a pointer to the object and thus a way to get at the data fieldds of the object.
    std::queue<Task*> task_queue;
    bool stopCalled = false;
    // map the task name to its condition variable, this will be useful in WaitForTask()
    std::map<std::string, pthread_cond_t> taskname_to_condvar;
    // 0 will be in queue not run / 1 will be finished running
    std::map<std::string, int> taskname_to_status;

    // the threads which will be in the threadpool
    pthread_t *threads;
    int number_threads;

    // (mutex) lock for accessing the queue and the map
    pthread_mutex_t queue_lock;
    pthread_mutex_t map_lock;
    pthread_mutex_t stop_lock;

    ThreadPool(int num_threads);
    // Submit a task with a particular name.
    void SubmitTask(const std::string &name, Task *task);
 
    // Wait for a task by name, if it hasn't been waited for yet. Only returns after the task is completed.
    void WaitForTask(const std::string &name);

    // Stop all threads. All tasks must have been waited for before calling this.
    // You may assume that SubmitTask() is not caled after this is called.
    void Stop();
};
#endif
