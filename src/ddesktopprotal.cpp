// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ddesktopprotal.h"

DDestkopPortal::DDestkopPortal(QObject *parent)
    : QObject(parent)
    , m_filechooser(new FileChooserPortal(this))
    , m_wallpaper(new WallPaperPortal(this))
    , m_notification(new NotificationProtal(this))
{
    const QByteArray &xdgCurrentDesktop = qgetenv("XDG_CURRENT_DESKTOP");
    if (xdgCurrentDesktop.toLower() ==  "deepin") {
        m_screenshot = new ScreenshotPortal(this);
        m_screencast = new ScreenCastPortal(this);
        m_background = new BackgroundPortal(this);
        m_settings = new SettingsPortal(this);
        m_inhibit = new InhibitPortal(this);
        m_account = new AccountPortal(this);
        m_session = new SessionPortal(this);
        m_shotcut = new GlobalShotcutProtal(this);
        m_lockdown = new LockdownProtal(this);
        m_secret = new SecretPortal(this);
    }
}
