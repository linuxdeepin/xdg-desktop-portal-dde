// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <private/qwaylandclientextension_p.h>
#include <qwayland-ext-image-copy-capture-v1.h>

class ImageCopyCaptureManager
    : public QWaylandClientExtensionTemplate<ImageCopyCaptureManager>
    , public QtWayland::ext_image_copy_capture_manager_v1
{
    Q_OBJECT
public:
    ImageCopyCaptureManager(QObject *parent = nullptr);
    uint32_t version();
};

class ImageCopyCaptureSession
    : public QObject
    , public QtWayland::ext_image_copy_capture_session_v1
{
    Q_OBJECT
public:
    ImageCopyCaptureSession(struct ::ext_image_copy_capture_session_v1 *object, QObject *parent = nullptr);

Q_SIGNALS:
    void bufferSizeChanged(uint32_t width, uint32_t height);
    void shmFormatChanged(uint32_t format);
    void dmabufDeviceChanged(wl_array *device);
    void dmabufFormatChanged(uint32_t format, wl_array *modifiers);
    void done();
    void stopped();

protected:
    void ext_image_copy_capture_session_v1_buffer_size(uint32_t width, uint32_t height) override;
    void ext_image_copy_capture_session_v1_shm_format(uint32_t format) override;
    void ext_image_copy_capture_session_v1_dmabuf_device(wl_array *device) override;
    void ext_image_copy_capture_session_v1_dmabuf_format(uint32_t format, wl_array *modifiers) override;
    void ext_image_copy_capture_session_v1_done() override;
    void ext_image_copy_capture_session_v1_stopped() override;
};

class ImageCopyCaptureFrame
    : public QObject
    , public QtWayland::ext_image_copy_capture_frame_v1
{
    Q_OBJECT
public:
    ImageCopyCaptureFrame(struct ::ext_image_copy_capture_frame_v1 *object, QObject *parent = nullptr);

Q_SIGNALS:
    void transformChanged(uint32_t transform);
    void damaged(int32_t x, int32_t y, int32_t width, int32_t height);
    void presentationTimeChanged(uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec);
    void ready();
    void failed(uint32_t reason);

protected:
    void ext_image_copy_capture_frame_v1_transform(uint32_t transform) override;
    void ext_image_copy_capture_frame_v1_damage(int32_t x, int32_t y, int32_t width, int32_t height) override;
    void ext_image_copy_capture_frame_v1_presentation_time(uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) override;
    void ext_image_copy_capture_frame_v1_ready() override;
    void ext_image_copy_capture_frame_v1_failed(uint32_t reason) override;
};

class ImageCopyCaptureCursorSession
    : public QObject
    , QtWayland::ext_image_copy_capture_cursor_session_v1
{
    Q_OBJECT
public:
    ImageCopyCaptureCursorSession(struct ::ext_image_copy_capture_cursor_session_v1 *object, QObject *parent = nullptr);

Q_SIGNALS:
    void enter();
    void leave();
    void positionChanged(int32_t x, int32_t y);
    void hotspotChanged(int32_t x, int32_t y);

protected:
    void ext_image_copy_capture_cursor_session_v1_enter() override;
    void ext_image_copy_capture_cursor_session_v1_leave() override;
    void ext_image_copy_capture_cursor_session_v1_position(int32_t x, int32_t y) override;
    void ext_image_copy_capture_cursor_session_v1_hotspot(int32_t x, int32_t y) override;
};
