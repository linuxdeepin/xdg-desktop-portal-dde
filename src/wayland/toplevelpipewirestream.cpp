// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "toplevelpipewirestream.h"
#include "loggings.h"
#include "protocols/common.h"
#include "pipewireutils.h"

#include <QGuiApplication>
#include <QScreen>
#include <QtMath>

ToplevelPipeWireStream::ToplevelPipeWireStream(QPointer<ScreenCastContext> context,
                                               PortalCommon::CursorModes mode,
                                               const ToplevelInfoPtr &toplevel,
                                               QObject *parent)
    : AbstractPipeWireStream(context, mode, parent)
    , m_toplevel(toplevel)
{
    uint32_t maximumFramerate = 1;
    foreach (auto screen, QGuiApplication::screens()) {
        maximumFramerate = qMax(maximumFramerate,
                                static_cast<uint32_t>(qMax(1, qRound(screen->refreshRate()))));
    }
    setMaxFramerate(maximumFramerate);

    if (m_toplevel && m_toplevel->handle) {
        connect(m_toplevel->handle, &ForeignToplevelHandle::closed,
                this, &ToplevelPipeWireStream::handleToplevelClosed);
    }
}

ToplevelPipeWireStream::~ToplevelPipeWireStream()
{
    // The capture source extends the toplevel handle. Tear it down while the
    // shared toplevel reference is still alive; the base destructor calls the
    // same idempotent helper as a final safeguard.
    teardown();
}

int ToplevelPipeWireStream::startScreencast()
{
    if (!m_context || !m_toplevel || !m_toplevel->handle
        || !m_context->m_foreignToplevelImageCaptureSourceManager
        || !m_context->m_foreignToplevelImageCaptureSourceManager->isActive()) {
        qCCritical(SCREENCAST) << "xdpw: toplevel image-capture source is unavailable";
        return -1;
    }

    ext_image_capture_source_v1 *source =
            m_context->m_foreignToplevelImageCaptureSourceManager->create_source(
                    m_toplevel->handle->object());
    return initializeCaptureSession(source) ? 0 : -1;
}

void ToplevelPipeWireStream::handleToplevelClosed()
{
    closeForSourceLoss(QStringLiteral("Selected window was closed"));
}
