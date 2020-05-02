#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>

#include "dash_cam.h"

#include "indicators.h"
#include "threadpool.h"
#include "log.h"
#include "server_core.h"
#include "timer.h"

#define MAX_CLIENTS_COUNT 1
#define TIME_FOR_PROCESSING 35 /* in seconds */
#define MAX_FILE_SIZE ((size_t) (1000 * 1000 * 80)) /* 80MB */
#define MAX_RECEIVED_SIZE (sizeof(messageHeader_t) + MAX_FILE_SIZE)

bool server_running = true;
message_t *msg = NULL;
message_t *response = NULL;
thread_pool_t *tp = NULL;
indicators_handlers_t *indicators_handlers;
size_t indicators_count = 0;
indicator_ctx_t **indicators_ctx;

int clint_fd = -1;

sem_t indicators_finish_sem;

void server_exit(void)
{
    server_running = 0;
}

static void timer_expired_handler(int sig, __attribute__((unused)) siginfo_t *si, __attribute__((unused)) void *uc)
{
    size_t i;
    for (i = 0; i < indicators_count + 1; i++)
    {
        sem_post(&indicators_finish_sem);
    }
    close(clint_fd);
    signal(sig, SIG_IGN);
}

static int check_header(const int fd, const messageHeader_t *header)
{
    int ret = -1;
    size_t read_size = read_wrapper(fd, (uint8_t *)header, sizeof(*header), true);
    if (read_size != sizeof(*header))
    {
        logger(ERROR, "read_wrapper error");
        goto exit;
    }

    if (ntohl(header->magic) != HEADER_MAGIC)
    {
        logger(ERROR, "Magic was not found in received data");
        goto exit;
    }

    if (ntohl(header->version) != HEADER_VERSION)
    {
        logger(ERROR, "Unknown protocol version %d. Server uses %d", ntohl(header->version), HEADER_VERSION);
        goto exit;
    }

    if (ntohl(header->size) == 0)
    {
        logger(ERROR, "Payload size is 0");
        goto exit;
    }

    ret = 0;
exit:
    return ret;
}

static size_t get_file_size(const messageHeader_t *header)
{
    size_t size = ntohl(header->size);

    if (size > MAX_FILE_SIZE)
    {
         logger(ERROR, "file size is too big (%lu). Max size is %lu", size, MAX_FILE_SIZE);
         size = 0;
    }
    return size;
}

static size_t get_frame_size(const messageHeader_t *header)
{
    size_t size = ntohl(header->size);
    size_t frame_size = ntohl(header->frame_size);

    if (frame_size > size)
    {
         logger(ERROR, "frame size is too big (%lu). file size is %lu", frame_size, size);
         frame_size = 0;
    }
    if (size%frame_size != 0)
    {
         logger(ERROR, "Can not divide size by frame size (%lu%%%lu != 0)", size, frame_size);
         frame_size = 0;
    }
    return frame_size;
}

static int alloc_message_buffers(void)
{
    size_t response_size = sizeof(messageHeader_t) + (sizeof(uint64_t) * indicators_count);
    msg = malloc(MAX_RECEIVED_SIZE);
    if(msg == NULL)
    {
        logger(ERROR, "Can not allocate memory for receive buffer (size %lu)", MAX_RECEIVED_SIZE);
        return -1;
    }

    response = malloc(response_size);
    if(response == NULL)
    {
        logger(ERROR, "Can not allocate memory for response buffer (size %lu)", response_size);
        return -1;
    }
    return 0;
}

static void free_message_buffers(void)
{
    free(msg);
    msg = NULL;

    free(response);
    response = NULL;
}

void calc_indicators(const uint8_t *data, const size_t size)
{
    size_t i;
    indicator_arg_t arg_pattern = {
            .ctx = NULL,
            .data = data,
            .size = size
    };
    for (i = 0; i < indicators_count; i++)
    {
        indicator_arg_t *arg = malloc(sizeof(*arg));
        if (arg == NULL)
        {
            logger(ERROR, "Failed to allocate memory");
            continue;
        }
        memcpy(arg, &arg_pattern, sizeof(arg_pattern));
        arg->ctx  = indicators_ctx[i];
        job_func_t fun_ptr = (job_func_t)indicators_handlers[i].indicator;
        thread_pool_add_task(tp, fun_ptr, arg, i);
    }
}

void *calc_indicators_finalize_handler(void *arg)
{
    sem_t **sem = arg;
    sem_post(*sem);
    return NULL;
}

