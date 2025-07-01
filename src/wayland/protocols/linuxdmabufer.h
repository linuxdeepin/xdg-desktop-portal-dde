// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "qwayland-linux-dmabuf-unstable-v1.h"

#include <QObject>
#include <QWaylandClientExtension>

class LinuxDmaBufV1
    : public QWaylandClientExtensionTemplate<LinuxDmaBufV1>
    , public QtWayland::zwp_linux_dmabuf_v1
{
    Q_OBJECT
public:
    LinuxDmaBufV1();
    ~LinuxDmaBufV1() override;
    uint32_t version();

protected:
    void zwp_linux_dmabuf_v1_format(uint32_t format) override;
    void zwp_linux_dmabuf_v1_modifier(uint32_t format,
                                      uint32_t modifier_hi,
                                      uint32_t modifier_lo) override;

Q_SIGNALS:
    void formatChanged(uint32_t format);
    void modifierChanged(uint32_t format,
                         uint32_t modifierHigh,
                         uint32_t modifierLow);
};

class LinuxBufferParamsV1
    : public QObject
    , public QtWayland::zwp_linux_buffer_params_v1
{
    Q_OBJECT
public:
    LinuxBufferParamsV1(struct ::zwp_linux_buffer_params_v1 *object,
                        QObject *parent = nullptr);
    ~LinuxBufferParamsV1() override;

protected:
    void zwp_linux_buffer_params_v1_created(struct ::wl_buffer *buffer) override;
    void zwp_linux_buffer_params_v1_failed() override;

Q_SIGNALS:
    void created(wl_buffer *buffer);
    void failed();
};

class LinuxDmaBufFeedbackV1
    : public QObject
    , public QtWayland::zwp_linux_dmabuf_feedback_v1
{
    Q_OBJECT
public:
    LinuxDmaBufFeedbackV1(struct ::zwp_linux_dmabuf_feedback_v1 *object,
                          QObject *parent = nullptr);
    ~LinuxDmaBufFeedbackV1() override;

protected:
    void zwp_linux_dmabuf_feedback_v1_done() override;
    void zwp_linux_dmabuf_feedback_v1_format_table(int32_t fd,uint32_t size) override;
    void zwp_linux_dmabuf_feedback_v1_main_device(wl_array *device) override;
    void zwp_linux_dmabuf_feedback_v1_tranche_done() override;
    void zwp_linux_dmabuf_feedback_v1_tranche_target_device(wl_array *device) override;
    void zwp_linux_dmabuf_feedback_v1_tranche_formats(wl_array *indices) override;
    void zwp_linux_dmabuf_feedback_v1_tranche_flags(uint32_t flags) override;

Q_SIGNALS:
    void feedbackDone();
    void formatTableReceived(int fd, uint size);
    void mainDeviceReceived(const wl_array *device);
    void trancheDone();
    void trancheTargetDeviceReceived(const wl_array *device);
    void trancheFormatsReceived(const wl_array *indices);
    void trancheFlagsReceived(uint flags);
};
