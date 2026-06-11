// SPDX-FileCopyrightText: 2021-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wallpaper.h"
#include "wayland/loggings.h"
#include "wayland/protocols/wallpapermanagerclient.h"
#include "wayland/dbushelpers.h"

#include <QDBusMessage>
#include <QGuiApplication>
#include <QScreen>
#include <QLoggingCategory>

WallPaperPortal::WallPaperPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
    , m_wallpaperManager(new TreelandWallpaperManagerV1())
{
}

WallPaperPortal::~WallPaperPortal()
{
    delete m_wallpaperManager;
}

void WallPaperPortal::SetWallpaperURI(const QDBusObjectPath &handle,
                                      const QString &app_id,
                                      const QString &parent_window,
                                      const QString &uri,
                                      const QVariantMap &options,
                                      const QDBusMessage &message,
                                      uint &replyResponse)
{
    Q_UNUSED(handle)
    Q_UNUSED(parent_window)

    qCDebug(WALLPAPER) << "SetWallpaperURI called with parameters:";
    qCDebug(WALLPAPER) << "    app_id:" << app_id;
    qCDebug(WALLPAPER) << "    uri:" << uri;
    qCDebug(WALLPAPER) << "    options:" << options;

    Location location;
    const QString setOn = options.value(QStringLiteral("set-on")).toString();
    if (setOn == QStringLiteral("background")) {
        location = Desktop;
    } else if (setOn == QStringLiteral("lockscreen")) {
        location = Lockscreen;
    } else if (setOn == QStringLiteral("both")) {
        location = Both;
    } else {
        qCWarning(WALLPAPER) << "Unknown location for wallpaper:" << setOn;
        replyResponse = PortalResponse::OtherError;
        return;
    }

    const QUrl url(uri);
    if (!url.isValid()) {
        qCWarning(WALLPAPER) << "Url is not valid:" << uri;
        replyResponse = PortalResponse::OtherError;
        return;
    }

    setWallpaper(url, location);
    replyResponse = PortalResponse::Success;
}

void WallPaperPortal::setWallpaper(const QUrl &url, WallPaperPortal::Location location)
{
    if (!m_wallpaperManager || !m_wallpaperManager->isActive()) {
        qCWarning(WALLPAPER) << "Wallpaper manager wayland extension is not available";
        return;
    }

    TreelandWallpaperV1::WallpaperRoles roles;
    switch (location) {
    case WallPaperPortal::Desktop:
        roles = TreelandWallpaperV1::Desktop;
        break;
    case WallPaperPortal::Lockscreen:
        roles = TreelandWallpaperV1::Lockscreen;
        break;
    case WallPaperPortal::Both:
        roles = TreelandWallpaperV1::Desktop | TreelandWallpaperV1::Lockscreen;
        break;
    }

    const QString filePath = url.toLocalFile();

    for (QScreen *screen : qGuiApp->screens()) {
        auto *wallpaper = m_wallpaperManager->createWallpaper(screen);
        if (!wallpaper) {
            continue;
        }
        wallpaper->setSource(filePath, roles);
    }
}
