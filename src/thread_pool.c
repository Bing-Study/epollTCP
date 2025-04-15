#include "thread_pool.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

static int task_queue_init(TaskQueue *queue,int capacity){
    queue->tasks = (ThreadPoolTask *) malloc(capacity * sizeof(ThreadPoolTask ));
    if(!queue->tasks){
        return THREAD_POOL_ERROR;
    }
    queue->capacity = capacity;
    queue->front = queue->rear = queue->count = 0;
    if (pthread_mutex_init(&queue->lock, NULL) != 0 ||
        pthread_cond_init(&queue->not_empty, NULL) != 0 ||
        pthread_cond_init(&queue->not_full, NULL) != 0) {
        free(queue->tasks);
        return THREAD_POOL_ERROR;
    }
    return  THREAD_POOL_SUCCESS;
}

void task_queue_clear(TaskQueue *queue){
    if(queue == NULL){
        return;
    }

    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->not_full);
    pthread_cond_destroy(&queue->not_empty);
    free(queue->tasks);
}

ThreadPool *thread_pool_init(int thread_num,int queue_size){
    if(thread_num <= 0 || queue_size <= 0){
        errno = EINVAL;
        return NULL;
    }
    ThreadPool *pool = (ThreadPool *)malloc(sizeof(ThreadPool));
    if(pool == NULL){
        printf("malloc pool false\n");
        errno = ENOMEM;
        return NULL;
    }

    if(task_queue_init(&pool->queue,queue_size) == -1){
        free(pool);
        return NULL;
    }
    pool->thread_count = thread_num;
    pool->shutdown = 0;

    if(pthread_mutex_init(&pool->pool_lock,NULL) != 0){
        task_queue_clear(&pool->queue);
        free(pool);
        return NULL;
    }

    pool->active_threads = 0;
    pthread_cond_init(&pool->all_idle, NULL);

    pool->threads = (pthread_t *)malloc(thread_num * sizeof(pthread_t));
    if(!pool->threads){
        pthread_mutex_destroy(&pool->pool_lock);
        task_queue_clear(&pool->queue);
        free(pool);
        errno = ENOMEM;
        return NULL;
    }

    //创建空闲线程
    for(int i = 0; i < thread_num ; i++){
        if(pthread_create(&pool->threads[i],NULL,worker_thread,pool) != 0){
            pool->thread_count = i;
            thread_pool_destroy(pool);
            return NULL;
        }
    }
    printf("Init pool success \n");
    return pool;
}
void thread_pool_destroy(ThreadPool *pool){
    if(!pool) return;

    // 设置关闭标志
    pthread_mutex_lock(&pool->pool_lock);
    pool->shutdown = 1;
    // 关闭等待
    while(pool->active_threads > 0 || pool->queue.count > 0) {
        pthread_cond_wait(&pool->all_idle, &pool->pool_lock);
    }
    pthread_mutex_unlock(&pool->pool_lock);

    // 唤醒所有等待线程
    pthread_cond_broadcast(&pool->queue.not_empty);
    pthread_cond_broadcast(&pool->queue.not_full);

    // 等待所有工作线程退出
    for (int i = 0; i < pool->thread_count; ++i) {
        pthread_join(pool->threads[i], NULL);
    }

    // 销毁同步原语
    pthread_mutex_destroy(&pool->pool_lock);
    pthread_mutex_destroy(&pool->queue.lock);
    pthread_cond_destroy(&pool->queue.not_empty);
    pthread_cond_destroy(&pool->queue.not_full);

    // 释放内存资源
    free(pool->queue.tasks);
    free(pool->threads);
    free(pool);
}
static void* worker_thread(void *arg) {
    ThreadPool *pool = (ThreadPool*)arg;
    while(1){
        pthread_mutex_lock(&pool->queue.lock);

        while(pool->queue.count == 0 && !pool->shutdown){
            pthread_cond_wait(&pool->queue.not_empty,&pool->queue.lock);
        }

        if(pool->shutdown){
            pthread_mutex_unlock(&pool->queue.lock);
            break;
        }

        //获取任务
        ThreadPoolTask task = task_queue_pop(&pool->queue);
        pthread_mutex_unlock(&pool->queue.lock);

        if (task.function) {
            pthread_mutex_lock(&pool->pool_lock);
            pool->active_threads++;
            pthread_mutex_unlock(&pool->pool_lock);

            task.function(task.arg);

            pthread_mutex_lock(&pool->pool_lock);
            pool->active_threads--;
            if (pool->active_threads == 0 && pool->shutdown) {
                pthread_cond_signal(&pool->all_idle);
            }
            pthread_mutex_unlock(&pool->pool_lock);
        }
    }
    return NULL;
}
int task_queue_push(TaskQueue *queue,ThreadPoolTask task){
    pthread_mutex_lock(&queue->lock);
    while(queue->count == queue->capacity){
        pthread_cond_wait(&queue->not_full,&queue->lock);
    }

    queue->tasks[queue->rear] = task;
    queue->rear = (queue->rear+1) % queue->capacity;
    queue->count++;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
    return 0;
}
ThreadPoolTask task_queue_pop(TaskQueue *queue){
    while(queue->count == 0){
        pthread_cond_wait(&queue->not_empty,&queue->lock);
    }

    ThreadPoolTask task = {NULL,NULL};

    if(queue->count > 0){
        task = queue->tasks[queue->front];
        queue->front = (queue->front +1) % queue->capacity;
        queue->count --;
        pthread_cond_signal(&queue->not_full);
    }
    return task;
}

