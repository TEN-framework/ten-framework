//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/**
 * @file
 * @brief A runloop is the core of an asynchronous event-driven programming
 * model, which handles all the events, timers, signals and message queues in an
 * application.
 */
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TEN_RUNLOOP_EVENT2 "event2"
#define TEN_RUNLOOP_UV "uv"
#define TEN_RUNLOOP_BARE "bare"

/**
 * @brief This data structure represents a runloop.
 */
typedef struct ten_runloop_t ten_runloop_t;

/**
 * @brief This data structure represents an asynchronous event signal.
 */
typedef struct ten_runloop_async_t ten_runloop_async_t;

/**
 * @brief This data structure represents a timer.
 */
typedef struct ten_runloop_timer_t ten_runloop_timer_t;

typedef struct ten_thread_t ten_thread_t;

TEN_UTILS_API bool ten_runloop_check_integrity(ten_runloop_t *self,
                                               bool check_thread);

/**
 * @brief Create a runloop.
 * @param type The implementation of the runloop.
 *             Create from a default one if |type| == NULL
 * @return The runloop. NULL if failed.
 */
TEN_UTILS_API ten_runloop_t *ten_runloop_create(const char *type);

/**
 * @brief Attach to an existing runloop with "raw loop pointer"
 *
 * @param type The implementation of the runloop. Must _not_ be "NULL".
 * @param raw The "raw loop pointer", for example ev_base* if type == "event2"
 * @return The runloop.
 * @note Be careful that |raw| must has current |type| otherwise crash happens.
 */
TEN_UTILS_API ten_runloop_t *ten_runloop_attach(const char *type, void *raw);

/**
 * @return true if @a loop is attached to another raw runloop.
 */
TEN_UTILS_API bool ten_runloop_is_attached(ten_runloop_t *loop);

/**
 * @brief Destroy a runloop.
 * @param loop The runloop to destroy.
 * @note Be _very_ careful that if loop is from |ten_runloop_attach|, timers and
 *       events created from |loop| may still be triggered unless you close them
 *       all before detaching.
 */
TEN_UTILS_API void ten_runloop_destroy(ten_runloop_t *loop);

/**
 * @brief Get current loop.
 * @return The current loop. NULL if no loop running.
 */
TEN_UTILS_API ten_runloop_t *ten_runloop_current(void);

/**
 * @brief Get underlying loop object.
 * @param loop The runloop.
 * @return The underlying loop object. NULL if failed.
 *         For example, libevent loop will return event_base*
 */
TEN_UTILS_API void *ten_runloop_get_raw(ten_runloop_t *loop);

/**
 * @brief Run the loop.
 * @param loop The runloop.
 * @note This function will block until the loop is stopped.
 *       If loop is coming from |ten_runloop_attach|, nothing would happen.
 */
TEN_UTILS_API void ten_runloop_run(ten_runloop_t *loop);

/**
 * @brief Stop the loop.
 * @param loop The runloop.
 * @note Stop a loop from |ten_runloop_create| will stop task queue as well as
 *       the underlying io loop.
 *       Stop a loop from |ten_runloop_attach| will only stop task queue.
 */
TEN_UTILS_API void ten_runloop_stop(ten_runloop_t *loop);

typedef void (*ten_runloop_on_stopped_func_t)(ten_runloop_t *loop, void *data);

/**
 * @brief Register a callback function which will be called when @a loop is
 * stopped completely.
 */
TEN_UTILS_API void ten_runloop_set_on_stopped(
    ten_runloop_t *loop, ten_runloop_on_stopped_func_t on_stopped,
    void *on_stopped_data);

/**
 * @brief Close the loop.
 * @param loop The runloop.
 * @note Notify a loop to close its relevant resources before stopping.
 */
TEN_UTILS_API void ten_runloop_close(ten_runloop_t *loop);

/**
 * @brief Check whether the loop is running.
 * @param loop The runloop.
 * @return 1 if the loop is running, 0 otherwise.
 */
TEN_UTILS_API int ten_runloop_alive(ten_runloop_t *loop);

/**
 * @brief Create an async signal
 * @param type The implementation of signal.
 *             Create from a default one if |type| == NULL
 * @return The signal. NULL if failed.
 */
TEN_UTILS_API ten_runloop_async_t *ten_runloop_async_create(const char *type);

/**
 * @brief Close an async signal.
 * @param async The signal.
 * @param close_cb The callback to be called when the signal is closed.
 */
TEN_UTILS_API void ten_runloop_async_close(
    ten_runloop_async_t *async, void (*close_cb)(ten_runloop_async_t *));

