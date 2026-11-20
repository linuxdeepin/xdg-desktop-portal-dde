// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wayland/screenshotimage.h"

#include <QColor>
#include <QImage>

#include <iostream>

namespace
{

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

QPoint transformedPoint(int transform, int x, int y, int width, int height)
{
    switch (transform) {
    case 0:
        return {x, y};
    case 1:
        return {height - 1 - y, x};
    case 2:
        return {width - 1 - x, height - 1 - y};
    case 3:
        return {y, width - 1 - x};
    case 4:
        return {width - 1 - x, y};
    case 5:
        return {y, x};
    case 6:
        return {x, height - 1 - y};
    case 7:
        return {height - 1 - y, width - 1 - x};
    default:
        return {};
    }
}

bool testTransforms()
{
    QImage source(2, 3, QImage::Format_ARGB32);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            source.setPixelColor(x, y, QColor(20 + x * 80, 30 + y * 60, 10 + x + y));
        }
    }

    for (int transform = 0; transform < 8; ++transform) {
        const QImage result = ScreenshotImage::normalizeOutput(source, transform, false);
        const bool swapsAxes = transform % 2 == 1;
        if (!expect(result.size() == (swapsAxes ? source.size().transposed() : source.size()),
                    "Unexpected transformed image size")) {
            return false;
        }
        for (int y = 0; y < source.height(); ++y) {
            for (int x = 0; x < source.width(); ++x) {
                const QPoint destination = transformedPoint(
                        transform, x, y, source.width(), source.height());
                if (!expect(result.pixelColor(destination) == source.pixelColor(x, y),
                            "wl_output transform moved a pixel to the wrong location")) {
                    return false;
                }
            }
        }
    }

    const QImage inverted = ScreenshotImage::normalizeOutput(source, 0, true);
    if (!expect(inverted.pixelColor(0, 0) == source.pixelColor(0, source.height() - 1),
                "Y-inverted screencopy image was not flipped")) {
        return false;
    }

    for (int transform = 0; transform < 8; ++transform) {
        const QImage result = ScreenshotImage::normalizeOutput(source, transform, true);
        for (int y = 0; y < source.height(); ++y) {
            for (int x = 0; x < source.width(); ++x) {
                const QPoint destination = transformedPoint(
                        transform, x, source.height() - 1 - y, source.width(), source.height());
                if (!expect(result.pixelColor(destination) == source.pixelColor(x, y),
                            "Y inversion and wl_output transform were applied in the wrong order")) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool testFractionalHorizontalLayout()
{
    QImage left(4, 4, QImage::Format_ARGB32);
    left.fill(Qt::red);
    QImage right(6, 6, QImage::Format_ARGB32);
    right.fill(Qt::blue);

    const QList<ScreenshotImage::Output> outputs{
        {QRect(-100, -20, 4, 4), left, QStringLiteral("left")},
        {QRect(-96, -20, 4, 4), right, QStringLiteral("right")},
    };
    const QImage result = ScreenshotImage::composeOutputs(outputs);
    return expect(result.size() == QSize(12, 6), "Mixed-scale horizontal canvas has wrong size")
            && expect(qFuzzyCompare(result.devicePixelRatio(), 1.5),
                      "Mixed-scale canvas has the wrong device pixel ratio")
            && expect(result.pixelColor(0, 0) == QColor(Qt::red), "Left output is missing")
            && expect(result.pixelColor(5, 5) == QColor(Qt::red), "Left output was cropped")
            && expect(result.pixelColor(6, 0) == QColor(Qt::blue), "Right output starts at wrong X")
            && expect(result.pixelColor(11, 5) == QColor(Qt::blue), "Right output was cropped");
}

bool testGapAndVerticalLayout()
{
    QImage top(2, 2, QImage::Format_ARGB32);
    top.fill(Qt::green);
    QImage bottom(4, 4, QImage::Format_ARGB32);
    bottom.fill(Qt::yellow);

    const QList<ScreenshotImage::Output> outputs{
        {QRect(0, -4, 2, 2), top, QStringLiteral("top")},
        {QRect(0, 0, 2, 2), bottom, QStringLiteral("bottom")},
    };
    const QImage result = ScreenshotImage::composeOutputs(outputs);
    return expect(result.size() == QSize(4, 12), "Vertical layout with a gap has wrong size")
            && expect(qFuzzyCompare(result.devicePixelRatio(), 2.0),
                      "Vertical mixed-scale canvas has the wrong device pixel ratio")
            && expect(result.pixelColor(0, 0) == QColor(Qt::green), "Top output is missing")
            && expect(result.pixelColor(0, 4).alpha() == 0, "Logical output gap was not preserved")
            && expect(result.pixelColor(0, 8) == QColor(Qt::yellow),
                      "Bottom output starts at wrong Y")
            && expect(result.pixelColor(3, 11) == QColor(Qt::yellow), "Bottom output was cropped");
}

bool testThreeFractionalScales()
{
    QImage scale125(5, 5, QImage::Format_ARGB32);
    scale125.fill(Qt::cyan);
    QImage scale150(6, 6, QImage::Format_ARGB32);
    scale150.fill(Qt::magenta);
    QImage scale200(8, 8, QImage::Format_ARGB32);
    scale200.fill(Qt::white);

    const QList<ScreenshotImage::Output> outputs{
        {QRect(0, 0, 4, 4), scale125, QStringLiteral("125-percent")},
        {QRect(4, 0, 4, 4), scale150, QStringLiteral("150-percent")},
        {QRect(8, 0, 4, 4), scale200, QStringLiteral("200-percent")},
    };
    const QImage result = ScreenshotImage::composeOutputs(outputs);
    return expect(result.size() == QSize(24, 8), "Three-scale canvas has wrong size")
            && expect(qFuzzyCompare(result.devicePixelRatio(), 2.0),
                      "Three-scale canvas has the wrong device pixel ratio")
            && expect(result.pixelColor(4, 0) == QColor(Qt::cyan), "125% output was cropped")
            && expect(result.pixelColor(7, 7) == QColor(Qt::cyan), "125% output was cropped")
            && expect(result.pixelColor(8, 0) == QColor(Qt::magenta),
                      "150% output starts at wrong X")
            && expect(result.pixelColor(11, 0) == QColor(Qt::magenta), "150% output was cropped")
            && expect(result.pixelColor(16, 0) == QColor(Qt::white),
                      "200% output starts at wrong X")
            && expect(result.pixelColor(23, 7) == QColor(Qt::white), "200% output was cropped");
}

}

int main()
{
    if (!testTransforms() || !testFractionalHorizontalLayout() || !testGapAndVerticalLayout()
        || !testThreeFractionalScales()) {
        return 1;
    }

    QString error;
    if (!expect(ScreenshotImage::composeOutputs({}, &error).isNull() && !error.isEmpty(),
                "Invalid composition did not report an error")) {
        return 1;
    }
    return 0;
}
