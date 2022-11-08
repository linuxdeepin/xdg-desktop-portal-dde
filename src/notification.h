// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>

class NotificationProtal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Notification")

public:
    explicit NotificationProtal(QObject *parent);
    ~NotificationProtal() = default;

signals:
    void ActionInvoked(const QString &, const QString &, const QString &, const QList<QVariant> &) const;

public slots:
    void AddNotification(const QString &app_id, const QString &id, const QVariantMap &notification);
    void RemoveNotification(const QString &app_id, const QString &id);
};
