// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QDBusObjectPath>
#include <QObject>

class QDBusMessage;

class GlobalShortcutRequest : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Request")

public:
    explicit GlobalShortcutRequest(const QDBusObjectPath &handle,
                                   const QString &brokerOwner,
                                   QObject *parent = nullptr);
    ~GlobalShortcutRequest() override;

    bool registerObject();
    QString path() const { return m_handle.path(); }

public Q_SLOTS:
    Q_SCRIPTABLE void Close(const QDBusMessage &message);

Q_SIGNALS:
    void closeRequested(GlobalShortcutRequest *request);

private:
    QDBusObjectPath m_handle;
    QString m_brokerOwner;
    bool m_registered = false;
    bool m_closed = false;
};
