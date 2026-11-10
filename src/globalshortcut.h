// SPDX-FileCopyrightText: 2021 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "globalshortcuttypes.h"

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>

class GlobalShortcutsController;
class GlobalShortcutsManagement;
class QDBusMessage;

class GlobalShortcutPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.GlobalShortcuts")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    explicit GlobalShortcutPortal(QObject *parent);
    ~GlobalShortcutPortal() override = default;

    uint version() const { return 2; }
    GlobalShortcutsController *controller() const { return m_controller; }

public Q_SLOTS:
    uint CreateSession(const QDBusObjectPath &handle,
                       const QDBusObjectPath &session_handle,
                       const QString &app_id,
                       const QVariantMap &options,
                       const QDBusMessage &message,
                       QVariantMap &results);
    uint BindShortcuts(const QDBusObjectPath &handle,
                       const QDBusObjectPath &session_handle,
                       const XdgShortcutList &shortcuts,
                       const QString &parent_window,
                       const QVariantMap &options,
                       const QDBusMessage &message,
                       QVariantMap &results);
    uint ListShortcuts(const QDBusObjectPath &handle,
                       const QDBusObjectPath &session_handle,
                       const QDBusMessage &message,
                       QVariantMap &results);
    void ConfigureShortcuts(const QDBusObjectPath &session_handle,
                            const QString &parent_window,
                            const QVariantMap &options,
                            const QDBusMessage &message);

Q_SIGNALS:
    void Activated(const QDBusObjectPath &session_handle,
                   const QString &shortcut_id,
                   qulonglong timestamp,
                   const QVariantMap &options);
    void Deactivated(const QDBusObjectPath &session_handle,
                     const QString &shortcut_id,
                     qulonglong timestamp,
                     const QVariantMap &options);
    void ShortcutsChanged(const QDBusObjectPath &session_handle,
                          const XdgShortcutList &shortcuts);

private:
    GlobalShortcutsController *const m_controller;
    GlobalShortcutsManagement *const m_management;
};
