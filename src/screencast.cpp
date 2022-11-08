// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screencast.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDEScreenCastProtal, "xdg-dde-screencast")

ScreenCastPortal::ScreenCastPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qCDebug(XdgDesktopDDEScreenCastProtal) << "init screencast";
}

uint ScreenCastPortal::CreateSession(const QDBusObjectPath &handle,
                                     const QDBusObjectPath &session_handle,
                                     const QString &app_id,
                                     const QVariantMap &options,
                                     QVariantMap &results)
{
    qCDebug(XdgDesktopDDEScreenCastProtal) << "screencast createsession started";
    return 0;
}

uint ScreenCastPortal::SelectSources(const QDBusObjectPath &handle,
                                     const QDBusObjectPath &session_handle,
                                     const QString &app_id,
                                     const QVariantMap &options,
                                     QVariantMap &results)
{
    qCDebug(XdgDesktopDDEScreenCastProtal) << "screencast selectsource started";
    return 0;
}

uint ScreenCastPortal::Start(const QDBusObjectPath &handle,
                             const QDBusObjectPath &session_handle,
                             const QString &app_id,
                             const QString &parent_window,
                             const QVariantMap &options,
                             QVariantMap &results)
{
    qCDebug(XdgDesktopDDEScreenCastProtal) << "screencast started";
    return 0;
}
