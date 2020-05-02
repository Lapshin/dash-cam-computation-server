#ifndef INDICATORS_H
#define INDICATORS_H

#include <stdint.h>
#include <stddef.h>

/**
 * Working context for an indicator
 *
 * The structure should be defined in the implementation file so it can be
 * modified at will without recompiling the server.
 */
typedef struct indicator_ctx indicator_ctx_t;

/**
 * Indicator arguments
 *
 * This would contain indicator ctx pointer, data and size of data which
 * should be processing by indicator_func_t function.
 */
typedef struct indicator_arg_s {
    indicator_ctx_t *ctx;
    const uint8_t   *data;
    const size_t     size;
} indicator_arg_t;

/**
 * Indicator function
 *
 * To process the first block of file data, `ctx` must be initialized with
 * indicator_ctx_init_t function. Use the same `ctx` for other blocks.
 *
 * @param[in]   arg     pointer to structure indicator_arg_t.
 */
typedef void (*indicator_func_t)(indicator_arg_t *arg);

/**
 * Indicator context allocator
 *
 * Memory allocated by this function should be freed using
 * indicator_ctx_free_t function.
 *
 * @returns     pointer to allocated memory of ctx
 */
typedef indicator_ctx_t* (*indicator_ctx_alloc_t)(void);

/**
 * Indicator context init
 *
 * Initialize context with default values
 *
 * * @param[in]    ctx     Pointer to indicator context.
 */
typedef void (*indicator_ctx_init_t)(indicator_ctx_t *);

/**
 * Indicator context free prototype
 *
 * Frees memory alocated by indicator_ctx_alloc_t.
 *
 * * @param[in]    ctx     Pointer to indicator context.
 */
typedef void (*indicator_ctx_free_t)(indicator_ctx_t *);

/**
 * Extract indicator value from context
 *
 * Call this to get the final value of the indicator after processing an entire
 * file.
 *
 * @param[in]   ctx     Indicator context.
 * @returns     Value of indicator.
 */
typedef uint64_t (*indicator_ctx_extract_t)(const indicator_ctx_t *);

typedef struct indicators_handlers_s {
    indicator_ctx_alloc_t   ctx_allocator;
    indicator_ctx_init_t    ctx_initializer;
    indicator_ctx_free_t    ctx_free;
    indicator_func_t        indicator;
    indicator_ctx_extract_t extract;
} indicators_handlers_t;

/**
 * Get indicators handlers
 *
 * Call this to get the all handlers of all indicators.
 *
 * @returns     Pointer to array of indicators handlers.
 */
indicators_handlers_t *get_indicators_handlers(void);

/**
 * Get count of indicators
 *
 * Call this to get the count of indicators in this library.
 *
 * @param[in]   ctx     Indicator context.
 * @returns     Count of indicators.
 */
size_t get_indicators_count(void);


#endif
