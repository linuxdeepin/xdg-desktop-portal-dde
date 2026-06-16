// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screenshotportal.h"
#include "protocols/common.h"
#include "protocols/treelandcapture.h"
#include "loggings.h"
#include "dbushelpers.h"
#include "request2.h"

#include <QApplication>
#include <QDateTime>
#include <QScreen>
#include <QPainter>
#include <QDir>
#include <QEventLoop>
#include <QPointer>
#include <QRegion>
#include <QStandardPaths>
#include <QTimer>

#include <private/qwaylandscreen_p.h>

Q_LOGGING_CATEGORY(portalWayland, "dde.portal.wayland");
struct ScreenCaptureInfo {
    QtWaylandClient::QWaylandScreen *screen {nullptr};
    QPointer<ScreenCopyFrame> capturedFrame {nullptr};
    QtWaylandClient::QWaylandShmBuffer *shmBuffer {nullptr};
    uint32_t shmFormat {0};
    uint32_t shmWidth {0};
    uint32_t shmHeight {0};
    uint32_t shmStride {0};
    QtWayland::zwlr_screencopy_frame_v1::flags flags;
    bool receivedShmBuffer {false};
    bool hasCompatibleShmBuffer {false};
    bool receivedDmabuf {false};
    bool bufferDone {false};
    bool copyRequested {false};
    bool completed {false};
};

static constexpr int FullscreenScreenshotTimeoutMs = 3000;

static void destroyScreenCaptureInfo(const std::list<std::shared_ptr<ScreenCaptureInfo>> &captureList)
{
    for (const auto &info : std::as_const(captureList)) {
        if (info->shmBuffer) {
            delete info->shmBuffer;
        }

        if (info->capturedFrame) {
            QObject::disconnect(info->capturedFrame, nullptr, nullptr, nullptr);
            if (info->capturedFrame->isInitialized())
                info->capturedFrame->destroy();
            delete info->capturedFrame;
        }
    }
}

ScreenshotPortalWayland::ScreenshotPortalWayland(PortalWaylandContext *context)
    : AbstractWaylandPortal(context)
{

}

uint ScreenshotPortalWayland::PickColor(const QDBusObjectPath &handle,
                                 const QString &app_id,
                                 const QString &parent_window, // might just ignore this argument now
                                 const QVariantMap &options,
                                 QVariantMap &results)
{
    // TODO Implement PickColor
    qCDebug(SCREESHOT) << "PickColor called with parameters:";
    qCDebug(SCREESHOT) << "    handle: " << handle.path();
    qCDebug(SCREESHOT) << "    app_id: " << app_id;
    qCDebug(SCREESHOT) << "    parent_window: " << parent_window;
    qCDebug(SCREESHOT) << "    options: " << options;

    return PortalResponse::Cancelled;
}

