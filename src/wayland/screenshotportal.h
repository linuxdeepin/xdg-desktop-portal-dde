// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "abstractwaylandportal.h"

#include <QDBusObjectPath>
#include <QObject>

class Request2;

class ScreenshotPortalWayland : public AbstractWaylandPortal
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Screenshot")

public:
    ScreenshotPortalWayland(PortalWaylandContext *context);

    QString fullScreenShot(Request2 *request, bool *cancelled);
    QString captureInteractively(Request2 *request, bool *cancelled);

public Q_SLOTS:
    uint PickColor(const QDBusObjectPath &handle,
                   const QString &app_id,
                   const QString &parent_window,
                   const QVariantMap &options,
                   QVariantMap &results);
    uint Screenshot(const QDBusObjectPath &handle,
                    const QString &app_id,
                    const QString &parent_window,
                    const QVariantMap &options,
                    QVariantMap &results);

private:
    bool m_screenshotInProgress = false;
};
