// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "toplevelpipewirestream.h"
#include "loggings.h"
#include "protocols/common.h"
#include "pipewireutils.h"

#include <QGuiApplication>
#include <QScreen>

ToplevelPipeWireStream::ToplevelPipeWireStream(QPointer<ScreenCastContext> context,
                                               PortalCommon::CursorModes mode,
                                               ToplevelInfo *toplevel,
                                               QObject *parent)
    : AbstractPipeWireStream(context, mode, parent)
    , m_toplevel(toplevel)
{
    foreach (auto screen, QGuiApplication::screens()) {
        qreal refreshRate = screen->refreshRate();
        if (refreshRate > m_framerate) {
            m_framerate = refreshRate;
        }
    }
}

int ToplevelPipeWireStream::startScreencast()
{
    if (!m_source) {
        m_source = m_context->m_foreignToplevelImageCaptureSourceManager->create_source(m_toplevel->handle->object());
        m_session = new ImageCopyCaptureSession(m_context->m_imageCopyCaptureManager->create_session(m_source,
                                                                                                     m_mode == PortalCommon::CursorModes::Embedded ?EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS : 0));
        connect(m_session, &ImageCopyCaptureSession::bufferSizeChanged, this,
                &AbstractPipeWireStream::handleCaptureSessionBufferSizeChanged);
        connect(m_session, &ImageCopyCaptureSession::shmFormatChanged, this,
                &AbstractPipeWireStream::handleCaptureSessionShmFormatChanged);
        connect(m_session, &ImageCopyCaptureSession::dmabufDeviceChanged, this,
                &AbstractPipeWireStream::handleCaptureSessionDmabufDeviceChanged);
        connect(m_session, &ImageCopyCaptureSession::dmabufFormatChanged, this,
                &AbstractPipeWireStream::handleCaptureSessionDmabufFormatChanged);
        connect(m_session, &ImageCopyCaptureSession::done, this,
                &AbstractPipeWireStream::handleCaptureSessionDone);
        connect(m_session, &ImageCopyCaptureSession::stopped, this,
                &AbstractPipeWireStream::handleCaptureSessionStopped);
    }
    wl_display_dispatch(waylandDisplay()->wl_display());
    wl_display_roundtrip(waylandDisplay()->wl_display());
    createStream();
    m_initialized = true;

    return 0;
}

void ToplevelPipeWireStream::startframeCapture()
{
    if (!m_context) {
        return;
    }

    if (!m_context->m_foreignToplevelImageCaptureSourceManager->isActive()) {
        return;
    }

    frameCapture();
}
