// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screenshotportal.h"
#include "protocols/common.h"
#include "protocols/treelandcapture.h"
#include "loggings.h"
#include "dbushelpers.h"

#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QDir>
#include <QStandardPaths>

#include <private/qwaylandscreen_p.h>

Q_LOGGING_CATEGORY(portalWayland, "dde.portal.wayland");
struct ScreenCaptureInfo {
    QtWaylandClient::QWaylandScreen *screen {nullptr};
    QPointer<ScreenCopyFrame> capturedFrame {nullptr};
    QtWaylandClient::QWaylandShmBuffer *shmBuffer {nullptr};
    QtWayland::zwlr_screencopy_frame_v1::flags flags;
};

static void destroyScreenCaptureInfo(std::list<std::shared_ptr<ScreenCaptureInfo>> captureList)
{
    for (const auto &info : std::as_const(captureList)) {
        if (info->shmBuffer) {
            delete info->shmBuffer;
        }

        if (info->capturedFrame) {
            delete info->capturedFrame;
        }
    }
}

static bool captureInfoHasImage(const std::shared_ptr<ScreenCaptureInfo> &info)
{
    return info && info->screen && info->shmBuffer && info->shmBuffer->image() &&
           !info->shmBuffer->image()->isNull();
}

static int captureInfoAxisStart(const std::shared_ptr<ScreenCaptureInfo> &info, bool horizontal)
{
    const QRect geometry = info->screen->geometry();
    return horizontal ? geometry.x() : geometry.y();
}

static int captureInfoAxisEnd(const std::shared_ptr<ScreenCaptureInfo> &info, bool horizontal)
{
    const QRect geometry = info->screen->geometry();
    return horizontal ? geometry.x() + geometry.width() :
                        geometry.y() + geometry.height();
}

static qreal captureInfoAxisScale(const std::shared_ptr<ScreenCaptureInfo> &info, bool horizontal)
{
    const QRect geometry = info->screen->geometry();
    const QImage *image = info->shmBuffer->image();
    const int logicalSize = horizontal ? geometry.width() : geometry.height();
    const int pixelSize = horizontal ? image->width() : image->height();

    if (logicalSize <= 0 || pixelSize <= 0)
        return 1.0;

    return static_cast<qreal>(pixelSize) / logicalSize;
}

static int logicalAxisToPhysical(const std::list<std::shared_ptr<ScreenCaptureInfo>> &captureList,
                                 int logical,
                                 bool horizontal)
{
    bool haveOrigin = false;
    int origin = 0;
    int pos;
    int physical = 0;

    for (const auto &info : std::as_const(captureList)) {
        if (!captureInfoHasImage(info))
            continue;

        const int start = captureInfoAxisStart(info, horizontal);
        if (!haveOrigin || start < origin) {
            origin = start;
            haveOrigin = true;
        }
    }
    if (!haveOrigin || logical <= origin)
        return 0;

    pos = origin;
    while (pos < logical) {
        int next = logical;
        qreal scale = 1.0;
        bool covered = false;

        for (const auto &info : std::as_const(captureList)) {
            if (!captureInfoHasImage(info))
                continue;

            const int start = captureInfoAxisStart(info, horizontal);
            const int end = captureInfoAxisEnd(info, horizontal);

            if (start <= pos && end > pos) {
                const qreal outputScale = captureInfoAxisScale(info, horizontal);
                next = qMin(next, end);
                scale = covered ? qMax(scale, outputScale) : outputScale;
                covered = true;
            } else if (start > pos) {
                next = qMin(next, start);
            }
        }

        if (next <= pos)
            break;

        if (!covered) {
            qCWarning(portalWayland) << "Gap in output layout at logical position" << pos
                                     << "to" << next
                                     << "on" << (horizontal ? "X" : "Y")
                                     << "axis, defaulting to scale 1.0";
        }

        physical += qRound((next - pos) * scale);
        pos = next;
    }

    return physical;
}

