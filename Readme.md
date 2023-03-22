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
    stop = true;
    mcv.notify_all();
    for(int i = 0 ; i < numThreads; i++){
        workThread[i].join();
    }
}
```

测试一下，多线程和线程池运行结果正确。

![25e1d7ddf5b73642a5f211f4fd69bd8](graphs/25e1d7ddf5b73642a5f211f4fd69bd8.png)

## Part B: Supporting Execution of Task Graphs

​	为线程池添加异步运行功能。

<p align="center">
    <img src="figs/task_graph.png" width=400>
</p>

```c++
virtual TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps) = 0;
virtual void sync() = 0;
```

​	要求实现**runAsyncWithDeps**和**sync**两个接口。



**runAsyncWithDeps**：

​	一个 taskBulk (例如图中的**launchD**)可以包含多个task，但是这些 task 都依赖于其它的 taskBulk（记录在 **const std::vector<TaskID>& deps**中），要等依赖的 taskBulk 全都执行完，才能执行本taskBulk中的task。要求该接口调用后立刻返回，不必等 task 都执行完。该接口返回一个TaskID。



**sync**:

​	阻塞直到该线程池中所有task全部执行完后才返回。



运行该异步线程池的示例代码：

```C++
// assume taskA and taskB are valid instances of IRunnable...

std::vector<TaskID> noDeps;  // empty vector

ITaskSystem *t = new TaskSystem(num_threads);

// bulk launch of 4 tasks
TaskID launchA = t->runAsyncWithDeps(taskA, 4, noDeps);

// bulk launch of 8 tasks
TaskID launchB = t->runAsyncWithDeps(taskB, 8, noDeps);

// at this point tasks associated with launchA and launchB
// may still be running

t->sync();

// at this point all 12 tasks associated with launchA and launchB
// are guaranteed to have terminated
```



​	实现方案：

用一个结构体TaskBulk存储某次launch的所有task

```c++
/*
TaskBulk有三种状态：
unpushed: 由于deps中的TaskBulk没有全部finish，还不能将taskVec中的task放入线程池的任务队列
pushed: 已经将taskVec中的所有task放入线程池的任务队列
finished: 本次launch的所有task全部完成
*/
enum BulkState {unpushed,pushed,finished};

struct MyTask;

struct TaskBulk{ 
    volatile int tasks_to_finish; //本次launch未完成的task数量，为0时将state改为finished
    std::vector<MyTask*> taskVec; //存储本次launch的所有task
    std::vector<TaskID> deps;	//依赖的之前的TaskBulks
    BulkState state;
    TaskBulk(int total_num, const std::vector<TaskID>& d):tasks_to_finish(total_num),deps(d),state(unpushed){}
};
```



**runAsyncWithDeps**的实现：

```c++
TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    
    __sync_bool_compare_and_swap(&(this->bulks_to_finish),bulks_to_finish,bulks_to_finish+1);// 需要完成的taskBulk数量+1
    /*
    下面仅仅是实例化了一个TaskBulk结构体记录这个Taskbulk,将其放入线程池的mBulkVec中，之后就返回，留给之后的线程函数bulkFetch处理
    */
    TaskBulk* tb = new TaskBulk(num_total_tasks,deps);
    for(int i = 0; i < num_total_tasks; i++){
        tb->taskVec.push_back(new MyTask(runnable, i, num_total_tasks, tb));       
    }
    block.lock();
    int taskID = mBulkVec.size();
    mBulkVec.push_back(tb); //线程池中存储所有taskBulk的一个vector
    block.unlock();
    mcv.notify_all();

    return taskID;
}
```



线程池额外启动一个线程执行**bulkFetch**函数，作用不断轮询**mBulkVec**中有没有达到启动条件的taskBulk，若有的话将其中所有task放入线程池的任务队列**mworkqueue**中 (自我感觉这里加锁处理得比较粗糙)

```C++
void TaskSystemParallelThreadPoolSpinning::bulkFetch(){
    while(!stop){
        block.lock();
        for(auto it = mBulkVec.begin(); it != mBulkVec.end(); it++){
            TaskBulk* tb = *it;
            if(tb->state==unpushed){
                bool mypush=true;
                for(unsigned int j = 0; j < tb->deps.size(); ++j){
                    TaskID bulkid=tb->deps[j];
                    if( (unsigned int)bulkid >= mBulkVec.size() || mBulkVec[bulkid]->state != finished){
                        mypush=false;
                        break;
                    }
                }
                if(mypush){
                    tb -> state = pushed;
                    std::unique_lock<std::mutex> unique(mlock);
                    for(unsigned int j = 0; j < tb -> taskVec.size(); j++){
                        mworkqueue.push(tb -> taskVec[j]);       
                    }
                    unique.unlock();
                    mcv.notify_all();
                    
                }
            }
        }
        block.unlock();
    }
}
```



工作线程执行的线程函数，和partA的区别在于每个task都加了一个指向它所属的**taskBulk**的指针，当完成的task是对应**taskBulk**的最后一个task，将该**taskBulk**的state改为finished，并将**bulks_to_finish**（该线程池要完成的taskBulk数量）减1：

```c++
void TaskSystemParallelThreadPoolSpinning::myworker(){
    while(!stop){
        std::unique_lock<std::mutex> unique(mlock);
        mcv.wait(unique,[&](){return !mworkqueue.empty() || stop;});
        if(stop)break;
        MyTask* mtask = mworkqueue.front();
        mworkqueue.pop();
        mtask->runnable->runTask(mtask->i,mtask->num_total_tasks);
        mtask->tb->tasks_to_finish--;
            if(mtask -> tb -> tasks_to_finish == 0){       __sync_bool_compare_and_swap(&bulks_to_finish, bulks_to_finish, bulks_to_finish-1);
                mtask -> tb -> state = finished;
            }
        }
    }
}
```



**sync**函数，一旦调用就要一直忙等待到所有taskBulk全部执行完

```c++
void TaskSystemParallelThreadPoolSpinning::sync() {
    while(bulks_to_finish!=0){
        ;
    }
    return;
}
```

