// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "globalshortcutsmanagement.h"

#include "globalshortcutscontroller.h"

#include <QDBusConnection>
#include <QDBusMessage>

GlobalShortcutsManagement::GlobalShortcutsManagement(
        QObject *parent,
        GlobalShortcutsController *controller)
    : QDBusAbstractAdaptor(parent)
    , m_controller(controller)
{
}

void GlobalShortcutsManagement::CompleteBind(
        const QString &request_id,
        uint response,
        const PortalShortcutUpdateList &selected,
        const QDBusMessage &message)
{
    QString errorName;
    QString errorText;
    if (m_controller->completeBind(request_id,
                                   response,
                                   selected,
                                   message,
                                   &errorName,
                                   &errorText)) {
        return;
    }
    message.setDelayedReply(true);
    QDBusConnection::sessionBus().send(message.createErrorReply(errorName, errorText));
}

PortalShortcutInfoList GlobalShortcutsManagement::ApplySessionShortcuts(
        const QDBusObjectPath &session_handle,
        const PortalShortcutUpdateList &updates,
        const QDBusMessage &message)
{
    message.setDelayedReply(true);
    m_controller->applySessionShortcuts(session_handle, updates, message);
    return {};
}

PortalShortcutConflictList GlobalShortcutsManagement::CheckSessionShortcutConflicts(
        const QDBusObjectPath &session_handle,
        const PortalShortcutUpdateList &shortcuts,
        const QDBusMessage &message)
{
    message.setDelayedReply(true);
    m_controller->checkSessionShortcutConflicts(session_handle, shortcuts, message);
    return {};
}
