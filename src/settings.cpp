// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dbushelpers.h"
#include "settings.h"

#include <QLoggingCategory>
#include <QDBusContext>
#include <QDBusMessage>
#include <QApplication>
#include <QPalette>
#include <QDBusConnection>
#include <QDBusMetaType>

#define NO_PREFERENCE 0
#define PREFER_DARK_APPEARANCE 1
#define PREFER_LIGHT_APPEARANCE 2

Q_LOGGING_CATEGORY(XdgDesktopDDESetting, "xdg-dde-settings")

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
    connect(qApp, &QApplication::paletteChanged, this, &SettingsPortal::onPaletteChanged);
}

void SettingsPortal::Read(const QString &group, const QString &key)
{
    qCDebug(XdgDesktopDDESetting) << "Read: group " << group << " key " << key;

    QObject *obj = QObject::parent();

    if (!obj) {
        qCWarning(XdgDesktopDDESetting) << "Failed to get dbus context";
        return;
    }

    void *ptr = obj->qt_metacast("QDBusContext");
    QDBusContext *q_ptr = reinterpret_cast<QDBusContext *>(ptr);

    if (!q_ptr) {
        qCWarning(XdgDesktopDDESetting) << "Failed to get dbus context";
        return;
    }

    QDBusMessage reply;
    QDBusMessage message = q_ptr->message();

    if (group == QLatin1String("org.freedesktop.appearance") && key == QLatin1String("color-scheme")) {
        reply = message.createReply(QVariant::fromValue(readFdoColorScheme()));
        QDBusConnection::sessionBus().send(reply);
        return;
    }
}

void SettingsPortal::onPaletteChanged(const QPalette &palette)
{
    Q_EMIT SettingChanged(QStringLiteral("org.freedesktop.appearance"), QStringLiteral("color-scheme"), readFdoColorScheme());
}

void SettingsPortal::ReadAll(const QStringList &groups)
{
    qCDebug(XdgDesktopDDESetting) << "ReadAll";
    qCDebug(XdgDesktopDDESetting) << "ReadAll called with parameters:";
    qCDebug(XdgDesktopDDESetting) << "    groups: " << groups;

    QObject *obj = QObject::parent();

    if (!obj) {
        qCWarning(XdgDesktopDDESetting) << "Failed to get dbus context";
        return;
    }

    void *ptr = obj->qt_metacast("QDBusContext");
    QDBusContext *q_ptr = reinterpret_cast<QDBusContext *>(ptr);

    if (!q_ptr) {
        qCWarning(XdgDesktopDDESetting) << "Failed to get dbus context";
        return;
    }

    VariantMapMap result;

    if (groupMatches(QStringLiteral("org.freedesktop.appearance"), groups)) {
        QVariantMap appearanceSettings;
        appearanceSettings.insert(QStringLiteral("color-scheme"), readFdoColorScheme().variant());

        result.insert(QStringLiteral("org.freedesktop.appearance"), appearanceSettings);
    }
    QDBusMessage message = q_ptr->message();
    QDBusMessage reply = message.createReply(QVariant::fromValue(result));
    QDBusConnection::sessionBus().send(reply);
}

QDBusVariant SettingsPortal::readFdoColorScheme()
{
    const QPalette palette = QApplication::palette();
    const int windowBackgroundGray = qGray(palette.window().color().rgb());

    uint result = NO_PREFERENCE; // no preference

    if (windowBackgroundGray < 192) {
        result = PREFER_DARK_APPEARANCE; // prefer dark
    } else {
        result = PREFER_LIGHT_APPEARANCE; // prefer light
    }

    return QDBusVariant(result);
}
