/*
 * SPDX-FileCopyrightText: 2017 Red Hat Inc
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * SPDX-FileCopyrightText: 2017 Jan Grulich <jgrulich@redhat.com>
 * SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
 */

#pragma once

#include "session.h"

#include <QDBusVirtualObject>
#include <QObject>

class QDBusObjectPath;

class Request2 : public QDBusVirtualObject
{
    Q_OBJECT
public:
    explicit Request2(const QDBusObjectPath &handle, QObject *parent = nullptr, const QString &portalName = QString(), const QVariant &data = QVariant());

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;
    QString introspect(const QString &path) const override;

    template<class T>
    static Request2 *makeClosableDialogRequest(const QDBusObjectPath &handle, T *dialogAndParent)
    {
        auto request = new Request2(handle, dialogAndParent);
        connect(request, &Request2::closeRequested, dialogAndParent, &T::reject);
        return request;
    }

    template<class T>
    static Request2 *makeClosableDialogRequestWithSession(const QDBusObjectPath &handle, T *dialogAndParent, Session *session)
    {
        auto request = makeClosableDialogRequest(handle, dialogAndParent);
        connect(session, &Session::closed, dialogAndParent, &T::reject);
        return request;
    }

Q_SIGNALS:
    void closeRequested();

private:
    const QVariant m_data;
    const QString m_portalName;
};
