// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "pipewiretimer.h"

#include <stdlib.h>
#include <stdbool.h>
#include <poll.h>
#include <sys/timerfd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define TIMESPEC_NSEC_PER_SEC 1000000000L

static void timespec_add(struct timespec *t, int64_t delta_ns) {
    int delta_ns_low = delta_ns % TIMESPEC_NSEC_PER_SEC;
    int delta_s_high = delta_ns / TIMESPEC_NSEC_PER_SEC;

    t->tv_sec += delta_s_high;

    t->tv_nsec += (long)delta_ns_low;
    if (t->tv_nsec >= TIMESPEC_NSEC_PER_SEC) {
        t->tv_nsec -= TIMESPEC_NSEC_PER_SEC;
        ++t->tv_sec;
    }
}

static bool timespec_less(struct timespec *t1, struct timespec *t2) {
    if (t1->tv_sec != t2->tv_sec) {
        return t1->tv_sec < t2->tv_sec;
    }
    return t1->tv_nsec < t2->tv_nsec;
}

static void update_timer(struct xdpw_state *state) {
    int timer_fd = state->timer_poll_fd;
    if (timer_fd < 0) {
        return;
    }
    bool updated = false;
    struct xdpw_timer *timer, *tmp;
    wl_list_for_each_safe(timer, tmp, &state->timers, link) {
        if (state->next_timer == NULL ||
            timespec_less(&timer->at, &state->next_timer->at)) {
            state->next_timer = timer;
            updated = true;
        }
    }

    if (updated) {
        struct itimerspec delay = { .it_value = state->next_timer->at };
        errno = 0;
        int ret = timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &delay, NULL);
        if (ret < 0) {
            fprintf(stderr, "failed to timerfd_settime(): %s\n",
                    strerror(errno));
        }
    }
}

struct xdpw_timer *xdpw_add_timer(struct xdpw_state *state, uint64_t delay_ns, xdpw_event_loop_timer_func_t func, void *data)
{
    struct xdpw_timer *timer = calloc(1, sizeof(struct xdpw_timer));
    if (timer == NULL) {
        return NULL;
    }
    timer->state = state;
    timer->func = func;
    timer->user_data = data;
    wl_list_insert(&state->timers, &timer->link);

    clock_gettime(CLOCK_MONOTONIC, &timer->at);
    timespec_add(&timer->at, delay_ns);

    update_timer(state);
    return timer;
}

void xdpw_destroy_timer(struct xdpw_timer *timer)
{
    if (timer == NULL) {
        return;
    }
    struct xdpw_state *state = timer->state;

    if (state->next_timer == timer) {
        state->next_timer = NULL;
    }

    wl_list_remove(&timer->link);
    free(timer);

    update_timer(state);
}
