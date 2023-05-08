// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "settings.h"

#include <QDBusMetaType>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusConnection>

#include <QLoggingCategory>

#include <KConfigCore/KConfigGroup>

Q_LOGGING_CATEGORY(XdgDesktopPortalKdeSettings, "xdp-kde-settings")

Q_DECLARE_METATYPE(SettingsPortal::VariantMapMap)

QDBusArgument &operator<<(QDBusArgument &argument, const SettingsPortal::VariantMapMap &mymap)
{
    argument.beginMap(QVariant::String, QVariant::Map);

    QMapIterator<QString, QVariantMap> i(mymap);
    while (i.hasNext()) {
        i.next();
        argument.beginMapEntry();
        argument << i.key() << i.value();
        argument.endMapEntry();
    }
    argument.endMap();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SettingsPortal::VariantMapMap &mymap)
{
    argument.beginMap();
    mymap.clear();

    while (!argument.atEnd()) {
        QString key;
        QVariantMap value;
        argument.beginMapEntry();
        argument >> key >> value;
        argument.endMapEntry();
        mymap.insert(key, value);
    }

    argument.endMap();
    return argument;
}

static bool groupMatches(const QString &group, const QStringList &patterns)
{
    for (const QString &pattern : patterns) {
        if (pattern.isEmpty()) {
            return true;
        }

        if (pattern == group) {
            return true;
        }

        if (pattern.endsWith(QLatin1Char('*')) && group.startsWith(pattern.left(pattern.length() - 1))) {
            return true;
        }
    }

    return false;
}

SettingsPortal::SettingsPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qDBusRegisterMetaType<VariantMapMap>();

    m_kdeglobals = KSharedConfig::openConfig();

    QDBusConnection::sessionBus().connect(QString(), QStringLiteral("/KDEPlatformTheme"), QStringLiteral("org.kde.KDEPlatformTheme"),
                                          QStringLiteral("refreshFonts"), this, SLOT(fontChanged()));
    QDBusConnection::sessionBus().connect(QString(), QStringLiteral("/KGlobalSettings"), QStringLiteral("org.kde.KGlobalSettings"),
                                          QStringLiteral("notifyChange"), this, SLOT(globalSettingChanged(int,int)));
    QDBusConnection::sessionBus().connect(QString(), QStringLiteral("/KToolBar"), QStringLiteral("org.kde.KToolBar"),
                                          QStringLiteral("styleChanged"), this, SLOT(toolbarStyleChanged()));
}

SettingsPortal::~SettingsPortal()
{
}

void SettingsPortal::ReadAll(const QStringList &groups)
{
    qCDebug(XdgDesktopPortalKdeSettings) << "ReadAll called with parameters:";
    qCDebug(XdgDesktopPortalKdeSettings) << "    groups: " << groups;

    //FIXME this is super ugly, but I was unable to make it properly return VariantMapMap
    QObject *obj = QObject::parent();

    if (!obj) {
        qCWarning(XdgDesktopPortalKdeSettings) << "Failed to get dbus context";
        return;
    }

    void *ptr = obj->qt_metacast("QDBusContext");
    QDBusContext *q_ptr = reinterpret_cast<QDBusContext *>(ptr);

    if (!q_ptr) {
        qCWarning(XdgDesktopPortalKdeSettings) << "Failed to get dbus context";
        return;
    }

    VariantMapMap result;
    for (const QString &settingGroupName : m_kdeglobals->groupList()) {
        //NOTE: use org.kde.kdeglobals prefix

        QString uniqueGroupName = QStringLiteral("org.kde.kdeglobals.") + settingGroupName;

        if (!groupMatches(uniqueGroupName, groups)) {
            continue;
        }

        QVariantMap map;
        KConfigGroup configGroup(m_kdeglobals, settingGroupName);

        for (const QString &key : configGroup.keyList()) {
            map.insert(key, configGroup.readEntry(key));
        }

        result.insert(uniqueGroupName, map);
    }

    QDBusMessage message = q_ptr->message();
    QDBusMessage reply = message.createReply(QVariant::fromValue(result));
    QDBusConnection::sessionBus().send(reply);
}