QString ScreenshotPortalWayland::fullScreenShot(Request2 *request, bool *cancelled)
{
    std::list<std::shared_ptr<ScreenCaptureInfo>> captureList;
    int pendingCapture = 0;
    auto screenCopyManager = context()->screenCopyManager();
    QEventLoop eventLoop;
    QTimer timeoutTimer;
    QRegion outputRegion;
    QImage::Format formatLast = QImage::Format_ARGB32_Premultiplied;
    bool captureFailed = false;
    bool timedOut = false;

    if (cancelled)
        *cancelled = false;

    auto completeCapture = [&pendingCapture, &eventLoop, &captureFailed]
            (const std::shared_ptr<ScreenCaptureInfo> &info, bool failed) {
        if (info->completed)
            return;

        info->completed = true;
        if (failed)
            captureFailed = true;

        if (--pendingCapture == 0) {
            eventLoop.quit();
        }
    };

    timeoutTimer.setSingleShot(true);
    connect(&timeoutTimer, &QTimer::timeout, &eventLoop, [&] {
        timedOut = true;
        eventLoop.quit();
    });
    if (request) {
        connect(request, &Request2::closeRequested, &eventLoop, [&] {
            if (cancelled)
                *cancelled = true;
            eventLoop.quit();
        });
    }

    // Capture each output
    for (auto screen : waylandDisplay()->screens()) {
        auto info = std::make_shared<ScreenCaptureInfo>();
        outputRegion += screen->geometry();
        auto output = screen->output();
        info->capturedFrame = screenCopyManager->captureOutput(false, output);
        if (!info->capturedFrame) {
            qCWarning(portalWayland) << "Failed to create screencopy frame for output" << screen->geometry();
            captureFailed = true;
            continue;
        }
        info->screen = screen;
        ++pendingCapture;
        captureList.push_back(info);
        connect(info->capturedFrame, &ScreenCopyFrame::buffer, this,
                [info](uint32_t format, uint32_t width, uint32_t height, uint32_t stride){
                    info->receivedShmBuffer = true;

                    /*
                     * QWaylandShmBuffer creates tightly packed images, so only
                     * accept the wl_shm format that it can represent. Wait for
                     * buffer_done before sending copy(), as required by the
                     * screencopy protocol.
                     */
                    if (stride != width * 4) {
                        qCDebug(SCREESHOT)
                        << "Receive a buffer format which is not compatible with QWaylandShmBuffer."
                        << "format:" << format << "width:" << width << "height:" << height
                        << "stride:" << stride;
                        return;
                    }

                    if (info->hasCompatibleShmBuffer)
                        return; // We only need one supported format

                    info->hasCompatibleShmBuffer = true;
                    info->shmFormat = format;
                    info->shmWidth = width;
                    info->shmHeight = height;
                    info->shmStride = stride;
                });
        connect(info->capturedFrame, &ScreenCopyFrame::linuxDmabuf, this,
                [info](uint32_t format, uint32_t width, uint32_t height) {
                    Q_UNUSED(format);
                    Q_UNUSED(width);
                    Q_UNUSED(height);
                    info->receivedDmabuf = true;
                });
        connect(info->capturedFrame, &ScreenCopyFrame::bufferDone, this,
                [this, info, completeCapture] {
                    if (info->completed || info->copyRequested)
                        return;

                    info->bufferDone = true;

                    if (!info->hasCompatibleShmBuffer) {
                        qCWarning(portalWayland)
                                << "No compatible wl_shm buffer for screenshot output"
                                << (info->screen ? info->screen->geometry() : QRect())
                                << "receivedShmBuffer=" << info->receivedShmBuffer
                                << "receivedDmabuf=" << info->receivedDmabuf;
                        completeCapture(info, true);
                        return;
                    }

                    info->shmBuffer = new QtWaylandClient::QWaylandShmBuffer(
                            waylandDisplay(),
                            QSize(info->shmWidth, info->shmHeight),
                            QtWaylandClient::QWaylandShm::formatFrom(static_cast<::wl_shm_format>(info->shmFormat)));
                    info->copyRequested = true;
                    info->capturedFrame->copy(info->shmBuffer->buffer());
                });
        connect(info->capturedFrame, &ScreenCopyFrame::frameFlags, this,
                [info](uint32_t flags){
                    info->flags = static_cast<QtWayland::zwlr_screencopy_frame_v1::flags>(flags);
                });
        connect(info->capturedFrame, &ScreenCopyFrame::ready, this,
                [info, &formatLast, completeCapture](uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) {
                    Q_UNUSED(tv_sec_hi);
                    Q_UNUSED(tv_sec_lo);
                    Q_UNUSED(tv_nsec);
                    if (info->completed)
                        return;
                    if (!info->shmBuffer || !info->shmBuffer->image()) {
                        completeCapture(info, true);
                    } else {
                        formatLast = info->shmBuffer->image()->format();
                        completeCapture(info, false);
                    }
                });
        connect(info->capturedFrame, &ScreenCopyFrame::failed, this, [info, completeCapture]{
            if (info->completed)
                return;
            qCWarning(portalWayland)
                    << "Screencopy failed for screenshot output"
                    << (info->screen ? info->screen->geometry() : QRect());
            completeCapture(info, true);
        });
    }
    if (pendingCapture == 0) {
        qCWarning(portalWayland) << "No outputs are available for screenshot";
        destroyScreenCaptureInfo(captureList);
        return "";
    }
    timeoutTimer.start(FullscreenScreenshotTimeoutMs);
    eventLoop.exec();
    timeoutTimer.stop();
    if ((cancelled && *cancelled) || timedOut || captureFailed) {
        if (timedOut) {
            qCWarning(portalWayland) << "Fullscreen screenshot timed out";
            for (const auto &info : std::as_const(captureList)) {
                if (info->completed)
                    continue;

                qCWarning(portalWayland)
                        << "Pending screenshot output"
                        << (info->screen ? info->screen->geometry() : QRect())
                        << "receivedShmBuffer=" << info->receivedShmBuffer
                        << "hasCompatibleShmBuffer=" << info->hasCompatibleShmBuffer
                        << "receivedDmabuf=" << info->receivedDmabuf
                        << "bufferDone=" << info->bufferDone
                        << "copyRequested=" << info->copyRequested;
            }
        }
        destroyScreenCaptureInfo(captureList);
        return "";
    }

    // Cat them according to layout
    QImage image(outputRegion.boundingRect().size(), formatLast);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    for (const auto &info : std::as_const(captureList)) {
        if (info->shmBuffer && info->shmBuffer->image()) {
            QRect targetRect = info->screen->geometry();
            // Convert to screen image local coordinates
            auto sourceRect = targetRect;
            sourceRect.moveTo(sourceRect.topLeft() - info->screen->geometry().topLeft());
            p.drawImage(targetRect, *info->shmBuffer->image(), sourceRect);
        } else {
            qCWarning(portalWayland) << "image is null!!!";
        }
    }
    static const char *SaveFormat = "PNG";
    auto saveBasePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QDir saveBaseDir(saveBasePath);
    if (!saveBaseDir.exists()) return "";
    QString picName = "portal screenshot - " + QDateTime::currentDateTime().toString() + ".png";
    if (image.save(saveBaseDir.absoluteFilePath(picName), SaveFormat)) {
        destroyScreenCaptureInfo(captureList);
        return saveBaseDir.absoluteFilePath(picName);
    } else {
        destroyScreenCaptureInfo(captureList);
        return "";
    }
}

