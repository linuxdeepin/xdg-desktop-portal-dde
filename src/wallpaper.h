// SPDX-FileCopyrightText: 2021-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QDBusMessage>
#include <QUrl>
#include <qobjectdefs.h>

class TreelandWallpaperManagerV1;

class WallPaperPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Wallpaper")

public:
    enum Location {
        Desktop,
        Lockscreen,
        Both
    };

    explicit WallPaperPortal(QObject *parent);
    ~WallPaperPortal();

public Q_SLOTS:
    void SetWallpaperURI(const QDBusObjectPath &handle,
                         const QString &app_id,
                         const QString &parent_window,
                         const QString &uri,
                         const QVariantMap &options,
                         const QDBusMessage &message,
                         uint &replyResponse);

private:
    void setWallpaper(const QUrl &url, Location location);

    TreelandWallpaperManagerV1 *m_wallpaperManager = nullptr;
};
