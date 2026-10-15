// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screenshotportal.h"

#include "dbushelpers.h"
#include "loggings.h"
#include "portalwaylandcontext.h"
#include "protocols/common.h"
#include "protocols/screencopy.h"
#include "protocols/treelandcapture.h"
#include "request2.h"
#include "screenshotimage.h"

#include <QDateTime>
#include <QDBusConnection>
#include <QDir>
#include <QPointer>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

#include <private/qwaylanddisplay_p.h>
#include <private/qwaylandscreen_p.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace
{

constexpr int CaptureTimeoutMs = 10000;

class OutputTransformObserver final : public QtWayland::wl_output
{
public:
    OutputTransformObserver(QtWaylandClient::QWaylandDisplay *display, uint32_t outputId)
    {
        if (!display) {
            return;
        }

        const auto globals = display->globals();
        const auto global = std::find_if(globals.cbegin(), globals.cend(), [outputId](const auto &entry) {
            return entry.id == outputId && entry.interface == QStringLiteral("wl_output");
        });
        if (global != globals.cend()) {
            init(display->wl_registry(), global->id, std::min(global->version, uint32_t(4)));
        }
    }

    ~OutputTransformObserver() override
    {
        if (!isInitialized()) {
            return;
        }
        if (version() >= 3) {
            release();
        } else {
            ::wl_output_destroy(object());
        }
    }

    int transform() const { return m_transform; }
    bool receivedTransform() const { return m_receivedTransform; }

protected:
    void output_geometry(int32_t,
                         int32_t,
                         int32_t,
                         int32_t,
                         int32_t,
                         const QString &,
                         const QString &,
                         int32_t transform) override
    {
        if (transform >= transform_normal && transform <= transform_flipped_270) {
            m_transform = transform;
            m_receivedTransform = true;
        }
    }

private:
    int m_transform = transform_normal;
    bool m_receivedTransform = false;
};

struct ScreenCapture
{
    QString name;
    QRect logicalGeometry;
    std::unique_ptr<OutputTransformObserver> transformObserver;
    std::unique_ptr<QtWaylandClient::QWaylandShmBuffer> buffer;
    std::unique_ptr<ScreenCopyFrame> frame;
    uint32_t flags = 0;
    bool waitForBufferDone = false;
    bool copyRequested = false;
    bool finished = false;
};

QString saveScreenshot(const QImage &image)
{
    if (image.isNull()) {
        return {};
    }

    const QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesPath.isEmpty()) {
        qCWarning(SCREESHOT) << "No writable Pictures location is available";
        return {};
    }

    QDir picturesDir(picturesPath);
    if (!picturesDir.exists() && !picturesDir.mkpath(QStringLiteral("."))) {
        qCWarning(SCREESHOT) << "Failed to create Pictures directory" << picturesPath;
        return {};
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    QTemporaryFile file(picturesDir.filePath(
            QStringLiteral("Screenshot_%1_XXXXXX.png").arg(timestamp)));
    if (!file.open()) {
        qCWarning(SCREESHOT) << "Failed to create screenshot file in" << picturesPath
                             << file.errorString();
        return {};
    }
    if (!image.save(&file, "PNG") || !file.flush()) {
        qCWarning(SCREESHOT) << "Failed to save screenshot" << file.errorString();
        return {};
    }

    file.setAutoRemove(false);
    return file.fileName();
}

PortalResponse::Response responseFromSourceFailure(uint32_t reason)
{
    using CaptureContext = QtWayland::treeland_capture_context_v1;
    return reason == CaptureContext::source_failure_user_cancel
            ? PortalResponse::Cancelled
            : PortalResponse::OtherError;
}

class ScreenshotOperation final : public QObject
{
public:
    ScreenshotOperation(PortalWaylandContext *context,
                        const QDBusObjectPath &handle,
                        const QDBusMessage &message,
                        bool interactive,
                        QObject *parent)
        : QObject(parent)
        , m_context(context)
        , m_message(message)
        , m_interactive(interactive)
        , m_request(new Request2(handle, this, QStringLiteral("Screenshot")))
    {
        m_captureTimeout.setSingleShot(true);
        connect(&m_captureTimeout, &QTimer::timeout, this, [this] {
            qCWarning(SCREESHOT) << "Screenshot capture timed out";
            finish(PortalResponse::OtherError);
        });
        connect(m_request, &Request2::closeRequested, this, [this] {
            finish(PortalResponse::Cancelled);
        });
    }

    ~ScreenshotOperation() override
    {
        releaseInteractiveContext();
    }

    void start()
    {
        if (!m_request->isRegistered()) {
            finish(PortalResponse::OtherError);
            return;
        }

        if (m_interactive) {
            startInteractiveCapture();
        } else {
            startFullscreenCapture();
        }
    }

private:
    void startFullscreenCapture()
    {
        auto manager = m_context ? m_context->screenCopyManager() : nullptr;
        if (!manager || !manager->isActive()) {
            qCWarning(SCREESHOT) << "Screen copy manager is not active";
            finish(PortalResponse::OtherError);
            return;
        }

        auto display = waylandDisplay();
        const auto screens = display ? display->screens() : QList<QtWaylandClient::QWaylandScreen *>();
        if (screens.isEmpty()) {
            qCWarning(SCREESHOT) << "No Wayland outputs are available";
            finish(PortalResponse::OtherError);
            return;
        }

        m_pendingOutputs = screens.size();
        m_outputs.reserve(screens.size());
        for (auto screen : screens) {
            auto capture = std::make_shared<ScreenCapture>();
            capture->name = screen->name();
            capture->logicalGeometry = screen->geometry();
            capture->waitForBufferDone = manager->version() >= 3;
            capture->transformObserver = std::make_unique<OutputTransformObserver>(
                    display, screen->outputId());

            auto frame = manager->captureOutput(false, screen->output());
            if (!frame || !frame->isInitialized()) {
                qCWarning(SCREESHOT) << "Failed to create screencopy frame for" << capture->name;
                finish(PortalResponse::OtherError);
                return;
            }
            capture->frame.reset(frame.data());
            m_outputs.append(capture);

            connect(capture->frame.get(), &ScreenCopyFrame::buffer, this,
                    [this, capture, display](uint32_t format,
                                             uint32_t width,
                                             uint32_t height,
                                             uint32_t stride) {
                if (m_finished || capture->finished || capture->buffer) {
                    return;
                }
                if (!isSafeShmBufferLayout(width, height, stride)) {
                    qCWarning(SCREESHOT) << "Unsupported screencopy buffer for" << capture->name
                                         << format << width << height << stride;
                    if (!capture->waitForBufferDone) {
                        finishOutput(capture, false);
                    }
                    return;
                }

                const QImage::Format imageFormat = QtWaylandClient::QWaylandShm::formatFrom(
                        static_cast<::wl_shm_format>(format));
                if (imageFormat == QImage::Format_Invalid) {
                    qCWarning(SCREESHOT) << "Unsupported screencopy pixel format" << format;
                    if (!capture->waitForBufferDone) {
                        finishOutput(capture, false);
                    }
                    return;
                }

                capture->buffer = std::make_unique<QtWaylandClient::QWaylandShmBuffer>(
                        display, QSize(width, height), imageFormat);
                if (!capture->buffer->buffer() || !capture->buffer->image()
                    || capture->buffer->image()->isNull()) {
                    qCWarning(SCREESHOT) << "Failed to allocate screencopy buffer for"
                                         << capture->name;
                    capture->buffer.reset();
                    if (!capture->waitForBufferDone) {
                        finishOutput(capture, false);
                    }
                    return;
                }
                if (!capture->waitForBufferDone) {
                    capture->copyRequested = true;
                    capture->frame->copy(capture->buffer->buffer());
                }
            });
            connect(capture->frame.get(), &ScreenCopyFrame::bufferDone, this,
                    [this, capture] {
                if (m_finished || capture->finished || capture->copyRequested) {
                    return;
                }
                if (!capture->buffer || !capture->buffer->buffer()) {
                    qCWarning(SCREESHOT) << "No usable screencopy shared-memory buffer for"
                                         << capture->name;
                    finishOutput(capture, false);
                    return;
                }
                capture->copyRequested = true;
                capture->frame->copy(capture->buffer->buffer());
            });
            connect(capture->frame.get(), &ScreenCopyFrame::frameFlags, this,
                    [capture](uint32_t flags) {
                capture->flags = flags;
            });
            connect(capture->frame.get(), &ScreenCopyFrame::ready, this,
                    [this, capture](uint32_t, uint32_t, uint32_t) {
                const QImage *image = capture->buffer ? capture->buffer->image() : nullptr;
                finishOutput(capture, image && !image->isNull());
            });
            connect(capture->frame.get(), &ScreenCopyFrame::failed, this,
                    [this, capture] {
                finishOutput(capture, false);
            });
        }

        m_captureTimeout.start(CaptureTimeoutMs);
    }

    void finishOutput(const std::shared_ptr<ScreenCapture> &capture, bool succeeded)
    {
        if (m_finished || capture->finished) {
            return;
        }
        capture->finished = true;
        if (!succeeded) {
            qCWarning(SCREESHOT) << "Screencopy failed for" << capture->name;
            finish(PortalResponse::OtherError);
            return;
        }

        if (--m_pendingOutputs != 0) {
            return;
        }
        m_captureTimeout.stop();

        QList<ScreenshotImage::Output> outputs;
        outputs.reserve(m_outputs.size());
        for (const auto &entry : std::as_const(m_outputs)) {
            const bool yInverted = entry->flags
                    & QtWayland::zwlr_screencopy_frame_v1::flags_y_invert;
            const int transform = entry->transformObserver
                    ? entry->transformObserver->transform()
                    : int(QtWayland::wl_output::transform_normal);
            if (entry->transformObserver && !entry->transformObserver->receivedTransform()) {
                qCWarning(SCREESHOT) << "No wl_output transform received for" << entry->name
                                     << "; assuming normal";
            }
            const QImage normalized = ScreenshotImage::normalizeOutput(
                    *entry->buffer->image(), transform, yInverted);
            if (normalized.isNull()) {
                finish(PortalResponse::OtherError);
                return;
            }
            outputs.append({entry->logicalGeometry, normalized, entry->name});
        }

        QString errorMessage;
        const QImage image = ScreenshotImage::composeOutputs(outputs, &errorMessage);
        if (image.isNull()) {
            qCWarning(SCREESHOT) << "Failed to compose screenshot:" << errorMessage;
            finish(PortalResponse::OtherError);
            return;
        }
        finish(PortalResponse::Success, image);
    }

    void startInteractiveCapture()
    {
        m_captureManager = m_context ? m_context->treelandCaptureManager() : nullptr;
        if (!m_captureManager || !m_captureManager->isActive()) {
            qCWarning(SCREESHOT) << "Treeland capture manager is not active";
            finish(PortalResponse::OtherError);
            return;
        }

        m_captureContext = m_captureManager->getContext();
        if (!m_captureContext || !m_captureContext->isInitialized()) {
            finish(PortalResponse::OtherError);
            return;
        }

        connect(m_captureManager, &TreeLandCaptureManager::activeChanged, this, [this] {
            if (!m_captureManager || !m_captureManager->isActive()) {
                finish(PortalResponse::OtherError);
            }
        });
        connect(m_captureContext, &TreeLandCaptureContext::sourceReady, this,
                [this](const QRect &, uint32_t) {
            startInteractiveFrame();
        });
        connect(m_captureContext, &TreeLandCaptureContext::sourceFailed, this,
                [this](uint32_t reason) {
            qCWarning(SCREESHOT) << "Treeland source selection failed with reason" << reason;
            finish(responseFromSourceFailure(reason));
        });

        m_captureContext->selectSource(
                QtWayland::treeland_capture_context_v1::source_type_output
                        | QtWayland::treeland_capture_context_v1::source_type_window
                        | QtWayland::treeland_capture_context_v1::source_type_region,
                true,
                false,
                nullptr);
    }

    void startInteractiveFrame()
    {
        if (m_finished || m_interactiveFrameStarted || !m_captureContext) {
            return;
        }
        m_interactiveFrameStarted = true;
        auto frame = m_captureContext->frame();
        if (!frame || !frame->isInitialized()) {
            finish(PortalResponse::OtherError);
            return;
        }

        connect(frame, &TreeLandCaptureFrame::ready, this, [this](const QImage &image) {
            if (image.isNull()) {
                finish(PortalResponse::OtherError);
                return;
            }
            finish(PortalResponse::Success, image);
        });
        connect(frame, &TreeLandCaptureFrame::failed, this, [this] {
            finish(PortalResponse::OtherError);
        });
        m_captureTimeout.start(CaptureTimeoutMs);
    }

    void releaseInteractiveContext()
    {
        if (m_captureManager && m_captureContext) {
            m_captureManager->releaseCaptureContext(m_captureContext);
        }
        m_captureContext.clear();
        m_captureManager.clear();
    }

    void finish(PortalResponse::Response response, const QImage &image = {})
    {
        if (m_finished) {
            return;
        }
        m_finished = true;
        m_captureTimeout.stop();
        releaseInteractiveContext();

        QVariantMap results;
        if (response == PortalResponse::Success) {
            const QString filePath = saveScreenshot(image);
            if (filePath.isEmpty()) {
                response = PortalResponse::OtherError;
            } else {
                results.insert(QStringLiteral("uri"),
                               QUrl::fromLocalFile(filePath).toString(QUrl::FullyEncoded));
            }
        }

        if (m_request) {
            m_request->unregister();
        }
        const QDBusMessage reply = m_message.createReply(
                QVariantList{QVariant::fromValue(uint(response)), results});
        if (!QDBusConnection::sessionBus().send(reply)) {
            qCWarning(SCREESHOT) << "Failed to send screenshot response";
        }
        if (m_request) {
            m_request->deleteLater();
        }
        deleteLater();
    }

    PortalWaylandContext *m_context = nullptr;
    QDBusMessage m_message;
    bool m_interactive = false;
    bool m_interactiveFrameStarted = false;
    bool m_finished = false;
    QPointer<Request2> m_request;
    QTimer m_captureTimeout;
    QList<std::shared_ptr<ScreenCapture>> m_outputs;
    qsizetype m_pendingOutputs = 0;
    QPointer<TreeLandCaptureManager> m_captureManager;
    QPointer<TreeLandCaptureContext> m_captureContext;
};

}

