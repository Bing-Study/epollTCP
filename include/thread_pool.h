#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <pthread.h>
#include <stdatomic.h>
#define THREAD_POOL_SIZE 4
#define QUEUE_SIZE 256

#define THREAD_POOL_ERROR -1;
#define THREAD_POOL_SUCCESS 1;

/* 任务结构体 */
typedef struct {
    void (*function)(void *);  // 任务函数指针
    void *arg;                 // 任务参数
} ThreadPoolTask;

/* 线程安全的任务队列 */
typedef struct {
    ThreadPoolTask *tasks;     // 任务数组
    int capacity;              // 队列容量
    int front;                 // 队首索引
    int rear;                  // 队尾索引
    int count;                 // 当前任务数
    pthread_mutex_t lock;      // 队列互斥锁
    pthread_cond_t not_empty;  // 非空条件变量
    pthread_cond_t not_full;   // 非满条件变量
} TaskQueue;

/* 线程池主结构 */
typedef struct {
    pthread_t *threads;        // 工作线程数组
    int thread_count;          // 线程数量
    TaskQueue queue;           // 任务队列
    int active_threads;         //活跃线程数
    int shutdown;              // 关闭标志
    pthread_mutex_t pool_lock; // 池状态锁
    pthread_cond_t  all_idle;   //空闲条件变量
} ThreadPool;

static int task_queue_init(TaskQueue *queue,int capacity);
int task_queue_push(TaskQueue *queue,ThreadPoolTask task);
int judeg_queue_is_empty(TaskQueue queue);
ThreadPoolTask task_queue_pop(TaskQueue *queue);
void task_queue_clear(TaskQueue *queue);
ThreadPool *thread_pool_init(int thread_num,int queue_size);
void thread_pool_destroy(ThreadPool *pool);
static void* worker_thread(void *arg);
int thread_pool_add_task(ThreadPool *pool,void (*func)(void *),void *arg);
int thread_pool_remove_task(ThreadPool *pool,ThreadPoolTask *target);
int thread_pool_get_active_count(ThreadPool *pool);
int thread_pool_get_queue_size(ThreadPool *pool);
int thread_pool_test();
void pool_function_test(void *arg);
void pool_function_test_1(void *arg);
void pool_function_test_2(void *arg);
#endif