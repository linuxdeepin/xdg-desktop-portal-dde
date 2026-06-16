// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapermanagerclient.h"
#include "loggings.h"

#include <QDebug>
#include <QGuiApplication>
#include <QMimeDatabase>
#include <qpa/qplatformnativeinterface.h>

TreelandWallpaperManagerV1::TreelandWallpaperManagerV1()
    : QWaylandClientExtensionTemplate<TreelandWallpaperManagerV1>(InterfaceVersion)
    , QtWayland::treeland_wallpaper_manager_v1()
{
    initialize();
}

TreelandWallpaperManagerV1::~TreelandWallpaperManagerV1()
{
    destroy();
}

TreelandWallpaperV1 *TreelandWallpaperManagerV1::createWallpaper(QScreen *screen)
{
    if (!screen) {
        qCCritical(WALLPAPER) << "Cannot create wallpaper: null screen";
        return nullptr;
    }

    auto *nativeInterface = qGuiApp->platformNativeInterface();
    if (!nativeInterface) {
        qCCritical(WALLPAPER) << "Cannot create wallpaper: no QPlatformNativeInterface";
        return nullptr;
    }

    auto *wlOutput = static_cast<wl_output *>(
        nativeInterface->nativeResourceForScreen(QByteArrayLiteral("output"), screen));
    if (!wlOutput) {
        qCCritical(WALLPAPER) << "Cannot create wallpaper: failed to get wl_output for" << screen->name();
        return nullptr;
    }

    auto *object = get_treeland_wallpaper(wlOutput, nullptr);
    if (!object) {
        qCCritical(WALLPAPER) << "Cannot create wallpaper: compositor returned null for" << screen->name();
        return nullptr;
    }

    return new TreelandWallpaperV1(object);
}

TreelandWallpaperV1::TreelandWallpaperV1(struct ::treeland_wallpaper_v1 *object)
    : QObject()
    , QtWayland::treeland_wallpaper_v1(object)
{
}

TreelandWallpaperV1::~TreelandWallpaperV1()
{
    destroy();
}

void TreelandWallpaperV1::setImageSource(const QString &filePath, WallpaperRoles roles)
{
    QtWayland::treeland_wallpaper_v1::set_image_source(filePath, roles);
}

void TreelandWallpaperV1::setVideoSource(const QString &filePath, WallpaperRoles roles)
{
    QtWayland::treeland_wallpaper_v1::set_video_source(filePath, roles);
}

void TreelandWallpaperV1::setSource(const QString &filePath, WallpaperRoles roles)
{
    static QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(filePath, QMimeDatabase::MatchContent);
    const bool isImage = mime.isValid() && mime.name().startsWith("image/");
    const bool isVideo = mime.isValid() && mime.name().startsWith("video/");

    if (isImage) {
        setImageSource(filePath, roles);
    } else if (isVideo) {
        setVideoSource(filePath, roles);
    } else {
        qCCritical(WALLPAPER) << "Unsupported wallpaper file type:" << filePath;
    }
}

void TreelandWallpaperV1::treeland_wallpaper_v1_failed(const QString &source, uint32_t error)
{
    Q_EMIT failed(source, static_cast<Error>(error));
}

void TreelandWallpaperV1::treeland_wallpaper_v1_changed(uint32_t role,
                                                        uint32_t sourceType,
                                                        const QString &fileSource)
{
    Q_EMIT changed(role, static_cast<SourceType>(sourceType), fileSource);
}
