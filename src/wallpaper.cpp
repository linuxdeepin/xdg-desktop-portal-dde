// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wallpaper.h"

#include <QLoggingCategory>
#include <QDBusMessage>
#include <qdbusconnection.h>
#include <qloggingcategory.h>
#include <QDBusPendingReply>

Q_LOGGING_CATEGORY(XdgDesktopDDEWallpaper, "xdg-dde-wallpaper")

WallPaperPortal::WallPaperPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qCDebug(XdgDesktopDDEWallpaper) << "WallPaper init";
}

uint WallPaperPortal::SetWallpaperURI(const QDBusObjectPath &handle,
                                      const QString &app_id,
                                      const QString &parent_window,
                                      const QString &uri,
                                      const QVariantMap &options)
{
    // TODO: 未处理options，仅实现设置主屏壁纸
    // options.value("show-preview").toBool(); // whether to show a preview of the picture. Note that the portal may decide to show a preview even if this option is not set
    // options.value("set-on").toString(); // where to set the wallpaper. Possible values are 'background', 'lockscreen' or 'both'

    qCDebug(XdgDesktopDDEWallpaper) << "Start set wallpaper";
    QDBusMessage primaryMsg = QDBusMessage::createMethodCall(QStringLiteral("org.deepin.dde.Display1"),
                                                             QStringLiteral("/org/deepin/dde/Display1"),
                                                             QStringLiteral("org.freedesktop.DBus.Properties"),
                                                             QStringLiteral("Get"));
    primaryMsg.setArguments({QVariant::fromValue(QStringLiteral("org.deepin.dde.Display1")), QVariant::fromValue(QStringLiteral("Primary"))});
    QDBusPendingReply<QDBusVariant> primaryReply = QDBusConnection::sessionBus().call(primaryMsg);
    if (primaryReply.isError()) {
        qCDebug(XdgDesktopDDEWallpaper) << "setting failed";
        return 1;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.deepin.dde.Appearance1"),
                                                      QStringLiteral("/org/deepin/dde/Appearance1"),
                                                      QStringLiteral("org.deepin.dde.Appearance1"),
                                                      QStringLiteral("SetMonitorBackground"));
    msg.setArguments({QVariant::fromValue(primaryReply.value().variant().toString()), QVariant::fromValue(uri)});
    QDBusPendingReply<> pcall = QDBusConnection::sessionBus().call(msg);
    if (pcall.isValid()) {
        qCDebug(XdgDesktopDDEWallpaper) << "setting succeed";
        return 0;
    }
    qCDebug(XdgDesktopDDEWallpaper) << "setting failed";

    return 1;
}
