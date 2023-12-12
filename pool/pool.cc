#include "pool.h"
#include <queue>
#include <map>
#include <iostream>

using namespace std;
// All variables for each ThreadPool must be within the class NO GLOBAL DATA

/* High Level Overview:
*  There will be a list of predefined Tasks when the code is tested
*  1. When they test our code they will first run the ThreadPool constructor which will spin up N threads that will be used to process tasks
*  2. When SubmitTask is called they will provide the Task to be queued and its name. We will enqueue the task, if queue is full double size, also create CV for task and add name to CV value to mapping 
*  3. WaitForTask will wait for a particular task and not return until that task has been run (assume WFT called once per task). By having a CV for each task we can implement something similar to
*     the monitor pattern seen on Slide 5 in lecture 14. Essentially we will wait for the condition to be finished inside a mutex and if the CV changes then we will broadcast that tasks name is done
*  4. Stop the thread pool. If thread is in the middle of running a task wait for it to finish before interrupting. On Piazza it notes the best way to do this is to have a boolean flag "stopCalled" 
*     that communicates to the worker threads when to stop running tasks. Essentially pointer to flag to each thread on creation, when stop called set it to True, and before each thread processes
*     a new task from the queue it should check this boolean to see if it needs to stop. AFTER done we need to de-allocate the thread pool.
*/

// function so that we can check the stopCalled flag inside the readers while inside of a mutex
bool check_stop(ThreadPool* arg){
    bool stop_condition;
    pthread_mutex_lock(&arg->stop_lock);
    stop_condition = arg->stopCalled;
    pthread_mutex_unlock(&arg->stop_lock);
    return stop_condition;
}

// the thread function should get any task (if there is any) out of the Task Queue and run it.
void* run_task(void* arg){
    // call the run function that the task is processing
    ThreadPool* current_threadpool = (ThreadPool*) arg;

    while(!check_stop(current_threadpool)){
        //init current task as nullptr and clear it later, this way it's initialized in a separate line
        Task* current_task = nullptr;{
            pthread_mutex_lock(&current_threadpool->queue_lock);
            //if threadpool not empty, set task as front item in queue
            if (!current_threadpool->task_queue.empty()) {
                current_task = current_threadpool->task_queue.front();
                current_threadpool->task_queue.pop();
            }
            pthread_mutex_unlock(&current_threadpool->queue_lock);
        }

        if (current_task) {
            current_task->Run();
            //set standin string to help w passing through to map
            std::string taskname = current_task->taskname;
            //delete here bc otherwise it can get wrapped up, string now contains name
            delete current_task;

            pthread_mutex_lock(&current_threadpool->map_lock);
            current_threadpool->taskname_to_status[taskname] = 1; //store 1 in map here
            pthread_cond_signal(&(current_threadpool->taskname_to_condvar[taskname]));
            pthread_mutex_unlock(&current_threadpool->map_lock);
        }
    }

    // can now use current_threadpool as a pointer to the threadpool class we just defined to get the variables needed (taskqueue, nameCVmap, flags)
    return 0;
}

Task::Task() {

}

Task::~Task() {

}

// Constructor (set up a ThreadPool with N Threads) | Each thread should have the same data (waitQ, stopCalled flag, potential mapping from name to CV table)
ThreadPool::ThreadPool(int num_threads) { 
    //initialize all threadpool locks
    pthread_mutex_init(&queue_lock, NULL);
    pthread_mutex_init(&map_lock, NULL);
    pthread_mutex_init(&stop_lock, NULL);
    // create an array of the size of the number of threads that will be needed
    threads = (pthread_t*)malloc(num_threads*sizeof(pthread_t));
    number_threads = num_threads;
    // create N threads to run tasks. Each thread gets a pointer to thread args, which is defined in pool.h. 
    // each thread should call the "run_task" function, and each thread needs to be able to touch the waitQueue
    for(int i=0; i < num_threads; i++){
        // format for pthread_create(thread, NULL, function_it_calls, arguments_it_gets)
        pthread_create(&threads[i], NULL, run_task, this); 
    }

}

// producer --> adds task to queue of tasks
void ThreadPool::SubmitTask(const std::string &name, Task* task) {
    // add Task to queue in threadpool | use mutex when enqueue-ing and dequeue-ing | also create and insert name->condvar mapping
    // adding stuff to the queue needs to be wrapped in a mutex
    pthread_mutex_lock(&this->queue_lock);
    // set the task name to be the name SubmitTask gives it
    task->taskname = name;

    // enqueue the Task inside of the ThreadPool's task_queue
    task_queue.push(task);

    // insert name to condvar mapping into the map
    pthread_mutex_lock(&this->map_lock);
    // create and initialize a new condition variable for the given task
    pthread_cond_t new_condvar;
    pthread_cond_init(&new_condvar, NULL);

    // add it to a map of task names to condition variables
    taskname_to_condvar.insert({name, new_condvar});
    // initialize the status for the task to 0 (in queue not run yet)
    int status = 0;
    taskname_to_status.insert({name, status});

    pthread_mutex_unlock(&this->map_lock);

    pthread_mutex_unlock(&this->queue_lock);
}

// wait for task, return when task complete
void ThreadPool::WaitForTask(const std::string &name) {
    pthread_mutex_lock(&this->map_lock);
    // if the task has not finished running therefore wait for it to finish
    while(this->taskname_to_status.at(name) == 0){
        pthread_cond_wait(&(this->taskname_to_condvar.at(name)), &this->map_lock);
    }
    // if we make it here we know that the status is 1, so thread has already ran (remove entry from both maps)
    taskname_to_condvar.erase(name);
    taskname_to_status.erase(name);
    
    pthread_mutex_unlock(&this->map_lock);
}

void ThreadPool::Stop() {
    // if anything running, wait for finish, look at map and make sure there are no 1s, once done
    // set the boolean flag to true so the workers know to stop running
    
    // this is one of the data races (we have a thread writing and a potential of one reading in run_task)
    pthread_mutex_lock(&this->stop_lock);
    this->stopCalled = true;
    pthread_mutex_unlock(&this->stop_lock);
    for(int i = 0; i < number_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    // free the threads and destroy the mutexes we made
    free(threads);
    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&map_lock);
    pthread_mutex_destroy(&stop_lock);
}