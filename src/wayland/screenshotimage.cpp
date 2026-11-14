// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screenshotimage.h"

#include <QPainter>
#include <QTransform>

#include <algorithm>
#include <cmath>

namespace ScreenshotImage
{
namespace
{

class AxisMapper
{
public:
    AxisMapper(const QList<Output> &outputs, bool horizontal)
        : m_horizontal(horizontal)
    {
        for (const auto &output : outputs) {
            const int start = axisStart(output);
            const int end = axisEnd(output);
            m_boundaries.append(start);
            m_boundaries.append(end);
        }

        std::sort(m_boundaries.begin(), m_boundaries.end());
        m_boundaries.erase(std::unique(m_boundaries.begin(), m_boundaries.end()),
                           m_boundaries.end());
        if (m_boundaries.isEmpty()) {
            return;
        }

        m_physicalBoundaries.reserve(m_boundaries.size());
        m_physicalBoundaries.append(0.0);
        qreal physical = 0.0;
        for (qsizetype i = 0; i + 1 < m_boundaries.size(); ++i) {
            const int start = m_boundaries.at(i);
            const int end = m_boundaries.at(i + 1);
            qreal scale = 1.0;
            bool covered = false;

            for (const auto &output : outputs) {
                if (axisStart(output) <= start && axisEnd(output) >= end) {
                    const int logicalSize = axisSize(output.logicalGeometry);
                    const int pixelSize = axisSize(output.image.size());
                    if (logicalSize > 0 && pixelSize > 0) {
                        const qreal outputScale = static_cast<qreal>(pixelSize) / logicalSize;
                        scale = covered ? std::max(scale, outputScale) : outputScale;
                        covered = true;
                    }
                }
            }

            physical += static_cast<qreal>(end - start) * scale;
            m_physicalBoundaries.append(physical);
        }
    }

    int map(int logical) const
    {
        if (m_boundaries.isEmpty()) {
            return 0;
        }

        const auto it = std::lower_bound(m_boundaries.cbegin(), m_boundaries.cend(), logical);
        if (it == m_boundaries.cend()) {
            return qRound(m_physicalBoundaries.constLast());
        }

        const qsizetype index = std::distance(m_boundaries.cbegin(), it);
        if (*it == logical || index == 0) {
            return qRound(m_physicalBoundaries.at(index));
        }

        const int segmentStart = m_boundaries.at(index - 1);
        const int segmentEnd = m_boundaries.at(index);
        const qreal physicalStart = m_physicalBoundaries.at(index - 1);
        const qreal physicalEnd = m_physicalBoundaries.at(index);
        const qreal ratio = static_cast<qreal>(logical - segmentStart)
                / static_cast<qreal>(segmentEnd - segmentStart);
        return qRound(physicalStart + ratio * (physicalEnd - physicalStart));
    }

private:
    int axisStart(const Output &output) const
    {
        return m_horizontal ? output.logicalGeometry.x() : output.logicalGeometry.y();
    }

    int axisEnd(const Output &output) const
    {
        return axisStart(output) + axisSize(output.logicalGeometry);
    }

    int axisSize(const QRect &rect) const
    {
        return m_horizontal ? rect.width() : rect.height();
    }

    int axisSize(const QSize &size) const
    {
        return m_horizontal ? size.width() : size.height();
    }

    bool m_horizontal;
    QList<int> m_boundaries;
    QList<qreal> m_physicalBoundaries;
};

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

    for (const auto &output : outputs) {
        if (output.logicalGeometry.isEmpty() || output.image.isNull()) {
            setError(errorMessage,
                     QStringLiteral("Invalid output geometry or image for %1").arg(output.name));
            return {};
        }
    }

    const AxisMapper xMapper(outputs, true);
    const AxisMapper yMapper(outputs, false);
    QList<QRect> targetRects;
    targetRects.reserve(outputs.size());
    QRect physicalRegion;
    for (const auto &output : outputs) {
        const QRect targetRect(QPoint(xMapper.map(output.logicalGeometry.x()),
                                      yMapper.map(output.logicalGeometry.y())),
                               output.image.size());
        targetRects.append(targetRect);
        physicalRegion = physicalRegion.united(targetRect);
    }

    if (physicalRegion.isEmpty()) {
        setError(errorMessage, QStringLiteral("Composed output region is empty"));
        return {};
    }

    QImage result(physicalRegion.size(), QImage::Format_ARGB32_Premultiplied);
    if (result.isNull()) {
        setError(errorMessage, QStringLiteral("Failed to allocate screenshot image"));
        return {};
    }
    result.fill(Qt::transparent);

    QPainter painter(&result);
    for (qsizetype i = 0; i < outputs.size(); ++i) {
        painter.drawImage(targetRects.at(i).topLeft() - physicalRegion.topLeft(),
                          outputs.at(i).image);
    }
    painter.end();
    return result;
}

}
