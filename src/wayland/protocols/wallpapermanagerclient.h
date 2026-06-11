// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-wallpaper-manager-unstable-v1.h"

#include <QtWaylandClient/QWaylandClientExtension>
#include <QScreen>

class TreelandWallpaperV1;
class TreelandWallpaperManagerV1
    : public QWaylandClientExtensionTemplate<TreelandWallpaperManagerV1>
    , public QtWayland::treeland_wallpaper_manager_v1
{
    Q_OBJECT
public:
    static constexpr int InterfaceVersion = 1;

    explicit TreelandWallpaperManagerV1();
    ~TreelandWallpaperManagerV1() override;

    TreelandWallpaperV1 *createWallpaper(QScreen *screen);
};

class TreelandWallpaperV1 : public QObject
    , public QtWayland::treeland_wallpaper_v1
{
    Q_OBJECT
public:
    enum class Error : uint32_t {
        AlreadyUsed = error_already_used,
        InvalidSource = error_invalid_source,
        PermissionDenied = error_permission_denied,
    };
    Q_ENUM(Error)

    enum class SourceType : uint32_t {
        Image = wallpaper_source_type_image,
        Video = wallpaper_source_type_video,
    };
    Q_ENUM(SourceType)

    enum WallpaperRole : uint32_t {
        Desktop = wallpaper_role_desktop,
        Lockscreen = wallpaper_role_lockscreen,
    };
    Q_DECLARE_FLAGS(WallpaperRoles, WallpaperRole)
    Q_FLAG(WallpaperRoles)

    explicit TreelandWallpaperV1(struct ::treeland_wallpaper_v1 *object);
    ~TreelandWallpaperV1() override;

    void setImageSource(const QString &filePath, WallpaperRoles roles);
    void setVideoSource(const QString &filePath, WallpaperRoles roles);
    void setSource(const QString &filePath, WallpaperRoles roles);

Q_SIGNALS:
    void failed(const QString &source, Error error);
    void changed(uint32_t role, SourceType sourceType, const QString &fileSource);

protected:
    void treeland_wallpaper_v1_failed(const QString &source, uint32_t error) override;
    void treeland_wallpaper_v1_changed(uint32_t role,
                                       uint32_t sourceType,
                                       const QString &fileSource) override;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TreelandWallpaperV1::WallpaperRoles)
