#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include "threadpool.h"
#include "log.h"

typedef enum lanes_manage_action_e {
    LANES_FIRST_TIME_INIT = 0,
    LANES_CLEAN_ALL_TASKS,
    LANES_CLEAN_TASKS_AND_RESTART_WORKERS,
    LANES_SHUTDOWN_ALL
} lanes_manage_action_t;

static void *processor(void *pool);
static void thread_pool_cleanup_task(queued_task_t **task);
static int manage_lanes_workers(thread_pool_t *pool, lanes_manage_action_t action, thread_pool_lane_conf_t *thread_pool_lanes);

thread_pool_t *thread_pool_create(thread_pool_lane_conf_t *thread_pool_lanes, const size_t lanes_count)
{
    int ret = -1;
    thread_pool_t *pool = NULL;

    if (thread_pool_lanes == NULL || lanes_count == 0)
    {
        logger(ERROR, "Wrong input parameters %p %lu", (void *)thread_pool_lanes, lanes_count);
        goto exit;
    }

    pool = calloc(sizeof(*pool), 1);
    if(pool == NULL) {
        logger(ERROR, "Can't alloc memory!");
        goto error;
    }

    pool->work = true;
    pthread_mutex_init(&pool->lock, NULL);

    pool->lanes_count = lanes_count;
    pool->lanes = calloc(sizeof(*(pool->lanes)), lanes_count);
    if(pool->lanes == NULL) {
        logger(ERROR, "Can't alloc memory!");
        goto error;
    }

    logger(DEBUG, "Creating %lu lanes in thread pool", lanes_count);
    ret = manage_lanes_workers(pool, LANES_FIRST_TIME_INIT, thread_pool_lanes);
    if (ret != 0)
    {
        logger(ERROR, "Error while init thread pool lanes");
        goto error;
    }
    goto exit;

error:
    thread_pool_destroy(pool);
    pool = NULL;
exit:
    return pool;
}

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
int thread_pool_add_task(thread_pool_t *pool, job_func_t job, void* arg, size_t lane_num)
{
    int ret = -1;
    queued_task_t *task = NULL;

    if (pool == NULL)
    {
        logger(ERROR, "Pool pointer is NULL");
        return ret;
    }
    task = malloc(sizeof(*task));
    if (task == NULL)
    {
        logger(ERROR, "Can't alloc memory!", __func__);
        return ret;
    }
    task->job  = job;
    task->args = arg;

    pthread_mutex_lock(&pool->lock);
    if (lane_num >= pool->lanes_count)
    {
        logger(ERROR, "pool lane is out of range (%lu:%lu)", lane_num, pool->lanes_count);
        goto error_exit;
    }
    if(pool->work == false)
    {
        goto error_exit;
    }
    TAILQ_INSERT_TAIL(&pool->lanes[lane_num].queue, task, next);
    sem_post(&pool->lanes[lane_num].queue_sem);

    goto exit;

error_exit:
    thread_pool_cleanup_task(&task);

exit:
    pthread_mutex_unlock(&pool->lock);
    return ret;
}

static size_t get_porcessor_lane_id(thread_pool_t *pool)
{
    size_t i, k;
    pthread_t self = pthread_self();
    pthread_mutex_lock(&pool->lock);

    for (i = 0; i < pool->lanes_count; i++)
    {
        for (k = 0; k < pool->lanes[i].size ; k++)
        {
            if (pool->lanes[i].processors[k] == self)
            {
                goto exit;
            }
        }
    }

exit:
    pthread_mutex_unlock(&pool->lock);
    return (i == pool->lanes_count) ? -1LU : i;
}

static void thread_pool_cleanup_task(queued_task_t **task)
{
    if (task == NULL)
    {
        return;
    }
    if (*task)
    {
        free((*task)->args);
        (*task)->args = NULL;
    }
    free(*task);
    *task = NULL;
}


static void *processor(void *arg) {
    queued_task_t *task = NULL;
    thread_pool_t *pool = (thread_pool_t*) arg;
    thread_pool_lane_t *lane = NULL;

    size_t lane_id = get_porcessor_lane_id(pool);
    if (lane_id == -1UL)
    {
        logger(ERROR, "Can not find my lane id");
        pthread_mutex_unlock(&pool->lock);
        return NULL;
    }
    logger(DEBUG, "My lane id is %lu", lane_id);
    lane = &(pool->lanes[lane_id]);

    while (true) {
        sem_wait(&lane->queue_sem);
        pthread_mutex_lock(&pool->lock);
        if (pool->work == false)
        {
            pthread_mutex_unlock(&pool->lock);
            break;
        }
        task = lane->queue.tqh_first;
        if (!task) {
            pthread_mutex_unlock(&pool->lock);
            continue;
        }
        TAILQ_REMOVE(&lane->queue, task, next);
        pthread_cleanup_push((void (*) (void*))thread_pool_cleanup_task, &task);
        pthread_mutex_unlock(&pool->lock);

        logger(DEBUG, "Lane #%lu is starting job", lane_id);
        task->job(task->args);
        pthread_cleanup_pop(1);
    }
    logger(DEBUG, "Pool worker finished");
    return NULL;
}

