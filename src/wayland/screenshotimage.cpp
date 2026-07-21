// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screenshotimage.h"

#include <QPainter>
#include <QTransform>

#include <algorithm>
#include <limits>

namespace ScreenshotImage
{
namespace
{

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

}

QImage normalizeOutput(const QImage &image, int transform, bool yInverted)
{
    if (image.isNull() || transform < 0 || transform > 7) {
        return {};
    }

    QImage result = yInverted ? image.mirrored(false, true) : image;
    QTransform rotation;
    switch (transform & 3) {
    case 1:
        rotation.rotate(90);
        break;
    case 2:
        rotation.rotate(180);
        break;
    case 3:
        rotation.rotate(270);
        break;
    default:
        break;
    }

    if (!rotation.isIdentity()) {
        result = result.transformed(rotation, Qt::FastTransformation);
    }
    if (transform >= 4) {
        result = result.mirrored(true, false);
    }
    return result;
}

QImage composeOutputs(const QList<Output> &outputs, QString *errorMessage)
{
    if (outputs.isEmpty()) {
        setError(errorMessage, QStringLiteral("No outputs to compose"));
        return {};
    }

    QRect logicalRegion;
    qreal scale = 1.0;
    for (const auto &output : outputs) {
        if (output.logicalGeometry.isEmpty() || output.image.isNull()) {
            setError(errorMessage,
                     QStringLiteral("Invalid output geometry or image for %1").arg(output.name));
            return {};
        }

        logicalRegion = logicalRegion.united(output.logicalGeometry);

        // KWin renders the whole workspace with one uniform scale: with
        // native-resolution enabled it uses the largest output scale and
        // upscales the other outputs into that workspace image. Use the same
        // model so mixed-DPI multi-screen screenshots keep the same geometry
        // and pixel dimensions as the KDE portal.
        const qreal horizontalScale =
                static_cast<qreal>(output.image.width()) / output.logicalGeometry.width();
        const qreal verticalScale =
                static_cast<qreal>(output.image.height()) / output.logicalGeometry.height();
        scale = std::max({scale, horizontalScale, verticalScale});
    }

    if (logicalRegion.isEmpty()) {
        setError(errorMessage, QStringLiteral("Composed output region is empty"));
        return {};
    }

    // Round logical edges in one coordinate system instead of rounding each
    // output size independently. Adjacent outputs then share exactly the same
    // physical boundary on fractional-scale layouts.
    const qreal physicalWidth = static_cast<qreal>(logicalRegion.width()) * scale;
    const qreal physicalHeight = static_cast<qreal>(logicalRegion.height()) * scale;
    const qreal maxImageEdge = std::numeric_limits<int>::max();
    if (physicalWidth > maxImageEdge || physicalHeight > maxImageEdge) {
        setError(errorMessage, QStringLiteral("Composed screenshot dimensions are too large"));
        return {};
    }

    const auto mapToPhysical = [scale](int logical) {
        return qRound(static_cast<qreal>(logical) * scale);
    };
    const QPoint logicalOrigin = logicalRegion.topLeft();
    const QSize physicalSize(qRound(physicalWidth), qRound(physicalHeight));

    QImage result(physicalSize, QImage::Format_ARGB32_Premultiplied);
    if (result.isNull()) {
        setError(errorMessage, QStringLiteral("Failed to allocate screenshot image"));
        return {};
    }
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    for (const auto &output : outputs) {
        const int left = mapToPhysical(output.logicalGeometry.x() - logicalOrigin.x());
        const int top = mapToPhysical(output.logicalGeometry.y() - logicalOrigin.y());
        const int right = mapToPhysical(
                output.logicalGeometry.x() + output.logicalGeometry.width() - logicalOrigin.x());
        const int bottom = mapToPhysical(
                output.logicalGeometry.y() + output.logicalGeometry.height() - logicalOrigin.y());
        painter.drawImage(QRect(left, top, right - left, bottom - top), output.image);
    }
    painter.end();

    result.setDevicePixelRatio(scale);
    return result;
}

}
