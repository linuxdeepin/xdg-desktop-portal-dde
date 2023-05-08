// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_SCREENCAST_H
#define XDG_DESKTOP_PORTAL_KDE_SCREENCAST_H

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>

class ScreenCastCommon;

class ScreenCastPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.ScreenCast")
    Q_PROPERTY(uint version READ version)
    Q_PROPERTY(uint AvailableSourceTypes READ AvailableSourceTypes)
public:
    enum SourceType {
        Any = 0,
        Monitor,
        Window
    };

    explicit ScreenCastPortal(QObject *parent);
    ~ScreenCastPortal();

    uint version() const { return 1; }
    uint AvailableSourceTypes() const { return Monitor; };

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

    uint Start(const QDBusObjectPath &handle,
               const QDBusObjectPath &session_handle,
               const QString &app_id,
               const QString &parent_window,
               const QVariantMap &options,
               QVariantMap &results);
};

#endif // XDG_DESKTOP_PORTAL_KDE_SCREENCAST_H

