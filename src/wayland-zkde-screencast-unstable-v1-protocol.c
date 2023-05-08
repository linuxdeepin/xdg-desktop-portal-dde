// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"

extern const struct wl_interface wl_output_interface;
extern const struct wl_interface zkde_screencast_stream_unstable_v1_interface;

static const struct wl_interface *types[] = {
	NULL,
	&zkde_screencast_stream_unstable_v1_interface,
	&wl_output_interface,
	NULL,
	&zkde_screencast_stream_unstable_v1_interface,
	NULL,
	NULL,
};

static const struct wl_message zkde_screencast_unstable_v1_requests[] = {
	{ "stream_output", "nou", types + 1 },
	{ "stream_window", "nsu", types + 4 },
	{ "destroy", "", types + 0 },
};

WL_EXPORT const struct wl_interface zkde_screencast_unstable_v1_interface = {
	"zkde_screencast_unstable_v1", 1,
	3, zkde_screencast_unstable_v1_requests,
	0, NULL,
};

static const struct wl_message zkde_screencast_stream_unstable_v1_requests[] = {
	{ "close", "", types + 0 },
};

static const struct wl_message zkde_screencast_stream_unstable_v1_events[] = {
	{ "closed", "", types + 0 },
	{ "created", "u", types + 0 },
	{ "failed", "s", types + 0 },
};

WL_EXPORT const struct wl_interface zkde_screencast_stream_unstable_v1_interface = {
	"zkde_screencast_stream_unstable_v1", 1,
	1, zkde_screencast_stream_unstable_v1_requests,
	3, zkde_screencast_stream_unstable_v1_events,
};

