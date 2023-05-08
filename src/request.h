// Copyright © 2017 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_REQUEST_H
#define XDG_DESKTOP_PORTAL_KDE_REQUEST_H

#include <QObject>
#include <QDBusVirtualObject>

class Request : public QDBusVirtualObject
{
    Q_OBJECT
public:
    explicit Request(QObject *parent = nullptr, const QString &portalName = QString(), const QVariant &data = QVariant());
    ~Request() override;

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;
    QString introspect(const QString &path) const override;

Q_SIGNALS:
    void closeRequested();

private:
    const QVariant m_data;
    const QString m_portalName;
};

#endif // XDG_DESKTOP_PORTAL_KDE_REQUEST_H

