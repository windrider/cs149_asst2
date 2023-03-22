#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>

const int max_thread_num=8;
enum BulkState {unpushed,pushed,finished};

struct MyTask;

struct TaskBulk{ 
    volatile int tasks_to_finish;
    std::vector<MyTask*> taskVec;
    std::vector<TaskID> deps;
    BulkState state;
    TaskBulk(int total_num, const std::vector<TaskID>& d):tasks_to_finish(total_num),deps(d),state(unpushed){}
};

struct MyTask{
    IRunnable* runnable;
    int i;
    int num_total_tasks;
    TaskBulk* tb;
    MyTask(IRunnable* r,int id,int num):runnable(r),i(id),num_total_tasks(num),tb(nullptr){}
    MyTask(IRunnable* r,int id,int num,TaskBulk* p):runnable(r),i(id),num_total_tasks(num),tb{p}{}
};



/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial: public ITaskSystem {
    public:
        TaskSystemSerial(int num_threads);
        ~TaskSystemSerial();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn: public ITaskSystem {
    public:
        TaskSystemParallelSpawn(int num_threads);
        ~TaskSystemParallelSpawn();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
        void myworker(IRunnable* runnable,int i, int num_total_tasks);

        std::thread workThread[max_thread_num];
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSpinning(int num_threads);
        ~TaskSystemParallelThreadPoolSpinning();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
        void myworker();
        void bulkFetch();
        std::thread workThread[max_thread_num];

        std::queue<MyTask*> mworkqueue;
        std::vector<TaskBulk*> mBulkVec;
        std::mutex mlock;
        std::mutex block;
        std::condition_variable mcv;
        volatile bool stop;
        volatile int myfinish;
        volatile int bulks_to_finish;
        int numThreads;

};

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();

        void myworker();
        std::thread workThread[max_thread_num];

        std::queue<MyTask*> mworkqueue;
        std::mutex mlock;
        std::condition_variable mcv;
        bool stop;
        volatile int myfinish;
        int numThreads;
};

#endif
