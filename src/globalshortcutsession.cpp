// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "globalshortcutsession.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(GlobalShortcutSessionLog, "xdg-dde-global-shortcut.session")

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

GlobalShortcutSession::GlobalShortcutSession(const QDBusObjectPath &handle,
                                             const QString &appId,
                                             const QString &brokerOwner,
                                             QObject *parent)
    : QObject(parent)
    , m_handle(handle)
    , m_appId(appId)
    , m_brokerOwner(brokerOwner)
    , m_sessionToken(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

GlobalShortcutSession::~GlobalShortcutSession()
{
    if (m_registered)
        QDBusConnection::sessionBus().unregisterObject(m_handle.path());
}

bool GlobalShortcutSession::registerObject()
{
    if (m_registered)
        return true;

    m_registered = QDBusConnection::sessionBus().registerObject(
            m_handle.path(),
            this,
            QDBusConnection::ExportScriptableSlots
                    | QDBusConnection::ExportScriptableSignals
                    | QDBusConnection::ExportScriptableProperties);
    if (!m_registered) {
        qCWarning(GlobalShortcutSessionLog)
                << "Failed to register session" << m_handle.path()
                << QDBusConnection::sessionBus().lastError().message();
    }
    return m_registered;
}

void GlobalShortcutSession::beginBind(const XdgShortcutList &shortcuts)
{
    m_bindAttempted = true;
    m_state = State::BindPending;
    m_declaredOrder.clear();
    m_declaredProperties.clear();
    m_boundIds.clear();
    m_boundProperties.clear();
    for (const XdgShortcut &shortcut : shortcuts) {
        m_declaredOrder.append(shortcut.id);
        m_declaredProperties.insert(shortcut.id, shortcut.properties);
    }
}

void GlobalShortcutSession::finishBind(const XdgShortcutList &boundShortcuts)
{
    updateBoundShortcuts(boundShortcuts);
    m_state = State::Bound;
}

void GlobalShortcutSession::updateBoundShortcuts(const XdgShortcutList &boundShortcuts)
{
    m_boundIds.clear();
    m_boundProperties.clear();
    for (const XdgShortcut &shortcut : boundShortcuts) {
        m_boundIds.insert(shortcut.id);
        m_boundProperties.insert(shortcut.id, shortcut.properties);
    }
}

void GlobalShortcutSession::failBind()
{
    m_boundIds.clear();
    m_boundProperties.clear();
    m_state = State::DeclaredOnly;
}

XdgShortcutList GlobalShortcutSession::activeShortcuts() const
{
    XdgShortcutList shortcuts;
    shortcuts.reserve(m_boundIds.size());
    for (const QString &id : m_declaredOrder) {
        if (m_boundIds.contains(id))
            shortcuts.append({id, m_boundProperties.value(id)});
    }
    return shortcuts;
}

void GlobalShortcutSession::Close(const QDBusMessage &message)
{
    if (message.service().isEmpty() || message.service() != m_brokerOwner
            || !isCurrentBroker(message.service())) {
        QDBusConnection::sessionBus().send(
                message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                                         QStringLiteral("The session belongs to another caller")));
        return;
    }
    QDBusConnection::sessionBus().send(message.createReply());
    if (m_state == State::Closed)
        return;
    m_state = State::Closed;
    if (m_closeHandler)
        m_closeHandler(this);
}
