// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "wayland-wayland-client-protocol.h"

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

#include <QScreen>

namespace PipeWireutils {

enum spa_video_format pipewireFormatStripAlpha(enum spa_video_format format);
uint32_t drmFormatfromWLShmFormat(enum wl_shm_format format);
int32_t wlOutputTransformFromQscreen(const QScreen *screen);
enum spa_video_format pipewireFormatFromDRMFormat(uint32_t format);
enum wl_shm_format wlShmFormatFromDRMFormat(uint32_t format);
int pipewireBPPFromDrmFourcc(uint32_t format);
uint32_t drmFourccFromPipewireFormat(enum spa_video_format format);
}
