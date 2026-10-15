// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "amhelper.h"
#include "loggings.h"

#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusMetaType>
#include <QDBusVariant>

#include <DUtil>

#include <utility>

static const QString AM_DBUS_SERVICE = "org.desktopspec.ApplicationManager1";
static const QString AM_DBUS_APPLICATION_INTERFACE = "org.desktopspec.ApplicationManager1.Application";
static const QString DESKTOP_ENTRY_ICON_KEY = "Desktop Entry";
static const QString DEFAULT_KEY = "default";
static QString locale = QLocale::system().name();
static QString DEFAULT_ICON = "application-default-icon";

namespace AMHelpers
{

void updateInfoFromAM(const QString &appID, QString &name, QString &icon)
{
    QString path = "/org/desktopspec/ApplicationManager1/" + DUtil::escapeToObjectPath(appID);
    QDBusInterface amIterface(AM_DBUS_SERVICE, path, AM_DBUS_APPLICATION_INTERFACE);
    if (!amIterface.isValid()) {
        qCCritical(PORTAL_COMMON) << "invalid interface:" << amIterface.lastError().message();
        return;
    }

    auto nameProperty = amIterface.property("Name");
    if (!nameProperty.isValid()) {
        qCCritical(PORTAL_COMMON) << "failed to get Name property:" << amIterface.lastError().message();
    } else {
        name = getLocaleOrDefaultValue(qdbus_cast<QStringMap>(nameProperty), locale, DEFAULT_KEY);
    }

    auto iconsProperty = amIterface.property("Icons");
    if (!iconsProperty.isValid()) {
        qCCritical(PORTAL_COMMON) << "failed to get Icons property:" << amIterface.lastError().message();
        icon = DEFAULT_ICON;
    } else {
        icon = getLocaleOrDefaultValue(qdbus_cast<QStringMap>(iconsProperty), DESKTOP_ENTRY_ICON_KEY, DEFAULT_ICON);
    }
}

QString getLocaleOrDefaultValue(const QStringMap &value, const QString &targetKey, const QString &fallbackKey)
{
    auto targetValue = value.value(targetKey);
    auto fallbackValue = value.value(fallbackKey);
    return !targetValue.isEmpty() ? targetValue : fallbackValue;
}

QString nameFromAM(const QString &appID)
{
    QString name = appID;
    QString path = "/org/desktopspec/ApplicationManager1/" + DUtil::escapeToObjectPath(appID);
    QDBusInterface amIterface(AM_DBUS_SERVICE, path, AM_DBUS_APPLICATION_INTERFACE);
    if (!amIterface.isValid()) {
        qCCritical(PORTAL_COMMON) << "invalid interface:" << amIterface.lastError().message();
        return name;
    }

    auto nameProperty = amIterface.property("Name");
    if (!nameProperty.isValid()) {
        qCCritical(PORTAL_COMMON) << "failed to get Name property:" << amIterface.lastError().message();
    } else {
        name = getLocaleOrDefaultValue(qdbus_cast<QStringMap>(nameProperty), locale, DEFAULT_KEY);
    }

    return name;
}

void nameFromAMAsync(const QString &appID,
                     QObject *context,
                     std::function<void(QString)> callback)
{
    if (!context || !callback) {
        return;
    }

    if (appID.isEmpty()) {
        callback(appID);
        return;
    }

    const QString path = "/org/desktopspec/ApplicationManager1/" + DUtil::escapeToObjectPath(appID);
    QDBusMessage message = QDBusMessage::createMethodCall(
            AM_DBUS_SERVICE,
            path,
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("Get"));
    message << AM_DBUS_APPLICATION_INTERFACE << QStringLiteral("Name");

    auto *watcher = new QDBusPendingCallWatcher(
            QDBusConnection::sessionBus().asyncCall(message),
            context);
    QObject::connect(watcher,
                     &QDBusPendingCallWatcher::finished,
                     context,
                     [appID, callback = std::move(callback)](QDBusPendingCallWatcher *watcher) mutable {
        QString name = appID;
        const QDBusPendingReply<QDBusVariant> reply = *watcher;
        if (reply.isError()) {
            qCDebug(PORTAL_COMMON) << "failed to get Name property asynchronously:"
                                   << reply.error().message();
        } else {
            const QString resolvedName = getLocaleOrDefaultValue(
                    qdbus_cast<QStringMap>(reply.value().variant()),
                    locale,
                    DEFAULT_KEY);
            if (!resolvedName.isEmpty()) {
                name = resolvedName;
            }
        }

        watcher->deleteLater();
        callback(std::move(name));
    });
}

}
