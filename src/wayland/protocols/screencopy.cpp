// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screencopy.h"

ScreenCopyFrame::ScreenCopyFrame(struct ::zwlr_screencopy_frame_v1 *object)
    : QObject(nullptr)
    , QtWayland::zwlr_screencopy_frame_v1(object)
{ }

void ScreenCopyFrame::zwlr_screencopy_frame_v1_buffer(uint32_t format,
                                                      uint32_t width,
                                                      uint32_t height,
                                                      uint32_t stride)
{
    Q_EMIT buffer(format, width, height, stride);
}

void ScreenCopyFrame::zwlr_screencopy_frame_v1_flags(uint32_t f)
{
    Q_EMIT frameFlags(f);
}

void ScreenCopyFrame::zwlr_screencopy_frame_v1_failed()
{
    Q_EMIT failed();
}

void ScreenCopyFrame::zwlr_screencopy_frame_v1_damage(uint32_t x,
                                                      uint32_t y,
                                                      uint32_t width,
                                                      uint32_t height)
{
    Q_EMIT damage(x, y, width, height);
}

void ScreenCopyFrame::zwlr_screencopy_frame_v1_linux_dmabuf(uint32_t format,
                                                            uint32_t width,
                                                            uint32_t height)
{
    Q_EMIT linuxDmabuf(format, width, height);
}

void ScreenCopyFrame::zwlr_screencopy_frame_v1_buffer_done()
{
    Q_EMIT bufferDone();
}

void ScreenCopyFrame::zwlr_screencopy_frame_v1_ready(uint32_t tv_sec_hi,
                                                     uint32_t tv_sec_lo,
                                                     uint32_t tv_nsec)
{
    Q_EMIT ready(tv_sec_hi, tv_sec_lo, tv_nsec);
}

ScreenCopyManager::ScreenCopyManager(QObject *parent)
    : QWaylandClientExtensionTemplate<ScreenCopyManager>(3)
    , QtWayland::zwlr_screencopy_manager_v1()
{ }

uint32_t ScreenCopyManager::version()
{
    return QtWayland::zwlr_screencopy_manager_v1::version();
}

QPointer<ScreenCopyFrame> ScreenCopyManager::captureOutput(
        int32_t overlay_cursor,
        struct ::wl_output *output)
{
    auto screen_copy_frame = capture_output(overlay_cursor, output);
    auto screenCopyFrame = new ScreenCopyFrame(screen_copy_frame);
    return screenCopyFrame;
}

QPointer<ScreenCopyFrame> ScreenCopyManager::captureOutputRegion(
        int32_t overlay_cursor,
        struct ::wl_output *output,
        int32_t x,
        int32_t y,
        int32_t width,
        int32_t height)
{
    auto screen_copy_frame = capture_output_region(overlay_cursor,
                                                   output,
                                                   x,
                                                   y,
                                                   width,
                                                   height);
    auto screenCopyFrame = new ScreenCopyFrame(screen_copy_frame);
    return screenCopyFrame;
}