static QRect captureInfoPhysicalRect(const std::list<std::shared_ptr<ScreenCaptureInfo>> &captureList,
                                     const std::shared_ptr<ScreenCaptureInfo> &info)
{
    const QRect geometry = info->screen->geometry();
    const QImage *image = info->shmBuffer->image();

    return QRect(logicalAxisToPhysical(captureList, geometry.x(), true),
                 logicalAxisToPhysical(captureList, geometry.y(), false),
                 image->width(),
                 image->height());
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

QString ScreenshotPortalWayland::fullScreenShot()
{
    std::list<std::shared_ptr<ScreenCaptureInfo>> captureList;
    int pendingCapture = 0;
    auto screenCopyManager = context()->screenCopyManager();
    QEventLoop eventLoop;
    QImage::Format formatLast;
    // Capture each output
    for (auto screen : waylandDisplay()->screens()) {
        auto info = std::make_shared<ScreenCaptureInfo>();
        auto output = screen->output();
        info->capturedFrame = screenCopyManager->captureOutput(false, output);
        info->screen = screen;
        ++pendingCapture;
        captureList.push_back(info);
        connect(info->capturedFrame, &ScreenCopyFrame::buffer, this,
                [info](uint32_t format, uint32_t width, uint32_t height, uint32_t stride){
                    // Create a new wl_buffer for reception
                    // For some reason, Qt regards stride == width * 4, and it creates buffer likewise, we must check this
                    if (stride != width * 4) {
                        qCDebug(SCREESHOT)
                        << "Receive a buffer format which is not compatible with QWaylandShmBuffer."
                        << "format:" << format << "width:" << width << "height:" << height
                        << "stride:" << stride;
                        return;
                    }
                    if (info->shmBuffer)
                        return; // We only need one supported format

                    info->shmBuffer = new QtWaylandClient::QWaylandShmBuffer(
                            waylandDisplay(),
                            QSize(width, height),
                            QtWaylandClient::QWaylandShm::formatFrom(static_cast<::wl_shm_format>(format)));
                    info->capturedFrame->copy(info->shmBuffer->buffer());
                });
        connect(info->capturedFrame, &ScreenCopyFrame::frameFlags, this,
                [info](uint32_t flags){
                    info->flags = static_cast<QtWayland::zwlr_screencopy_frame_v1::flags>(flags);
                });
        connect(info->capturedFrame, &ScreenCopyFrame::ready, this,
                [info, &pendingCapture, &eventLoop, &formatLast](uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) {
                    Q_UNUSED(tv_sec_hi);
                    Q_UNUSED(tv_sec_lo);
                    Q_UNUSED(tv_nsec);
                    formatLast = info->shmBuffer->image()->format();
                    if (--pendingCapture == 0) {
                        eventLoop.quit();
                    }
                });
        connect(info->capturedFrame, &ScreenCopyFrame::failed, this, [&pendingCapture, &eventLoop]{
            if (--pendingCapture == 0) {
                eventLoop.quit();
            }
        });
    }
    eventLoop.exec();

    QRect physicalOutputRegion;
    for (const auto &info : std::as_const(captureList)) {
        if (!captureInfoHasImage(info))
            continue;

        physicalOutputRegion = physicalOutputRegion.united(captureInfoPhysicalRect(captureList, info));
    }
    if (physicalOutputRegion.isEmpty()) {
        qCWarning(portalWayland) << "No valid output images are available for screenshot";
        destroyScreenCaptureInfo(captureList);
        return "";
    }

    // Cat outputs according to their physical pixel buffers. QScreen geometry
    // is logical under fractional scaling, while screencopy images are pixels.
    QImage image(physicalOutputRegion.size(), formatLast);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    for (const auto &info : std::as_const(captureList)) {
        if (captureInfoHasImage(info)) {
            const QImage *outputImage = info->shmBuffer->image();
            QRect targetRect = captureInfoPhysicalRect(captureList, info);
            targetRect.translate(-physicalOutputRegion.topLeft());
            p.drawImage(targetRect, *outputImage, QRect(QPoint(0, 0), outputImage->size()));
        } else {
            qCWarning(portalWayland) << "No image available for output geometry"
                                     << (info && info->screen ? info->screen->geometry() : QRect());
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

QString ScreenshotPortalWayland::captureInteractively()
{
    auto captureManager = context()->treelandCaptureManager();
    auto captureContext = captureManager->getContext();
    if (!captureContext) {
        return "";
    }
    captureContext->selectSource(QtWayland::treeland_capture_context_v1::source_type_output
                                         | QtWayland::treeland_capture_context_v1::source_type_window
                                         | QtWayland::treeland_capture_context_v1::source_type_region
                                 ,true
                                 , false
                                 ,nullptr);
    QEventLoop loop;
    connect(captureContext, &TreeLandCaptureContext::sourceReady, &loop, &QEventLoop::quit);
    loop.exec();
    auto frame = captureContext->frame();
    QImage result;
    connect(frame, &TreeLandCaptureFrame::ready, this, [this, &result, &loop](QImage image) {
        result = image;
        loop.quit();
    });
    connect(frame, &TreeLandCaptureFrame::failed, &loop, &QEventLoop::quit);
    loop.exec();
    if (result.isNull()) return "";
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
    if (options["modal"].toBool()) {
        // TODO if modal, we should block parent_window
    }
    QString filePath;
    if (options["interactive"].toBool()) {
        filePath = captureInteractively();
    } else {
        filePath = fullScreenShot();
    }
    if (filePath.isEmpty()) {
        return 1;
    }
    results.insert(QStringLiteral("uri"), QUrl::fromLocalFile(filePath).toString(QUrl::FullyEncoded));
    return 0;
}
