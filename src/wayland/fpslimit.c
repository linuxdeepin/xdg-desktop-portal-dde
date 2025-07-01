// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "fpslimit.h"

#include <assert.h>
#include <stdbool.h>

#define TIMESPEC_NSEC_PER_SEC 1000000000L
#define FPS_MEASURE_PERIOD_SEC 5.0

static bool timespec_is_zero(struct timespec *t) {
    return t->tv_sec == 0 && t->tv_nsec == 0;
}

static int64_t timespec_diff_ns(struct timespec *t1, struct timespec *t2) {
    int64_t s = t1->tv_sec - t2->tv_sec;
    int64_t ns = t1->tv_nsec - t2->tv_nsec;

    return s * TIMESPEC_NSEC_PER_SEC + ns;
}

static void measure_fps(struct fps_limit_state *state, struct timespec *now) {
    if (timespec_is_zero(&state->fps_last_time)) {
        state->fps_last_time = *now;
        return;
    }

    state->fps_frame_count++;

    int64_t elapsed_ns = timespec_diff_ns(now, &state->fps_last_time);

    double elapsed_sec = (double) elapsed_ns / (double) TIMESPEC_NSEC_PER_SEC;
    if (elapsed_sec < FPS_MEASURE_PERIOD_SEC) {
        return;
    }

    double avg_frames_per_sec = state->fps_frame_count / elapsed_sec;

    state->fps_last_time = *now;
    state->fps_frame_count = 0;
}

void fps_limit_measure_start(struct fps_limit_state *state, double max_fps)
{
    if (max_fps <= 0.0) {
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &state->frame_last_time);
}

uint64_t fps_limit_measure_end(struct fps_limit_state *state, double max_fps)
{
    if (max_fps <= 0.0) {
        return 0;
    }

    assert(!timespec_is_zero(&state->frame_last_time));

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t elapsed_ns = timespec_diff_ns(&now, &state->frame_last_time);

    measure_fps(state, &now);

    int64_t target_ns = (1.0 / max_fps) * TIMESPEC_NSEC_PER_SEC;
    int64_t delay_ns = target_ns - elapsed_ns;
    if (delay_ns > 0) {
        return delay_ns;
    } else {
        return 0;
    }
}
