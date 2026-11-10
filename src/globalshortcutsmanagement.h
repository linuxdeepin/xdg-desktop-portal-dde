// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "globalshortcuttypes.h"

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>

class GlobalShortcutsController;
class QDBusMessage;

class GlobalShortcutsManagement : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.dde.impl.portal.GlobalShortcutsManagement1")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    GlobalShortcutsManagement(QObject *parent, GlobalShortcutsController *controller);

    uint version() const { return 1; }

public Q_SLOTS:
    void CompleteBind(const QString &request_id,
                      uint response,
                      const PortalShortcutUpdateList &selected,
                      const QDBusMessage &message);
    PortalShortcutInfoList ApplySessionShortcuts(
            const QDBusObjectPath &session_handle,
            const PortalShortcutUpdateList &updates,
            const QDBusMessage &message);
    PortalShortcutConflictList CheckSessionShortcutConflicts(
            const QDBusObjectPath &session_handle,
            const PortalShortcutUpdateList &shortcuts,
            const QDBusMessage &message);

private:
    GlobalShortcutsController *const m_controller;
};
