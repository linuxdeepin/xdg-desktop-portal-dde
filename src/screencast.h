// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <qobjectdefs.h>

class ScreenCastPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.ScreenCast")
    //Q_PROPERTY(uint version READ version CONSTANT)
    //Q_PROPERTY(uint AvailableSourceTypes READ AvailableSourceTypes CONSTANT)
    //Q_PROPERTY(uint AvailableCursorModes READ AvailableCursorModes CONSTANT)

public:
    enum SourceType {
        Any = 0,
        Monitor = 1,
        Window = 2,
        Virtual = 4,
    };
    Q_ENUM(SourceType)
    Q_DECLARE_FLAGS(SourceTypes, SourceType)

    enum CursorModes {
        Hidden = 1,
        Embedded = 2,
        Metadata = 4,
    };
    Q_ENUM(CursorModes)

    enum PersistMode {
        NoPersist = 0,
        PersistWhilerunning = 1,
        PersistUntilRevoked = 2,
    };

    Q_ENUM(PersistMode)

    explicit ScreenCastPortal(QObject *parent);
    ~ScreenCastPortal() = default;

public slots:
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
