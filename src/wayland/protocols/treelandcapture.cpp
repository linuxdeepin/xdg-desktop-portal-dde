// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "treelandcapture.h"
#include "common.h"
#include "loggings.h"

#include <algorithm>

void destruct_treeland_capture_manager(TreeLandCaptureManager *manager)
{
    qDeleteAll(manager->captureContexts);
    manager->captureContexts.clear();
}


QPointer<TreeLandCaptureContext> TreeLandCaptureManager::getContext()
{
    auto context = get_context();
    if (!context) {
        qCWarning(PORTAL_COMMON) << "Failed to create Treeland capture context";
        return nullptr;
    }
    auto captureContext = new TreeLandCaptureContext(context);
    if (!captureContext->isInitialized()) {
        delete captureContext;
        return nullptr;
    }
    captureContexts.append(captureContext);
    return captureContext;
}

TreeLandCaptureContext::TreeLandCaptureContext(struct ::treeland_capture_context_v1 *object)
    : QObject()
    , QtWayland::treeland_capture_context_v1(object)
    , m_captureFrame(nullptr)
{}

void TreeLandCaptureContext::treeland_capture_context_v1_source_ready(int32_t region_x, int32_t region_y, uint32_t region_width, uint32_t region_height, uint32_t source_type)
{
    m_captureRegion = QRect(region_x, region_y, region_width, region_height);
    m_sourceType = static_cast<QtWayland::treeland_capture_context_v1::source_type>(source_type);
    Q_EMIT sourceReady(m_captureRegion, source_type);
}

void TreeLandCaptureContext::treeland_capture_context_v1_source_failed(uint32_t reason)
{
    Q_EMIT sourceFailed(reason);
}

QPointer<TreeLandCaptureFrame> TreeLandCaptureContext::frame()
{
    if (m_captureFrame)
        return m_captureFrame;
    auto capture_frame = capture();
    if (!capture_frame) {
        qCWarning(PORTAL_COMMON) << "Failed to create Treeland capture frame";
        return nullptr;
    }
    m_captureFrame = new TreeLandCaptureFrame(capture_frame);
    return m_captureFrame;
}

void TreeLandCaptureContext::selectSource(uint32_t sourceHint, bool freeze, bool withCursor, ::wl_surface *mask)
{
    select_source(sourceHint, freeze, withCursor, mask);
}
void TreeLandCaptureContext::releaseCaptureFrame() {
    if (m_captureFrame) {
        delete m_captureFrame;
        m_captureFrame = nullptr;
    }
}

void TreeLandCaptureFrame::treeland_capture_frame_v1_buffer(uint32_t format, uint32_t width, uint32_t height, uint32_t stride)
{
    if (m_terminal || m_pendingShmBuffer) {
        return;
    }
    if (!isSafeShmBufferLayout(width, height, stride)) {
        qCWarning(PORTAL_COMMON)
                << "Receive a buffer format which is not compatible with QWaylandShmBuffer."
                << "format:" << format << "width:" << width << "height:" << height
                << "stride:" << stride;
        return;
    }

    const QImage::Format imageFormat = QtWaylandClient::QWaylandShm::formatFrom(
            static_cast<::wl_shm_format>(format));
    if (imageFormat == QImage::Format_Invalid) {
        qCWarning(PORTAL_COMMON) << "Unsupported Treeland capture pixel format" << format;
        return;
    }

    m_pendingShmBuffer = new QtWaylandClient::QWaylandShmBuffer(
            waylandDisplay(), QSize(width, height), imageFormat);
    if (!m_pendingShmBuffer->buffer() || !m_pendingShmBuffer->image()
        || m_pendingShmBuffer->image()->isNull()) {
        qCWarning(PORTAL_COMMON) << "Failed to allocate Treeland capture buffer";
        delete m_pendingShmBuffer;
        m_pendingShmBuffer = nullptr;
        return;
    }
}

void TreeLandCaptureFrame::treeland_capture_frame_v1_buffer_done()
{
    if (m_terminal || m_copyRequested) {
        return;
    }
    if (!m_pendingShmBuffer || !m_pendingShmBuffer->buffer()) {
        qCWarning(PORTAL_COMMON) << "Treeland capture offered no usable shared-memory buffer";
        m_terminal = true;
        Q_EMIT failed();
        return;
    }

    m_copyRequested = true;
    copy(m_pendingShmBuffer->buffer());
}

void TreeLandCaptureFrame::treeland_capture_frame_v1_flags(uint32_t flags)
{
    m_flags = flags;
}

void TreeLandCaptureFrame::treeland_capture_frame_v1_ready()
{
    if (m_terminal) {
        return;
    }
    if (!m_copyRequested || !m_pendingShmBuffer || !m_pendingShmBuffer->image()
        || m_pendingShmBuffer->image()->isNull()) {
        qCWarning(PORTAL_COMMON) << "Treeland capture frame is ready without a valid image";
        delete m_pendingShmBuffer;
        m_pendingShmBuffer = nullptr;
        m_terminal = true;
        Q_EMIT failed();
        return;
    }
    if (m_shmBuffer)
        delete m_shmBuffer;
    m_shmBuffer = m_pendingShmBuffer;
    m_pendingShmBuffer = nullptr;
    m_terminal = true;
    const bool yInverted = m_flags & QtWayland::treeland_capture_frame_v1::flags_y_inverted;
    Q_EMIT ready(yInverted ? m_shmBuffer->image()->mirrored(false, true)
                           : *m_shmBuffer->image());
}

void TreeLandCaptureFrame::treeland_capture_frame_v1_failed()
{
    if (m_terminal) {
        return;
    }
    m_terminal = true;
    Q_EMIT failed();
}

void TreeLandCaptureManager::releaseCaptureContext(QPointer<TreeLandCaptureContext> context)
{
    const auto it = std::find(captureContexts.begin(), captureContexts.end(), context.data());
    if (it == captureContexts.end()) {
        return;
    }

    auto entry = *it;
    captureContexts.erase(it);
    entry->deleteLater();
}
