#include "tasksys.h"
#include <iostream>
#include <exception>
#include <atomic>
#include <unistd.h>


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
    return 0;
}

void TaskSystemSerial::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //


}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::myworker(IRunnable* runnable,int i, int num_total_tasks){
    runnable->runTask(i,num_total_tasks);
}
void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    //printf("run begin\n");
    std::thread mythread[max_thread_num];
    for(int i=0;i<num_total_tasks;i++){
        //workThread[i]=std::thread(&TaskSystemParallelSpawn::myworker,this,runnable,i,num_total_tasks);
        //std::thread t(&TaskSystemParallelSpawn::myworker,this,runnable,i,num_total_tasks);
        mythread[i]=std::thread(&TaskSystemParallelSpawn::myworker,this,runnable,i,num_total_tasks);
        //printf("run 1\n");
    }
    //printf("ok\n");
    

    
    for(int i=0;i<num_total_tasks;i++){
        //printf("join begin\n");
        //if(workThread[i].joinable())
            mythread[i].join();
    }
    
    
    return;
    

    
    /*
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
    */
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
     for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}
void TaskSystemParallelThreadPoolSpinning::myworker(){
    while(!stop){
        std::unique_lock<std::mutex> unique(mlock);
        //printf("wait before\n");
        mcv.wait(unique,[&](){return !mworkqueue.empty()||stop;});
        if(stop)break;
        MyTask* mtask=mworkqueue.front();
        mworkqueue.pop();
        mtask->runnable->runTask(mtask->i,mtask->num_total_tasks);
        if(mtask->tb==nullptr)__sync_bool_compare_and_swap(&myfinish,myfinish,myfinish+1);
        else{
            mtask->tb->tasks_to_finish--;
            if(mtask->tb->tasks_to_finish==0)
            {__sync_bool_compare_and_swap(&bulks_to_finish,bulks_to_finish,bulks_to_finish-1);
                printf("bulk finish\n");
            }
        }
        //printf("work finish %d\n",myfinish);
        //mcv.notify_all();
    }
    //printf("thread over\n");
}


TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    stop=false;
    myfinish=0;
    numThreads=num_threads;
    bulks_to_finish=0;
    for(int i=0;i<num_threads;i++){
        workThread[i]=std::thread(&TaskSystemParallelThreadPoolSpinning::myworker,this);
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    //printf("dtor\n");
    //std::unique_lock<std::mutex> unique(mlock);
    stop=true;
    //unique.unlock();
    mcv.notify_all();
    for(int i=0;i<numThreads;i++){
        workThread[i].join();
    }
    //printf("pool stop\n");
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    //printf("%d\n",num_total_tasks);
    std::unique_lock<std::mutex> unique(mlock);
    for(int i=0;i<num_total_tasks;i++){
        
        mworkqueue.push(new MyTask(runnable,i,num_total_tasks));
       
        
    }
    unique.unlock();
    mcv.notify_all();
    //printf("wait myfinish\n");
    while(myfinish!=num_total_tasks){
        ;
    }
    myfinish=0;
    //printf("myfinish\n");
    /*
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
    */
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    __sync_bool_compare_and_swap(&(this->bulks_to_finish),bulks_to_finish,bulks_to_finish+1);
    TaskBulk* tb=new TaskBulk(num_total_tasks);
    int taskID=mBulkVec.size();
    mBulkVec.push_back(tb);

    /*
    auto mydeps=deps;
    while(!mydeps.empty()){
        for(auto it=mydeps.begin();it!=mydeps.end();++it){
            int bulkID=*it;
            if(mBulkVec[bulkID]->tasks_to_finish==0){
                mydeps.erase(it);
            }
        }
    }
    */

    std::unique_lock<std::mutex> unique(mlock);
    for(int i=0;i<num_total_tasks;i++){
        mworkqueue.push(new MyTask(runnable,i,num_total_tasks,tb));       
    }
    unique.unlock();
    mcv.notify_all();
    printf("runAsync\n");

    return taskID;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    while(bulks_to_finish!=0){
        ;
    }
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

void TaskSystemParallelThreadPoolSleeping::myworker(){
    
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
