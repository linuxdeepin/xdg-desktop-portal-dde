/*
 * SPDX-FileCopyrightText: 2016 Red Hat Inc
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * SPDX-FileCopyrightText: 2016 Jan Grulich <jgrulich@redhat.com>
 */

#include "request2.h"
#include "loggings.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

Request2::Request2(const QDBusObjectPath &handle, QObject *parent, const QString &portalName, const QVariant &data)
    : QDBusVirtualObject(parent)
    , m_data(data)
    , m_portalName(portalName)
    , m_path(handle.path())
{
    auto sessionBus = QDBusConnection::sessionBus();
    m_registered = sessionBus.registerVirtualObject(
            m_path, this, QDBusConnection::VirtualObjectRegisterOption::SingleNode);
    if (!m_registered) {
        qCDebug(PORTAL_COMMON) << sessionBus.lastError().message();
        qCDebug(PORTAL_COMMON) << "Failed to register request object for" << m_path;
    }
}

Request2::~Request2()
{
    unregister();
}

void Request2::unregister()
{
    if (!m_registered) {
        return;
    }
    QDBusConnection::sessionBus().unregisterObject(m_path);
    m_registered = false;
}

bool Request2::handleMessage(const QDBusMessage &message, const QDBusConnection &connection)
{
    if (!m_registered || message.type() != QDBusMessage::MessageType::MethodCallMessage
        || message.path() != m_path) {
        return false;
    }

    qCDebug(PORTAL_COMMON) << message.interface();
    qCDebug(PORTAL_COMMON) << message.member();
    qCDebug(PORTAL_COMMON) << message.path();

    if (message.interface() == QLatin1String("org.freedesktop.impl.portal.Request")) {
        if (message.member() == QLatin1String("Close")) {
            Q_EMIT closeRequested();
            QDBusMessage reply = message.createReply();
            const bool sent = connection.send(reply);
            unregister();
            deleteLater();
            return sent;
        }
    }

    return false;
}

QString Request2::introspect(const QString &path) const
{
    QString nodes;

    if (path == m_path) {
        nodes = QStringLiteral(
            "<interface name=\"org.freedesktop.impl.portal.Request\">"
            "    <method name=\"Close\">"
            "    </method>"
            "</interface>");
    }

    qCDebug(PORTAL_COMMON) << nodes;

    return nodes;
}
