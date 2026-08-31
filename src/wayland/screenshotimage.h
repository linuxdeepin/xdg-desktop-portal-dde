// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QImage>
#include <QList>
#include <QRect>
#include <QString>

namespace ScreenshotImage
{

struct Output
{
    QRect logicalGeometry;
    QImage image;
    QString name;
};

QImage normalizeOutput(const QImage &image, int transform, bool yInverted);
QImage composeOutputs(const QList<Output> &outputs, QString *errorMessage = nullptr);

}
