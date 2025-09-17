// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "imagecopycapture.h"


ImageCopyCaptureManager::ImageCopyCaptureManager(QObject *parent)
    : QWaylandClientExtensionTemplate<ImageCopyCaptureManager>(1)
    , QtWayland::ext_image_copy_capture_manager_v1()
{
}

uint32_t ImageCopyCaptureManager::version()
{
    return QtWayland::ext_image_copy_capture_manager_v1::version();
}

ImageCopyCaptureSession::ImageCopyCaptureSession(struct ::ext_image_copy_capture_session_v1 *object, QObject *parent)
    : QObject(parent)
    , QtWayland::ext_image_copy_capture_session_v1(object)
{
}

void ImageCopyCaptureSession::ext_image_copy_capture_session_v1_buffer_size(uint32_t width, uint32_t height)
{
    Q_EMIT bufferSizeChanged(width, height);
}

void ImageCopyCaptureSession::ext_image_copy_capture_session_v1_shm_format(uint32_t format)
{
    Q_EMIT shmFormatChanged(format);
}

void ImageCopyCaptureSession::ext_image_copy_capture_session_v1_dmabuf_device(wl_array *device)
{
    Q_EMIT dmabufDeviceChanged(device);
}

void ImageCopyCaptureSession::ext_image_copy_capture_session_v1_dmabuf_format(uint32_t format, wl_array *modifiers)
{
    Q_EMIT dmabufFormatChanged(format, modifiers);
}

void ImageCopyCaptureSession::ext_image_copy_capture_session_v1_done()
{
    Q_EMIT done();
}

void ImageCopyCaptureSession::ext_image_copy_capture_session_v1_stopped()
{
    Q_EMIT stopped();
}

ImageCopyCaptureFrame::ImageCopyCaptureFrame(struct ::ext_image_copy_capture_frame_v1 *object, QObject *parent)
    : QObject(parent)
    , QtWayland::ext_image_copy_capture_frame_v1(object)
{
}

void ImageCopyCaptureFrame::ext_image_copy_capture_frame_v1_transform(uint32_t transform)
{
    Q_EMIT transformChanged(transform);
}

void ImageCopyCaptureFrame::ext_image_copy_capture_frame_v1_damage(int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_EMIT damaged(x, y, width, height);
}

void ImageCopyCaptureFrame::ext_image_copy_capture_frame_v1_presentation_time(uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
    Q_EMIT presentationTimeChanged(tv_sec_hi, tv_sec_lo, tv_nsec);
}

void ImageCopyCaptureFrame::ext_image_copy_capture_frame_v1_ready()
{
    Q_EMIT ready();
}

void ImageCopyCaptureFrame::ext_image_copy_capture_frame_v1_failed(uint32_t reason)
{
    Q_EMIT failed(reason);
}

ImageCopyCaptureCursorSession::ImageCopyCaptureCursorSession(struct ::ext_image_copy_capture_cursor_session_v1 *object, QObject *parent)
    : QObject(parent)
    , QtWayland::ext_image_copy_capture_cursor_session_v1(object)
{
}

void ImageCopyCaptureCursorSession::ext_image_copy_capture_cursor_session_v1_enter()
{
    Q_EMIT enter();
}

void ImageCopyCaptureCursorSession::ext_image_copy_capture_cursor_session_v1_leave()
{
    Q_EMIT leave();
}

void ImageCopyCaptureCursorSession::ext_image_copy_capture_cursor_session_v1_position(int32_t x, int32_t y)
{
    Q_EMIT positionChanged(x, y);
}

void ImageCopyCaptureCursorSession::ext_image_copy_capture_cursor_session_v1_hotspot(int32_t x, int32_t y)
{
    Q_EMIT hotspotChanged(x, y);
}
