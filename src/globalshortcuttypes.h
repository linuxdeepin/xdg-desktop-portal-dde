// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QDBusArgument>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantMap>

struct XdgShortcut
{
    QString id;
    QVariantMap properties;
};
using XdgShortcutList = QList<XdgShortcut>;

struct PortalShortcutInfo
{
    QString id;
    QString appId;
    QString shortcutId;
    QString description;
    QStringList hotkeys;
    QStringList preferredKeys;
    bool enabled = false;
    bool active = false;
};
using PortalShortcutInfoList = QList<PortalShortcutInfo>;

struct PortalShortcutUpdate
{
    QString shortcutId;
    QString description;
    QStringList hotkeys;
    QStringList preferredKeys;
    bool enabled = false;
};
using PortalShortcutUpdateList = QList<PortalShortcutUpdate>;

struct PortalShortcutConflict
{
    QString shortcutId;
    QString hotkey;
    QString conflictType;
    QString conflictingAppId;
    QString conflictingShortcutId;
};
using PortalShortcutConflictList = QList<PortalShortcutConflict>;

struct PortalTrigger
{
    QString shortcutId;
    QString trigger;
};
using PortalTriggerList = QList<PortalTrigger>;

Q_DECLARE_METATYPE(XdgShortcut)
Q_DECLARE_METATYPE(XdgShortcutList)
Q_DECLARE_METATYPE(PortalShortcutInfo)
Q_DECLARE_METATYPE(PortalShortcutInfoList)
Q_DECLARE_METATYPE(PortalShortcutUpdate)
Q_DECLARE_METATYPE(PortalShortcutUpdateList)
Q_DECLARE_METATYPE(PortalShortcutConflict)
Q_DECLARE_METATYPE(PortalShortcutConflictList)
Q_DECLARE_METATYPE(PortalTrigger)
Q_DECLARE_METATYPE(PortalTriggerList)

QDBusArgument &operator<<(QDBusArgument &argument, const XdgShortcut &shortcut);
const QDBusArgument &operator>>(const QDBusArgument &argument, XdgShortcut &shortcut);

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcutInfo &shortcut);
const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcutInfo &shortcut);

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcutUpdate &shortcut);
const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcutUpdate &shortcut);

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcutConflict &conflict);
const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcutConflict &conflict);

QDBusArgument &operator<<(QDBusArgument &argument, const PortalTrigger &trigger);
const QDBusArgument &operator>>(const QDBusArgument &argument, PortalTrigger &trigger);

void registerGlobalShortcutMetaTypes();
