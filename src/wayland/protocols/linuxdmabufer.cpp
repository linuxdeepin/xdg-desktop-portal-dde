// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linuxdmabufer.h"

#define LINUXDMAVERSION 4

LinuxDmaBufV1::LinuxDmaBufV1()
    : QWaylandClientExtensionTemplate<LinuxDmaBufV1>(LINUXDMAVERSION)
{
}

LinuxDmaBufV1::~LinuxDmaBufV1()
{
}

uint32_t LinuxDmaBufV1::version()
{
    return QtWayland::zwp_linux_dmabuf_v1::version();
}

void LinuxDmaBufV1::zwp_linux_dmabuf_v1_format(uint32_t format)
{
    Q_EMIT formatChanged(format);
}

void LinuxDmaBufV1::zwp_linux_dmabuf_v1_modifier(uint32_t format, uint32_t modifier_hi, uint32_t modifier_lo)
{
    Q_EMIT modifierChanged(format, modifier_hi, modifier_lo);
}

LinuxBufferParamsV1::LinuxBufferParamsV1(struct ::zwp_linux_buffer_params_v1 *params, QObject *parent)
    : QObject(parent)
    , QtWayland::zwp_linux_buffer_params_v1(params)
{
}

LinuxBufferParamsV1::~LinuxBufferParamsV1()
{
}

void LinuxBufferParamsV1::zwp_linux_buffer_params_v1_created(wl_buffer *buffer)
{
    Q_EMIT created(buffer);
}

void LinuxBufferParamsV1::zwp_linux_buffer_params_v1_failed()
{
    Q_EMIT failed();
}

LinuxDmaBufFeedbackV1::LinuxDmaBufFeedbackV1(struct ::zwp_linux_dmabuf_feedback_v1 *object, QObject *parent)
    : QObject(parent)
    , QtWayland::zwp_linux_dmabuf_feedback_v1(object)
{
}

LinuxDmaBufFeedbackV1::~LinuxDmaBufFeedbackV1()
{
}

void LinuxDmaBufFeedbackV1::zwp_linux_dmabuf_feedback_v1_done()
{
    Q_EMIT feedbackDone();
}

void LinuxDmaBufFeedbackV1::zwp_linux_dmabuf_feedback_v1_format_table(int32_t fd, uint32_t size)
{
    Q_EMIT formatTableReceived(fd, size);
}

void LinuxDmaBufFeedbackV1::zwp_linux_dmabuf_feedback_v1_main_device(wl_array *device)
{
    Q_EMIT mainDeviceReceived(device);
}

void LinuxDmaBufFeedbackV1::zwp_linux_dmabuf_feedback_v1_tranche_done()
{
    Q_EMIT trancheDone();
}

void LinuxDmaBufFeedbackV1::zwp_linux_dmabuf_feedback_v1_tranche_target_device(wl_array *device)
{
    Q_EMIT trancheTargetDeviceReceived(device);
}

void LinuxDmaBufFeedbackV1::zwp_linux_dmabuf_feedback_v1_tranche_formats(wl_array *indices)
{
    Q_EMIT trancheFormatsReceived(indices);
}

void LinuxDmaBufFeedbackV1::zwp_linux_dmabuf_feedback_v1_tranche_flags(uint32_t flags)
{
    Q_EMIT trancheFlagsReceived(flags);
}
