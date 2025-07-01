// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <time.h>

#include <wayland-util.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*xdpw_event_loop_timer_func_t)(void *data);

struct xdpw_timer {
    struct xdpw_state *state;
    xdpw_event_loop_timer_func_t func;
    void *user_data;
    struct timespec at;
    struct wl_list link; // xdpw_state::timers
};

struct xdpw_state {
    int timer_poll_fd;
    struct wl_list timers;
    struct xdpw_timer *next_timer;
};

struct xdpw_timer *xdpw_add_timer(struct xdpw_state *state, uint64_t delay_ns,
                                  xdpw_event_loop_timer_func_t func,
                                  void *data);
void xdpw_destroy_timer(struct xdpw_timer *timer);

#ifdef __cplusplus
}
#endif
