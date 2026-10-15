// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "abstractwaylandportal.h"

#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QObject>

class ScreenshotPortalWayland : public AbstractWaylandPortal
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Screenshot")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    explicit ScreenshotPortalWayland(PortalWaylandContext *context);

    uint version() const { return 2; }

public Q_SLOTS:
    uint PickColor(const QDBusObjectPath &handle,
                   const QString &app_id,
                   const QString &parent_window,
                   const QVariantMap &options,
                   QVariantMap &results);
    void Screenshot(const QDBusObjectPath &handle,
                    const QString &app_id,
                    const QString &parent_window,
                    const QVariantMap &options,
                    const QDBusMessage &message,
                    uint &replyResponse,
                    QVariantMap &replyResults);
};
