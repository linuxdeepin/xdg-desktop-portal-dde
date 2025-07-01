// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

struct fps_limit_state {
    struct timespec frame_last_time;

    struct timespec fps_last_time;
    uint64_t fps_frame_count;
};

void fps_limit_measure_start(struct fps_limit_state *state,
                             double max_fps);
uint64_t fps_limit_measure_end(struct fps_limit_state *state,
                               double max_fps);

#ifdef __cplusplus
}
#endif
