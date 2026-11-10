// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "globalshortcutrequest.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(GlobalShortcutRequestLog, "xdg-dde-global-shortcut.request")

namespace {

constexpr auto PortalBrokerService = "org.freedesktop.portal.Desktop";

bool isCurrentBroker(const QString &sender)
{
    QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
    const QDBusReply<QString> owner = interface
            ? interface->serviceOwner(QString::fromLatin1(PortalBrokerService))
            : QDBusReply<QString>();
    return owner.isValid() && !owner.value().isEmpty() && owner.value() == sender;
}

} // namespace

GlobalShortcutRequest::GlobalShortcutRequest(const QDBusObjectPath &handle,
                                             const QString &brokerOwner,
                                             QObject *parent)
    : QObject(parent)
    , m_handle(handle)
    , m_brokerOwner(brokerOwner)
{
}

GlobalShortcutRequest::~GlobalShortcutRequest()
{
    if (m_registered)
        QDBusConnection::sessionBus().unregisterObject(m_handle.path());
}

bool GlobalShortcutRequest::registerObject()
{
    if (m_registered)
        return true;

    m_registered = QDBusConnection::sessionBus().registerObject(
            m_handle.path(), this, QDBusConnection::ExportScriptableSlots);
    if (!m_registered) {
        qCWarning(GlobalShortcutRequestLog)
                << "Failed to register request" << m_handle.path()
                << QDBusConnection::sessionBus().lastError().message();
    }
    return m_registered;
}

void GlobalShortcutRequest::Close(const QDBusMessage &message)
{
    if (message.service().isEmpty() || message.service() != m_brokerOwner
            || !isCurrentBroker(message.service())) {
        QDBusConnection::sessionBus().send(
                message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                                         QStringLiteral("The request belongs to another caller")));
        return;
    }
    QDBusConnection::sessionBus().send(message.createReply());
    if (m_closed)
        return;
    m_closed = true;
    Q_EMIT closeRequested(this);
}