QDBusVariant SettingsPortal::Read(const QString &group, const QString &key)
{
    qCDebug(XdgDesktopPortalKdeSettings) << "Read called with parameters:";
    qCDebug(XdgDesktopPortalKdeSettings) << "    group: " << group;
    qCDebug(XdgDesktopPortalKdeSettings) << "    key: " << key;

    // All our namespaces start with this prefix
    if (!group.startsWith(QStringLiteral("org.kde.kdeglobals"))) {
        qCWarning(XdgDesktopPortalKdeSettings) << "Namespace " << group << " is not supported";
        return QDBusVariant();
    }

    QString groupName = group.right(group.length() - QStringLiteral("org.kde.kdeglobals.").length());

    if (!m_kdeglobals->hasGroup(groupName)) {
        qCWarning(XdgDesktopPortalKdeSettings) << "Group " << group << " doesn't exist";
        return QDBusVariant();
    }

    KConfigGroup configGroup(m_kdeglobals, groupName);

    if (!configGroup.hasKey(key)) {
        qCWarning(XdgDesktopPortalKdeSettings) << "Key " << key << " doesn't exist";
        return QDBusVariant();
    }

    return QDBusVariant(configGroup.readEntry(key));
}

void SettingsPortal::fontChanged()
{
    Q_EMIT SettingChanged(QStringLiteral("org.kde.kdeglobals.General"), QStringLiteral("font"), Read(QStringLiteral("org.kde.kdeglobals.General"), QStringLiteral("font")));
}

void SettingsPortal::globalSettingChanged(int type, int arg)
{
    m_kdeglobals->reparseConfiguration();

    // Mostly based on plasma-integration needs
    switch (type) {
    case PaletteChanged:
        // Plasma-integration will be loading whole palette again, there is no reason to try to identify
        // particular categories or colors
        Q_EMIT SettingChanged(QStringLiteral("org.kde.kdeglobals.General"), QStringLiteral("ColorScheme"), Read(QStringLiteral("org.kde.kdeglobals.General"), QStringLiteral("ColorScheme")));
        break;
    case FontChanged:
        fontChanged();
        break;
    case StyleChanged:
        Q_EMIT SettingChanged(QStringLiteral("org.kde.kdeglobals.KDE"), QStringLiteral("widgetStyle"), Read(QStringLiteral("org.kde.kdeglobals.KDE"), QStringLiteral("widgetStyle")));
        break;
    case SettingsChanged: {
        SettingsCategory category = static_cast<SettingsCategory>(arg);
        if (category == SETTINGS_QT || category == SETTINGS_MOUSE) {
            // TODO
        } else if (category == SETTINGS_STYLE) {
            // TODO
        }
        break;
    }
    case IconChanged:
        // we will get notified about each category, but it probably makes sense to send this signal just once
        if (arg == 0) { // KIconLoader::Desktop
            Q_EMIT SettingChanged(QStringLiteral("org.kde.kdeglobals.Icons"), QStringLiteral("Theme"), Read(QStringLiteral("org.kde.kdeglobals.Icons"), QStringLiteral("Theme")));
        }
        break;
    case CursorChanged:
        // TODO
        break;
    case ToolbarStyleChanged:
        toolbarStyleChanged();
        break;
    default:
        break;
    }
}

void SettingsPortal::toolbarStyleChanged()
{
    Q_EMIT SettingChanged(QStringLiteral("org.kde.kdeglobals.Toolbar style"), QStringLiteral("ToolButtonStyle"), Read(QStringLiteral("org.kde.kdeglobals.Toolbar style"), QStringLiteral("ToolButtonStyle")));
}
