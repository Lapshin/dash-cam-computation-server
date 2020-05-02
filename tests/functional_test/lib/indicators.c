#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

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

void indicator(__attribute__((unused))indicator_arg_t *arg)
{
    arg->ctx->value += arg->size;
    return;
}

indicator_ctx_t *indicator_alloc(void)
{
    return malloc(sizeof(indicator_ctx_t));
}

void indicator_free(indicator_ctx_t *ctx)
{
    free(ctx);
}

void indicator_init(indicator_ctx_t *ctx)
{
    memset(ctx, 0x00, sizeof(*ctx));
}

uint64_t indicator_extract(const indicator_ctx_t *ctx)
{
    uint64_t ret_val;
    assert(ctx != NULL);
    ret_val = ctx->value;
    return ret_val;
}

static indicators_handlers_t indicators_handlers[] =
{
        {&indicator_alloc, &indicator_init, &indicator_free, &indicator, &indicator_extract},
        {&indicator_alloc, &indicator_init, &indicator_free, &indicator, &indicator_extract},
        {&indicator_alloc, &indicator_init, &indicator_free, &indicator, &indicator_extract},
};


indicators_handlers_t *get_indicators_handlers(void)
{
    return indicators_handlers;
}

size_t get_indicators_count(void)
{
    return sizeof(indicators_handlers)/sizeof(indicators_handlers[0]);
}
