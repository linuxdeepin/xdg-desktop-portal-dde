// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "outputpipewirestream.h"
#include "loggings.h"
#include "protocols/common.h"
#include "pipewireutils.h"

#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

OutputPipeWireStream::OutputPipeWireStream(QPointer<ScreenCastContext> context,
                                           PortalCommon::CursorModes mode,
                                           QScreen *output,
                                           QObject *parent)
    : AbstractPipeWireStream(context, mode, parent)
    , m_output(output)
{
    m_framerate = output->refreshRate();
    connect(qApp, &QGuiApplication::screenRemoved, this, &OutputPipeWireStream::handleScreenRemoved);
}

int OutputPipeWireStream::startScreencast()
{
    if (!m_source) {
        auto nativeInterface = qGuiApp->platformNativeInterface();
        auto *wlOutput = reinterpret_cast<wl_output *>(
                nativeInterface->nativeResourceForScreen(QByteArrayLiteral("output"), m_output));
        if (!wlOutput) {
            qCCritical(SCREENCAST) << "Could not find a matching Wayland output for screen";
            return -1;
        }

        m_source = m_context->m_outputImageCaptureSourceManager->create_source(wlOutput);
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

void OutputPipeWireStream::startframeCapture()
{
    if (!m_context) {
        return;
    }

    if (!m_context->m_outputImageCaptureSourceManager->isActive()) {
        return;
    }

    frameCapture();
}

void OutputPipeWireStream::handleScreenRemoved(QScreen *screen)
{
    if (screen == m_output) {
        Q_EMIT closed(nodeId());
    }
}