QString ScreenshotPortalWayland::captureInteractively(Request2 *request, bool *cancelled)
{
    auto captureManager = context()->treelandCaptureManager();
    auto captureContext = captureManager->getContext();
    bool sourceFailed = false;
    if (!captureContext) {
        return "";
    }
    if (cancelled)
        *cancelled = false;
    captureContext->selectSource(QtWayland::treeland_capture_context_v1::source_type_output
                                         | QtWayland::treeland_capture_context_v1::source_type_window
                                         | QtWayland::treeland_capture_context_v1::source_type_region
                                 ,true
                                 , false
                                 ,nullptr);
    QEventLoop loop;
    connect(captureContext, &TreeLandCaptureContext::sourceReady, &loop, &QEventLoop::quit);
    connect(captureContext, &TreeLandCaptureContext::sourceFailed, &loop, [&] {
        sourceFailed = true;
        loop.quit();
    });
    if (request) {
        connect(request, &Request2::closeRequested, &loop, [&] {
            if (cancelled)
                *cancelled = true;
            loop.quit();
        });
    }
    loop.exec();
    if ((cancelled && *cancelled) || sourceFailed) {
        captureManager->releaseCaptureContext(captureContext);
        return "";
    }
    auto frame = captureContext->frame();
    QImage result;
    connect(frame, &TreeLandCaptureFrame::ready, this, [this, &result, &loop](QImage image) {
        result = image;
        loop.quit();
    });
    connect(frame, &TreeLandCaptureFrame::failed, &loop, &QEventLoop::quit);
    if (request) {
        connect(request, &Request2::closeRequested, &loop, [&] {
            if (cancelled)
                *cancelled = true;
            loop.quit();
        });
    }
    loop.exec();
    captureManager->releaseCaptureContext(captureContext);
    if ((cancelled && *cancelled) || result.isNull()) return "";
    auto saveBasePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QDir saveBaseDir(saveBasePath);
    if (!saveBaseDir.exists()) return "";
    QString picName = "portal screenshot - " + QDateTime::currentDateTime().toString() + ".png";
    if (result.save(saveBaseDir.absoluteFilePath(picName), "PNG")) {
        return saveBaseDir.absoluteFilePath(picName);
    } else {
        return "";
    }
}

uint ScreenshotPortalWayland::Screenshot(const QDBusObjectPath &handle,
                                  const QString &app_id,
                                  const QString &parent_window,
                                  const QVariantMap &options,
                                  QVariantMap &results)
{
    uint response = PortalResponse::Success;
    QPointer<Request2> request;
    bool cancelled = false;

    if (m_screenshotInProgress) {
        qCWarning(portalWayland) << "Rejecting concurrent screenshot request";
        return PortalResponse::OtherError;
    }
    m_screenshotInProgress = true;

    request = new Request2(handle, this, QStringLiteral("Screenshot"));
    if (options["modal"].toBool()) {
        // TODO if modal, we should block parent_window
    }
    QString filePath;
    if (options["interactive"].toBool()) {
        filePath = captureInteractively(request, &cancelled);
    } else {
        filePath = fullScreenShot(request, &cancelled);
    }
    if (filePath.isEmpty()) {
        response = cancelled ? PortalResponse::Cancelled :
                               PortalResponse::OtherError;
    } else {
        results.insert(QStringLiteral("uri"), QUrl::fromLocalFile(filePath).toString(QUrl::FullyEncoded));
    }
    if (request)
        delete request.data();
    m_screenshotInProgress = false;
    return response;
}
