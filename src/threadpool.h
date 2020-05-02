#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <semaphore.h>
#include <stdatomic.h>
#include <sys/queue.h>
#include <stdbool.h>

typedef void * (*job_func_t)(void *);

typedef struct queued_task_s {
    job_func_t job;
    void *args;
    TAILQ_ENTRY(queued_task_s) next;
} queued_task_t;

typedef struct thread_pool_lane_s {
    size_t size;
    pthread_t *processors;
    sem_t queue_sem;
    TAILQ_HEAD(, queued_task_s) queue;
} thread_pool_lane_t;

typedef struct thread_pool_s {
    pthread_mutex_t lock;
    bool work;
    thread_pool_lane_t *lanes;
    size_t lanes_count;
} thread_pool_t;

typedef struct thread_pool_lane_conf_s {
    size_t size;
} thread_pool_lane_conf_t;

thread_pool_t *thread_pool_create(thread_pool_lane_conf_t *thread_pool_lanes, size_t lanes_count);

/**
 * Add task for thread pool lane
 *
 * Call this to get the all handlers of all indicators.
 *
 * @param[in]   pool        existing pool pointer.
 * @param[in]   job         Pointer to function to run.
 * @param[in]   arg         Pointer to argument for job function. MUST created by malloc()
 * @param[in]   lane_num    number of lane in pool
 * @returns     Zero if success
 */
int thread_pool_add_task(thread_pool_t *pool, job_func_t job, void* arg, size_t lane_num);
void thread_pool_remove_all_tasks(thread_pool_t *pool);
void thread_pool_destroy(thread_pool_t *pool);

#endif /* THREADPOOL_H_ */
