// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "outputpipewirestream.h"
#include "loggings.h"
#include "protocols/common.h"
#include "pipewireutils.h"

#include <QGuiApplication>
#include <QtMath>
#include <qpa/qplatformnativeinterface.h>

OutputPipeWireStream::OutputPipeWireStream(QPointer<ScreenCastContext> context,
                                           PortalCommon::CursorModes mode,
                                           QScreen *output,
                                           QObject *parent)
    : AbstractPipeWireStream(context, mode, parent)
    , m_output(output)
{
    setMaxFramerate(static_cast<uint32_t>(qMax(1, qRound(output->refreshRate()))));
    connect(qApp, &QGuiApplication::screenRemoved, this, &OutputPipeWireStream::handleScreenRemoved);
}

int OutputPipeWireStream::startScreencast()
{
    if (!m_context || !m_output
        || !m_context->m_outputImageCaptureSourceManager
        || !m_context->m_outputImageCaptureSourceManager->isActive()) {
        qCCritical(SCREENCAST) << "xdpw: output image-capture source is unavailable";
        return -1;
    }

    auto *nativeInterface = qGuiApp->platformNativeInterface();
    auto *wlOutput = reinterpret_cast<wl_output *>(
            nativeInterface->nativeResourceForScreen(QByteArrayLiteral("output"), m_output));
    if (!wlOutput) {
        qCCritical(SCREENCAST) << "xdpw: could not find a matching Wayland output";
        return -1;
    }

    ext_image_capture_source_v1 *source =
            m_context->m_outputImageCaptureSourceManager->create_source(wlOutput);
    return initializeCaptureSession(source) ? 0 : -1;
}

void OutputPipeWireStream::handleScreenRemoved(QScreen *screen)
{
    if (screen == m_output) {
        closeForSourceLoss(QStringLiteral("Selected output was removed"));
    }
}
