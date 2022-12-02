// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "notification.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDENotification, "xdg-dde-notification")

// TODO: if needed
NotificationPortal::NotificationPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qCDebug(XdgDesktopDDENotification) << "init NotificationProtal";
}

void NotificationPortal::AddNotification(const QString &app_id, const QString &id, const QVariantMap &notification)
{
    qCDebug(XdgDesktopDDENotification) << "notification add start";
}

void NotificationPortal::RemoveNotification(const QString &app_id, const QString &id)
{
    qCDebug(XdgDesktopDDENotification) << "notification remove start";
}
