# Stanford CS149 asst2

​	实现一个**Task Execusion Library**。

​    这是一个在能在多核CPU上多线程并行执行任务C++库。

## Part A: Synchronous Bulk Task Launch

​	实现同步版本的**Task Execusion Library**。

​	该库的虚函数接口已经给出，并且已经给出顺序执行各任务的代码，要求实验者使用多线程和线程池的方法实现**TaskSystem类**。

#### 多线程

​	默认库的使用者已经实现了自己的**runnable->runTask**函数，所以只需重载**run**函数将其改为多线程的版本，并编写一个**myworker**函数用于执行实际的task

```c++
oid TaskSystemParallelSpawn::myworker(IRunnable* runnable,int i, int num_total_tasks){
    runnable->runTask(i,num_total_tasks);
}
void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    std::thread mythread[max_thread_num];
    for(int i=0;i<num_total_tasks;i++){
        mythread[i]=std::thread(&TaskSystemParallelSpawn::myworker,this,runnable,i,num_total_tasks);
    }
    for(int i=0;i<num_total_tasks;i++){
            mythread[i].join();
    }    
    return;
```



#### 线程池(sleeping)

   **MyTask**类型表示一个任务

```c++
struct MyTask{
    IRunnable* runnable;     //由线程池的使用者实现的，实际执行任务的类
    int i;                   //task ID
    int num_total_tasks;     //任务总数
    TaskBulk* tb;
    MyTask(IRunnable* r,int id,int num):runnable(r),i(id),num_total_tasks(num),tb(nullptr){}
    //MyTask(IRunnable* r,int id,int num,TaskBulk* p):runnable(r),i(id),num_total_tasks(num),tb{p}{}
};
```

​	在该类中添加一些必须的成员变量

```c++
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
        std::thread workThread[max_thread_num];

        std::queue<MyTask*> mworkqueue;  //任务队列
        //std::vector<TaskBulk*> mBulkVec;
        std::mutex mlock;				 //保护任务队列的锁
        std::condition_variable mcv;     //检查任务队列是否为空的条件变量
        bool stop;						 //线程池是否停止
        volatile int myfinish;           //已经执行完的任务数量
        //volatile int bulks_to_finish;
        int numThreads;					 //线程数

};
```

编写线程函数myworker，一直查询任务队列中是否有任务可执行，并在执行完任务后用原子操作使myfinish+1

```c++
void TaskSystemParallelThreadPoolSpinning::myworker(){
    while(!stop){
        std::unique_lock<std::mutex> unique(mlock);
        mcv.wait(unique,[&](){return !mworkqueue.empty()||stop;});
        if(stop)break;
        MyTask* mtask=mworkqueue.front();
        mworkqueue.pop();
        mtask->runnable->runTask(mtask->i,mtask->num_total_tasks);
		__sync_bool_compare_and_swap(&myfinish,myfinish,myfinish+1);
            }
        }
    }
}
```

构造函数，初始化多个工作线程开始执行：

```c++
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
```

   run函数，把使用者发来的任务都塞进任务队列，唤醒所有的工作线程，由于是同步线程池，要等到所有的任务都执行完后才返回

```c++
void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    std::unique_lock<std::mutex> unique(mlock);
    for(int i=0;i<num_total_tasks;i++){        
        mworkqueue.push(new MyTask(runnable,i,num_total_tasks));      
    }
    unique.unlock();
    mcv.notify_all();
    while(myfinish!=num_total_tasks){
        ;
    }
    myfinish=0;
}

```

析构函数，唤醒并终止所有线程

```c++
TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    stop=true;
    mcv.notify_all();
    for(int i=0;i<numThreads;i++){
        workThread[i].join();
    }
}
```

测试一下，多线程和线程池运行结果正确。

![25e1d7ddf5b73642a5f211f4fd69bd8](graphs/25e1d7ddf5b73642a5f211f4fd69bd8.png)

## Part B: Supporting Execution of Task Graphs

​	为线程池添加异步运行功能。

