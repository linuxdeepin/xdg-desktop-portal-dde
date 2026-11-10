// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "globalshortcuttypes.h"

#include <QDBusMetaType>

QDBusArgument &operator<<(QDBusArgument &argument, const XdgShortcut &shortcut)
{
    argument.beginStructure();
    argument << shortcut.id << shortcut.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, XdgShortcut &shortcut)
{
    argument.beginStructure();
    argument >> shortcut.id >> shortcut.properties;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcutInfo &shortcut)
{
    argument.beginStructure();
    argument << shortcut.id << shortcut.appId << shortcut.shortcutId << shortcut.description
             << shortcut.hotkeys << shortcut.preferredKeys << shortcut.enabled << shortcut.active;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcutInfo &shortcut)
{
    argument.beginStructure();
    argument >> shortcut.id >> shortcut.appId >> shortcut.shortcutId >> shortcut.description
             >> shortcut.hotkeys >> shortcut.preferredKeys >> shortcut.enabled >> shortcut.active;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcutUpdate &shortcut)
{
    argument.beginStructure();
    argument << shortcut.shortcutId << shortcut.description << shortcut.hotkeys
             << shortcut.preferredKeys << shortcut.enabled;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcutUpdate &shortcut)
{
    argument.beginStructure();
    argument >> shortcut.shortcutId >> shortcut.description >> shortcut.hotkeys
             >> shortcut.preferredKeys >> shortcut.enabled;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcutConflict &conflict)
{
    argument.beginStructure();
    argument << conflict.shortcutId << conflict.hotkey << conflict.conflictType
             << conflict.conflictingAppId << conflict.conflictingShortcutId;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcutConflict &conflict)
{
    argument.beginStructure();
    argument >> conflict.shortcutId >> conflict.hotkey >> conflict.conflictType
             >> conflict.conflictingAppId >> conflict.conflictingShortcutId;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalTrigger &trigger)
{
    argument.beginStructure();
    argument << trigger.shortcutId << trigger.trigger;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalTrigger &trigger)
{
    argument.beginStructure();
    argument >> trigger.shortcutId >> trigger.trigger;
    argument.endStructure();
    return argument;
}

void registerGlobalShortcutMetaTypes()
{
    qDBusRegisterMetaType<XdgShortcut>();
    qDBusRegisterMetaType<XdgShortcutList>();
    qDBusRegisterMetaType<PortalShortcutInfo>();
    qDBusRegisterMetaType<PortalShortcutInfoList>();
    qDBusRegisterMetaType<PortalShortcutUpdate>();
    qDBusRegisterMetaType<PortalShortcutUpdateList>();
    qDBusRegisterMetaType<PortalShortcutConflict>();
    qDBusRegisterMetaType<PortalShortcutConflictList>();
    qDBusRegisterMetaType<PortalTrigger>();
    qDBusRegisterMetaType<PortalTriggerList>();
}