ScreenshotPortalWayland::ScreenshotPortalWayland(PortalWaylandContext *context)
    : AbstractWaylandPortal(context)
{
}

uint ScreenshotPortalWayland::PickColor(const QDBusObjectPath &handle,
                                        const QString &app_id,
                                        const QString &parent_window,
                                        const QVariantMap &options,
                                        QVariantMap &results)
{
    Q_UNUSED(handle)
    Q_UNUSED(app_id)
    Q_UNUSED(parent_window)
    Q_UNUSED(options)
    Q_UNUSED(results)
    return PortalResponse::Cancelled;
}

void ScreenshotPortalWayland::Screenshot(const QDBusObjectPath &handle,
                                         const QString &app_id,
                                         const QString &parent_window,
                                         const QVariantMap &options,
                                         const QDBusMessage &message,
                                         uint &replyResponse,
                                         QVariantMap &replyResults)
{
    Q_UNUSED(app_id)
    Q_UNUSED(parent_window)
    Q_UNUSED(replyResponse)
    Q_UNUSED(replyResults)

    message.setDelayedReply(true);
    const bool interactive = options.value(QStringLiteral("interactive"), false).toBool();
    auto operation = new ScreenshotOperation(context(), handle, message, interactive, this);
    QTimer::singleShot(0, operation, [operation] {
        operation->start();
    });
}
