// SPDX-FileCopyrightText: 2021 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "globalshortcut.h"

#include "globalshortcutscontroller.h"
#include "globalshortcutsmanagement.h"

#include <QDBusMessage>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDEGlobalShortcut, "xdg-dde-global-shortcut")

GlobalShortcutPortal::GlobalShortcutPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
    , m_controller(new GlobalShortcutsController(parent))
    , m_management(new GlobalShortcutsManagement(parent, m_controller))
{
    connect(m_controller,
            &GlobalShortcutsController::activated,
            this,
            &GlobalShortcutPortal::Activated);
    connect(m_controller,
            &GlobalShortcutsController::deactivated,
            this,
            &GlobalShortcutPortal::Deactivated);
    connect(m_controller,
            &GlobalShortcutsController::shortcutsChanged,
            this,
            &GlobalShortcutPortal::ShortcutsChanged);
    qCDebug(XdgDesktopDDEGlobalShortcut) << "Initialized GlobalShortcuts portal v2";
}

uint GlobalShortcutPortal::CreateSession(const QDBusObjectPath &handle,
                                         const QDBusObjectPath &session_handle,
                                         const QString &app_id,
                                         const QVariantMap &options,
                                         const QDBusMessage &message,
                                         QVariantMap &results)
{
    return m_controller->createSession(
            handle, session_handle, app_id, options, results, message);
}

uint GlobalShortcutPortal::BindShortcuts(const QDBusObjectPath &handle,
                                         const QDBusObjectPath &session_handle,
                                         const XdgShortcutList &shortcuts,
                                         const QString &parent_window,
                                         const QVariantMap &options,
                                         const QDBusMessage &message,
                                         QVariantMap &results)
{
    return m_controller->bindShortcuts(
            handle, session_handle, shortcuts, parent_window, options, results, message);
}

uint GlobalShortcutPortal::ListShortcuts(const QDBusObjectPath &handle,
                                         const QDBusObjectPath &session_handle,
                                         const QDBusMessage &message,
                                         QVariantMap &results)
{
    return m_controller->listShortcuts(handle, session_handle, results, message);
}

void GlobalShortcutPortal::ConfigureShortcuts(const QDBusObjectPath &session_handle,
                                              const QString &parent_window,
                                              const QVariantMap &options,
                                              const QDBusMessage &message)
{
    m_controller->configureShortcuts(session_handle, parent_window, options, message);
}
