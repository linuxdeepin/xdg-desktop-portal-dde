// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "abstractwaylandportal.h"
#include "portalcommon.h"
#include "treelandintegration.h"
#include "dbushelpers.h"

#include <QObject>
#include <QDBusObjectPath>

Q_GLOBAL_STATIC(TreelandIntergration, globalIntergration)

class ScreenCastSession;

class ScreencastPortalWayland : public AbstractWaylandPortal
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.ScreenCast")

    Q_PROPERTY(uint version READ version CONSTANT)
    Q_PROPERTY(uint AvailableSourceTypes READ AvailableSourceTypes CONSTANT)
    Q_PROPERTY(uint AvailableCursorModes READ AvailableCursorModes CONSTANT)
public:
    ScreencastPortalWayland(PortalWaylandContext *context);
    ~ScreencastPortalWayland() override;

    uint version() const { return 1;}
    uint AvailableSourceTypes() const;
    uint AvailableCursorModes() const;

public Q_SLOTS:
    uint CreateSession(const QDBusObjectPath &handle,
                       const QDBusObjectPath &session_handle,
                       const QString &app_id,
                       const QVariantMap &options,
                       QVariantMap &results);

    uint SelectSources(const QDBusObjectPath &handle,
                       const QDBusObjectPath &session_handle,
                       const QString &app_id,
                       const QVariantMap &options,
                       QVariantMap &results);

    void Start(const QDBusObjectPath &handle,
               const QDBusObjectPath &session_handle,
               const QString &app_id,
               const QString &parent_window,
               const QVariantMap &options,
               const QDBusMessage &message,
               uint &replyResponse,
               QVariantMap &replyResults);
private:
    static std::pair<PortalResponse::Response, QVariantMap> continueStartAfterDialog(ScreenCastSession *session,
                                                                              const QList<QPointer<QScreen>> &selectedOutputs,
                                                                              const QRect &selectedRegion,
                                                                              bool allowRestore);
};