static void thread_pool_create_worker(thread_pool_t *pool, pthread_t *thread_id)
{
    sigset_t set;
    sigfillset(&set);
    pthread_sigmask(SIG_BLOCK, &set, NULL); // TODO error checking...

    /* any newly created threads inherit the signal mask */
    pthread_create(thread_id, NULL, processor, pool);

    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

static int manage_lanes_workers(thread_pool_t *pool, lanes_manage_action_t action, thread_pool_lane_conf_t *thread_pool_lanes)
{
    int ret = -1;
    size_t i, k;
    queued_task_t *task = NULL;
    thread_pool_lane_t *lane = NULL;
    if (pool == NULL || pool->lanes == NULL)
    {
        goto exit;
    }
    if (action == LANES_FIRST_TIME_INIT && thread_pool_lanes == NULL)
    {
        goto exit;
    }

    for (i = 0; i < pool->lanes_count; i++)
    {
        lane = &(pool->lanes[i]);
        if (action == LANES_FIRST_TIME_INIT)
        {
            lane->size = thread_pool_lanes[i].size;
        }
        for (k = 0; k < lane->size; k++)
        {
            switch (action)
            {
            case LANES_FIRST_TIME_INIT:
                lane->processors = calloc(sizeof(*(lane->processors)), lane->size);
                if (lane->processors == NULL)
                {
                    logger(ERROR, "Can't alloc memory for processors!");
                    goto exit;
                }

                ret = sem_init(&lane->queue_sem, 0, 0);
                if (ret)
                {
                    logger(ERROR, "Failed on sem_init. ret = %d", ret);
                    goto exit;
                }
                logger(DEBUG, "INIT LANE QUEUE");
                TAILQ_INIT(&lane->queue);
                thread_pool_create_worker(pool, &lane->processors[k]);
                break;
            case LANES_CLEAN_ALL_TASKS:
            case LANES_CLEAN_TASKS_AND_RESTART_WORKERS:
            case LANES_SHUTDOWN_ALL:
                while ((task = lane->queue.tqh_first)) {
                    TAILQ_REMOVE(&lane->queue, task, next);
                    thread_pool_cleanup_task(&task);
                }
                if (action == LANES_CLEAN_ALL_TASKS)
                {
                    break;
                }
                pthread_cancel(lane->processors[k]);
                pthread_join(lane->processors[k], NULL);

                if (action == LANES_SHUTDOWN_ALL)
                {
                    sem_destroy(&lane->queue_sem);
                    free(lane->processors);
                    lane->processors = NULL;
                }
                else if (action == LANES_CLEAN_TASKS_AND_RESTART_WORKERS)
                {
                    thread_pool_create_worker(pool, &lane->processors[k]);
                }
                break;

            default:
                break;
            }
        }
    }
    ret = 0;

exit:
    return ret;
}
static void thread_pool_shutdown_lanes(thread_pool_t *pool)
{
    pthread_mutex_lock(&pool->lock);
    pool->work = false;
    manage_lanes_workers(pool, LANES_SHUTDOWN_ALL, NULL);
    pthread_mutex_unlock(&pool->lock);
}

/* Function for external usage, just clear and restart working threads */
void thread_pool_remove_all_tasks(thread_pool_t *pool)
{
    if (pool == NULL)
    {
        logger(ERROR, "pool pointer is NULL");
        return;
    }
    pthread_mutex_lock(&pool->lock);
    manage_lanes_workers(pool, LANES_CLEAN_TASKS_AND_RESTART_WORKERS, NULL);
    pthread_mutex_unlock(&pool->lock);
}

void thread_pool_destroy(thread_pool_t *pool)
{
    if (pool == NULL)
    {
        logger(ERROR, "pool pointer is NULL");
        return;
    }

    thread_pool_shutdown_lanes(pool);

    free(pool->lanes);
    pthread_mutex_destroy(&pool->lock);
    free(pool);
}
