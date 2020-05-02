#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "indicators.h"

/**
 * Indicator context
 *
 * This would contain whatever information the indicator functions require to
 * keep state across calls.
 */
struct indicator_ctx {
    /** Current value of the indicator */
    uint64_t value;
};

/**
 * Indicator function
 *
 * To process the first block of file data, `ctx` must be initialized with
 * indicator_ctx_init_t function. Use the same `ctx` for other blocks.
 *
 * @param[in]   arg     pointer to structure with input data, size, and ctx.
 */
void indicator(indicator_arg_t *arg)
{
#define SPEED 5 /* in MB/sec*/
    unsigned int sleep_time = (unsigned int)(arg->size)/SPEED;
    sleep(sleep_time/1000000);
    usleep(sleep_time%1000000);
    return;
}

/**
 * Indicator context allocator
 *
 * Memory allocated by this function should be freed using
 * indicator_ctx_free_t function.
 *
 * @returns     pointer to allocated memory of ctx
 */
indicator_ctx_t *indicator_alloc(void)
{
    return malloc(sizeof(indicator_ctx_t));
}

/**
 * Indicator context free
 *
 * Frees memory alocated by indicator_ctx_alloc_t.
 *
 * * @param[in]    ctx     Pointer to indicator context.
 */
void indicator_free(indicator_ctx_t *ctx)
{
    free(ctx);
}

/**
 * Indicator context init
 *
 * Initialize context with default values
 *
 * * @param[in]    ctx     Pointer to indicator context.
 */
void indicator_init(indicator_ctx_t *ctx)
{
    memset(ctx, 0x00, sizeof(*ctx));
}

/**
 * Extract indicator value from context
 *
 * Call this to get the final value of the indicator after processing an entire
 * file.
 *
 * @param[in]   ctx     Indicator context.
 * @returns     Value of indicator.
 */
uint64_t indicator_extract(__attribute__((unused))const indicator_ctx_t *ctx)
{
    static uint64_t result = 0;
    return result++;
}

static indicators_handlers_t indicators_handlers[] =
{
        {&indicator_alloc, &indicator_init, &indicator_free, &indicator, &indicator_extract},
        {&indicator_alloc, &indicator_init, &indicator_free, &indicator, &indicator_extract},
        {&indicator_alloc, &indicator_init, &indicator_free, &indicator, &indicator_extract},
        {&indicator_alloc, &indicator_init, &indicator_free, &indicator, &indicator_extract},
};

/**
 * Get indicators handlers
 *
 * Call this to get the all handlers of all indicators.
 *
 * @returns     Pointer to array of indicators handlers.
 */
indicators_handlers_t *get_indicators_handlers(void)
{
    return indicators_handlers;
}

/**
 * Get count of indicators
 *
 * Call this to get the count of indicators in this library.
 *
 * @param[in]   ctx     Indicator context.
 * @returns     Count of indicators.
 */
size_t get_indicators_count(void)
{
    return sizeof(indicators_handlers)/sizeof(indicators_handlers[0]);
}
