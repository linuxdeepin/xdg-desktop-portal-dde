// SPDX-FileCopyrightText: 2021-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "settings.h"

#include "dbushelpers.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QLoggingCategory>
#include <QPalette>
#include <QEvent>

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

        if (pattern.endsWith(QLatin1Char('*'))
            && group.startsWith(pattern.left(pattern.length() - 1))) {
            return true;
        }
    }

    return false;
}

/* accent-color */
struct AccentColorArray
{
    double r = 0.0; // 0-1
    double g = 0.0; // 0-1
    double b = 0.0; // 0-1

    operator QVariant() const { return QVariant::fromValue(*this); }
};
Q_DECLARE_METATYPE(AccentColorArray)

QDBusArgument &operator<<(QDBusArgument &argument, const AccentColorArray &item)
{
    argument.beginStructure();
    argument << item.r << item.g << item.b;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, AccentColorArray &item)
{
    argument.beginStructure();
    argument >> item.r >> item.g >> item.b;
    argument.endStructure();
    return argument;
}

SettingsPortal::SettingsPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qDBusRegisterMetaType<VariantMapMap>();
    qDBusRegisterMetaType<AccentColorArray>();
    qApp->installEventFilter(this);
}

bool SettingsPortal::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange) {
        onPaletteChanged(qApp->palette());
    }
    return QDBusAbstractAdaptor::eventFilter(obj, event);
}

QDBusVariant SettingsPortal::Read(const QString &group,
                                  const QString &key,
                                  const QDBusMessage &message)
{
    qCDebug(XdgDesktopDDESetting) << "Read: group " << group << " key " << key;

    if (group == QLatin1String("org.freedesktop.appearance")) {
        if (key == QLatin1String("color-scheme")) {
            return readFdoColorScheme();
        }
        if (key == QLatin1String("accent-color")) {
            return readAccentColor();
        }
    }

    message.setDelayedReply(true);
    const QDBusMessage reply = message.createErrorReply(
            QDBusError::UnknownProperty,
            QStringLiteral("Requested setting is not supported"));
    QDBusConnection::sessionBus().send(reply);
    return {};
}

void SettingsPortal::onPaletteChanged(const QPalette &palette)
{
    Q_EMIT SettingChanged(QStringLiteral("org.freedesktop.appearance"),
                          QStringLiteral("color-scheme"),
                          readFdoColorScheme());
    Q_EMIT SettingChanged(QStringLiteral("org.freedesktop.appearance"),
                          QStringLiteral("accent-color"),
                          readAccentColor());
}

VariantMapMap SettingsPortal::ReadAll(const QStringList &groups)
{
    qCDebug(XdgDesktopDDESetting) << "ReadAll";
    qCDebug(XdgDesktopDDESetting) << "ReadAll called with parameters:";
    qCDebug(XdgDesktopDDESetting) << "    groups: " << groups;

    VariantMapMap result;

    if (groupMatches(QStringLiteral("org.freedesktop.appearance"), groups)) {
        QVariantMap appearanceSettings;
        appearanceSettings.insert(QStringLiteral("color-scheme"), readFdoColorScheme().variant());
        appearanceSettings.insert(QStringLiteral("accent-color"), readAccentColor().variant());

        result.insert(QStringLiteral("org.freedesktop.appearance"), appearanceSettings);
    }
    return result;
}

QDBusVariant SettingsPortal::readFdoColorScheme() const
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

QDBusVariant SettingsPortal::readAccentColor() const
{
    const QColor accentColor = qGuiApp->palette().highlight().color();
    return QDBusVariant(
            AccentColorArray{ accentColor.redF(), accentColor.greenF(), accentColor.blueF() });
}
