// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <private/qwaylandclientextension_p.h>
#include <qwayland-wlr-screencopy-unstable-v1.h>

struct ScreenCopyFrameInfo {
    uint32_t width;
    uint32_t height;
    uint32_t size;
    uint32_t stride;
    uint32_t format;
};

class ScreenCopyFrame : public QObject, public QtWayland::zwlr_screencopy_frame_v1
{
    Q_OBJECT
public:
    ScreenCopyFrame(struct ::zwlr_screencopy_frame_v1 *object);

Q_SIGNALS:
    void buffer(uint32_t format, uint32_t width, uint32_t height, uint32_t stride);
    void frameFlags(uint32_t flags);
    void ready(uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec);
    void failed();
    void damage(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void linuxDmabuf(uint32_t format, uint32_t width, uint32_t height);
    void bufferDone();

protected:
    void zwlr_screencopy_frame_v1_buffer(uint32_t format,
                                         uint32_t width,
                                         uint32_t height,
                                         uint32_t stride) override;
    void zwlr_screencopy_frame_v1_flags(uint32_t flags) override;
    void zwlr_screencopy_frame_v1_ready(uint32_t tv_sec_hi,
                                        uint32_t tv_sec_lo,
                                        uint32_t tv_nsec) override;
    void zwlr_screencopy_frame_v1_failed() override;
    void zwlr_screencopy_frame_v1_damage(uint32_t x,
                                         uint32_t y,
                                         uint32_t width,
                                         uint32_t height) override;
    void zwlr_screencopy_frame_v1_linux_dmabuf(uint32_t format,
                                               uint32_t width,
                                               uint32_t height) override;
    void zwlr_screencopy_frame_v1_buffer_done() override;
};


class ScreenCopyManager
    : public QWaylandClientExtensionTemplate<ScreenCopyManager>
    , public QtWayland::zwlr_screencopy_manager_v1
{
    Q_OBJECT
public:
    ScreenCopyManager(QObject *parent = nullptr);
    uint32_t version();

    QPointer<ScreenCopyFrame> captureOutput(int32_t overlay_cursor,
                                            struct ::wl_output *output);
    QPointer<ScreenCopyFrame> captureOutputRegion(int32_t overlay_cursor,
                                                  struct ::wl_output *output,
                                                  int32_t x,
                                                  int32_t y,
                                                  int32_t width,
                                                  int32_t height);
};