void calc_indicators_finalize(void)
{
    size_t i;
    for (i = 0; i < indicators_count; i++)
    {
        sem_t **arg = malloc(sizeof(*arg));
        if (arg == NULL)
        {
            logger(ERROR, "Failed to allocate memory");
            continue;
        }
        *arg = &indicators_finish_sem;
        thread_pool_add_task(tp, calc_indicators_finalize_handler, arg, i);
    }
}

static inline size_t get_received_frames(size_t cur_size, size_t prev_size, size_t frame_size)
{
    return ((cur_size - prev_size) / frame_size);
}

int get_file_and_calc_indicators(const int fd, uint8_t *data_ptr, const size_t size, const size_t frame_size)
{
    int ret = -1;
    uint8_t *current_ptr     = data_ptr;
    uint8_t *prev_ptr        = data_ptr;
    size_t   current_size    = 0;
    size_t   prev_size       = 0;
    size_t   frames_received = 0;

    while(current_size != size)
    {
        size_t read_size = read_wrapper(fd, current_ptr, size, false);
        if (read_size == 0)
        {
            logger(ERROR, "Error while receiving data");
            goto exit;
        }
        current_size += read_size;

        current_ptr =  data_ptr + current_size;

        frames_received = get_received_frames(current_size, prev_size, frame_size);
        logger(DEBUG, "current_size %lu frames_received %lu", current_size, frames_received);
        if (frames_received > 0)
        {
            size_t size_to_process = frames_received * frame_size;
            logger(DEBUG, "size_to_process %lu", size_to_process);
            calc_indicators(prev_ptr, size_to_process);
            prev_size += size_to_process;
            prev_ptr = data_ptr + prev_size;
        }
    }
    calc_indicators_finalize();

    ret = 0;
exit:
    return ret;
}

static void init_indicators_ctx(void)
{
    size_t i;
    for (i = 0; i < indicators_count; i++)
    {
        indicators_handlers[i].ctx_initializer(indicators_ctx[i]);
    }
}

static int waiting_for_indicators_finish()
{
    size_t i;
    for (i = 0; i < indicators_count; i++)
    {
        sem_wait(&indicators_finish_sem);
    }
    return (sem_trywait(&indicators_finish_sem) == 0) ? -1 : 0;
}

static void clear_indicators_finish_sem(void)
{
    while (sem_trywait(&indicators_finish_sem) == 0)
    {
        ;
    }
}

int send_indicators_metrics_to_client(const int fd)
{
    ssize_t ret = -1;
    size_t i = 0;
    uint32_t  payload_size  = (uint32_t)(sizeof(uint64_t) * indicators_count);
    size_t    response_size = sizeof(messageHeader_t) + payload_size;
    uint64_t *payload_ptr   = (uint64_t *)response->payload;

    logger(DEBUG, "Payload size %lu", payload_size);
    memset(response, 0x00, sizeof(response->header) + payload_size);

    response->header.magic      = htonl(HEADER_MAGIC);
    response->header.size       = htonl(payload_size);
    response->header.frame_size = htonl(sizeof(uint64_t));
    response->header.version    = htonl(HEADER_VERSION);

    for (i = 0; i < indicators_count; i++)
    {
        payload_ptr[i] = htobe64(indicators_handlers->extract(indicators_ctx[i]));
    }

    ret = write(fd, response, response_size);
    if (ret < 0 || ((size_t) ret) < response_size)
    {
        logger(ERROR, "error while sending response (%d:%s)",  errno, strerror(errno));
        return -1;
    }
    return 0;
}


static int process_messages(const int fd)
{
    int ret = -1;
    messageHeader_t  *header = NULL;
    size_t file_size  = 0;
    size_t frame_size = 0;
    memset(msg, 0x00, MAX_RECEIVED_SIZE);

    logger(DEBUG, "Start processing message");

    header = &msg->header;
    ret = check_header(fd, header);
    if (ret != 0) {
        logger(ERROR, "Bad header");
        goto exit;
    }

    file_size = get_file_size(header);
    if (file_size == 0)
    {
        logger(ERROR, "Bad file size");
        ret = -1;
        goto exit;
    }

    frame_size = get_frame_size(header);
    if (frame_size == 0)
    {
        logger(ERROR, "Bad frame size");
        ret = -1;
        goto exit;
    }

    logger(DEBUG, "Starting to read file with size %lu", file_size);

    init_indicators_ctx();

    ret = get_file_and_calc_indicators(fd, msg->payload, file_size, frame_size);
    if (ret != 0) {
        logger(ERROR, "Error while calculating indicators");
        goto cleanup;
    }

    logger(DEBUG, "Waiting for indicators finish");
    ret = waiting_for_indicators_finish();
    if (ret != 0) {
        logger(ERROR, "Calculating was not finished in time slot");
        goto cleanup;
    }
    logger(DEBUG, "Indicators are finished");

    ret = send_indicators_metrics_to_client(fd);
    if (ret != 0) {
        logger(ERROR, "Calculating was not finished in time slot");
        goto exit;
    }
    logger(DEBUG, "Indicators metrics were sent to client");

    goto exit;

cleanup:
    thread_pool_remove_all_tasks(tp);
exit:

    return ret;
}