int judeg_queue_is_empty(TaskQueue queue){
    if(queue.front == queue.rear)
        return 1;
    return 0;
}
int thread_pool_add_task(ThreadPool *pool,void (*func)(void *),void *arg){
    if(pool == NULL || func == NULL){
        errno = EINVAL;
        return -1;
    }
    pthread_mutex_lock(&pool->pool_lock);
    if(pool->shutdown){
        pthread_mutex_unlock(&pool->pool_lock);
        return -1;
    }
    pthread_mutex_unlock(&pool->pool_lock);
    ThreadPoolTask task = {NULL,NULL};
    task.function = func;
    task.arg = arg;

    if(task_queue_push(&pool->queue,task) == 0){
        printf("push queue success!\n");
    }
    return 0;
}

int thread_pool_remove_task(ThreadPool *pool,ThreadPoolTask *target) {
    if (pool == NULL || target == NULL) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pool->queue.lock);
    int found = 0;
    int current = pool->queue.front;
    for (int i = 0; i < pool->queue.count; i++) {
        ThreadPoolTask *task = &pool->queue.tasks[current];
        // 自定义匹配逻辑：比较函数指针和参数
        if (task->function == target->function &&
            task->arg == target->arg) {
            found = 1;

            // 移动后续元素覆盖当前任务
            for (int j = i; j < pool->queue.count - 1; j++) {
                int next = (current + 1) % pool->queue.capacity;
                pool->queue.tasks[current] = pool->queue.tasks[next];
                current = next;
            }

            pool->queue.count--;
            pool->queue.rear = (pool->queue.rear - 1 + pool->queue.capacity) % pool->queue.capacity;
            break;
        }

        current = (current + 1) % pool->queue.capacity;
    }

    if (found) {
        pthread_cond_signal(&pool->queue.not_full);  // 通知队列有空位
    }

    pthread_mutex_unlock(&pool->queue.lock);
    return found ? 0 : -1;  // 返回是否成功移除
}

int thread_pool_get_active_count(ThreadPool *pool){
    pthread_mutex_lock(&pool->pool_lock);
    int count = pool->active_threads;
    pthread_mutex_unlock(&pool->pool_lock);
    return count;
}
int thread_pool_get_queue_size(ThreadPool *pool) {
    pthread_mutex_lock(&pool->queue.lock);
    int size = pool->queue.count;
    pthread_mutex_unlock(&pool->queue.lock);
    return size;
}
int thread_pool_test(){
    ThreadPool *pool;
    int i = 0;
    pool = thread_pool_init(4,20);
    sleep(1);
    if(thread_pool_add_task(pool,pool_function_test,NULL) == 0){
        printf("add task to pool success\n");
    }
    if(thread_pool_add_task(pool,pool_function_test_1,NULL) == 0){
        printf("add task to pool success\n");
    }
    if(thread_pool_add_task(pool,pool_function_test_2,NULL) == 0){
        printf("add task to pool success\n");
    }
    sleep(3);
    i = thread_pool_get_active_count(pool);
    printf("the thread pool activite count:%d\n",i);
    thread_pool_destroy(pool);
    return 0;
}
void pool_function_test(void *arg){
    int i;
    for(i = 0 ; i < 20 ; i++){
        printf("%s: i : %d\n",__func__,i);
        sleep(1);
    }
}
void pool_function_test_1(void *arg){
    int i;
    for(i = 0 ; i < 20 ; i++){
        printf("%s: i : %d\n",__func__,i);
        sleep(1);
    }

}
void pool_function_test_2(void *arg){
    int i;
    for(i = 0 ; i < 20 ; i++){
        printf("%s: i : %d\n",__func__,i);
        sleep(2);
    }
}