/**
 * @brief Destroy an async signal.
 * @param async The signal.
 */
TEN_UTILS_API void ten_runloop_async_destroy(ten_runloop_async_t *async);

/**
 * @brief Notify the signal.
 * @param async The signal.
 * @return 0 if success, -1 otherwise.
 * @note The signal callback will be called in the thread of |loop| that initted
 */
TEN_UTILS_API int ten_runloop_async_notify(ten_runloop_async_t *async);

/**
 * @brief Bind an async signal to a runloop.
 * @param async The signal.
 * @param loop The runloop.
 * @param callback The callback to be called when the signal is notified.
 * @return 0 if success, -1 otherwise.
 * @note The implementation of |loop| should be the same as the implementation
 *       of |async|, otherwise the behavior is undefined.
 */
TEN_UTILS_API int ten_runloop_async_init(
    ten_runloop_async_t *async, ten_runloop_t *loop,
    void (*callback)(ten_runloop_async_t *));

typedef void (*ten_runloop_task_func_t)(void *from, void *arg);

/**
 * @brief Create an async task and insert it in front.
 * @param loop The runloop.
 * @param task_cb The callback to be called when the task is executed.
 * @param from The pointer of poster.
 * @param arg The argument to be passed to the callback.
 * @return 0 if success, -1 otherwise.
 */
TEN_UTILS_API int ten_runloop_post_task_front(ten_runloop_t *loop,
                                              ten_runloop_task_func_t task_cb,
                                              void *from, void *arg);

/**
 * @brief Create an async task and insert it in back.
 * @param loop The runloop.
 * @param task_cb The callback to be called when the task is executed.
 * @param from The pointer of poster.
 * @param arg The argument to be passed to the callback.
 * @return 0 if success, -1 otherwise.
 */
TEN_UTILS_API int ten_runloop_post_task_tail(ten_runloop_t *loop,
                                             ten_runloop_task_func_t task_cb,
                                             void *from, void *arg);

/**
 * @brief Get pending task size
 * @param loop The runloop.
 * @return The pending task size.
 */
TEN_UTILS_API size_t ten_runloop_task_queue_size(ten_runloop_t *loop);

/**
 * @brief Execute all the remaining tasks in the runloop task queue.
 * @param loop The runloop.
 */
TEN_UTILS_API void ten_runloop_flush_task(ten_runloop_t *loop);

/**
 * @brief Create a timer in of a runloop
 * @param type The implementation of timer.
 *             Create from a default one if |type| == NULL
 * @param timeout Timeout time in ms.
 * @param periodic Whether the timer is notified periodicity
 * @return The timer. NULL if failed.
 */
TEN_UTILS_API ten_runloop_timer_t *ten_runloop_timer_create(const char *type,
                                                            uint64_t timeout,
                                                            uint64_t periodic);

/**
 * @brief Set timeout and callback style
 * @param timer The timer.
 * @param timeout Timeout time in ms.
 * @param periodic Whether the timer is notified periodicity.
 * @return 0 if success, -1 if the timer not valid.
 * @note Will not take effect immediately if already started.
 */
TEN_UTILS_API int ten_runloop_timer_set_timeout(ten_runloop_timer_t *timer,
                                                uint64_t timeout,
                                                uint64_t periodic);

/**
 * @brief Bind an timer to a runloop and start.
 * @param timer The timer.
 * @param loop The runloop.
 * @param callback The callback to be called when timer is notified.
 * @return 0 if success, -1 otherwise.
 * @note The implementation of |loop| should be the same as the implementation
 *       of |timer|, otherwise the behavior is undefined.
 */
TEN_UTILS_API int ten_runloop_timer_start(
    ten_runloop_timer_t *timer, ten_runloop_t *loop,
    void (*callback)(ten_runloop_timer_t *, void *), void *arg);

/**
 * @brief Stop an timer.
 * @param timer The timer.
 * @param stop_cb The callback to be called when the timer is stopped.
 */
TEN_UTILS_API void ten_runloop_timer_stop(ten_runloop_timer_t *timer,
                                          void (*stop_cb)(ten_runloop_timer_t *,
                                                          void *),
                                          void *arg);

/**
 * @brief Stop an timer.
 * @param timer The timer.
 * @param close_cb The callback to be called when the timer is stopped.
 */
TEN_UTILS_API void ten_runloop_timer_close(
    ten_runloop_timer_t *timer, void (*close_cb)(ten_runloop_timer_t *, void *),
    void *arg);

/**
 * @brief Destroy an timer.
 * @param timer The timer.
 */
TEN_UTILS_API void ten_runloop_timer_destroy(ten_runloop_timer_t *timer);