static void polling(const int server_fd)
{
    int ret = 0;

    while(server_running)
    {
        logger(DEBUG, "Clear data before next client...");
        timer_disarm();
        if (clint_fd != -1)
        {
            close(clint_fd);
        }
        clear_indicators_finish_sem();

        logger(INFO, "Waiting for new connection...");
        ret = waiting_connection(server_fd);
        if (ret < 0)
        {
            continue;
        }

        logger(DEBUG, "Arming timer before accepting connection");
        timer_arm(TIME_FOR_PROCESSING);
        clint_fd = accept_connection(server_fd);
        if (clint_fd < 0)
        {
            logger(DEBUG, "Connection is not established");
            continue;
        }

        /* process data from client */
        logger(INFO, "[fd %d] Process incoming data from client...", clint_fd);
        ret = process_messages(clint_fd);
        logger(INFO, "[fd %d] Processing finished. %s", clint_fd, ret == 0 ? "Success" : "Fail");
    }
}

int init_thread_pool(void)
{
    int ret = -1;
    size_t i;
    thread_pool_lane_conf_t *lanes = calloc(sizeof(*lanes),indicators_count);
    if (lanes == NULL)
    {
        logger(ERROR, "Error while allocating memory");
        goto exit;
    }
    for (i = 0; i < indicators_count; i++)
    {
        lanes[i].size = MAX_CLIENTS_COUNT;
    }
    tp = thread_pool_create(lanes, indicators_count);
    if (tp != NULL)
    {
        ret = 0;
    }

exit:
    free(lanes);
    return ret;
}

int init_indicators_lib()
{
    size_t i;
    indicators_count = get_indicators_count();
    indicators_handlers = get_indicators_handlers();
    if (indicators_handlers == NULL || indicators_count == 0)
    {
        return -1;
    }
    indicators_ctx = calloc(sizeof(*indicators_ctx), indicators_count);
    for (i = 0; i < indicators_count; i++)
    {
        indicators_ctx[i] = indicators_handlers[i].ctx_allocator();
    }
    return 0;
}

void deinit_indicators_lib()
{
    size_t i;
    if (indicators_handlers == NULL || indicators_count == 0 || indicators_ctx == NULL)
    {
        return;
    }
    for (i = 0; i < indicators_count; i++)
    {
        indicators_handlers[i].ctx_free(indicators_ctx[i]);
        indicators_ctx[i] = NULL;
    }
    free(indicators_ctx);
    indicators_ctx = NULL;
}

void server_run(char *ip, uint16_t  port)
{
    int server_fd = -1;

    logger(INFO, "Preparing all server's resources...");

    server_fd = init_server(ip, port, MAX_CLIENTS_COUNT);
    if(server_fd < 0)
    {
        logger(ERROR, "Error while create server socket descriptor");
        goto exit;
    }

    if (init_indicators_lib() != 0)
    {
        logger(ERROR, "Bad data from indicators library");
        goto exit;
    }

    if (alloc_message_buffers() != 0)
    {
        logger(ERROR, "Can not allocate memory for message buffers");
        goto exit;
    }

    if (init_thread_pool() != 0)
    {
        logger(ERROR, "Error while initializing thread pool");
        goto exit;
    }

    if (init_timer(timer_expired_handler) != 0)
    {
        logger(ERROR, "Error while initializing timer");
        goto exit;
    }

    if (sem_init(&indicators_finish_sem, 0, 0) != 0)
    {
        logger(ERROR, "Failed on sem_init");
        goto exit;
    }

    logger(INFO, "Server prepared, start to polling...");
    /* working loop */
    polling(server_fd);

exit:

    free_message_buffers();
    deinit_timer();
    deinit_indicators_lib();
    thread_pool_destroy(tp);
    if (server_fd != -1)
    {
        close(server_fd);
    }
}
