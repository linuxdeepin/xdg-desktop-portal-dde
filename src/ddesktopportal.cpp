// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ddesktopportal.h"

#include "appchooser.h"
#include "secret.h"
#include "lockdown.h"
#include "globalshortcut.h"
#include "session.h"
#include "remotedesktop.h"
#include "account.h"
#include "inhibit.h"
#include "settings.h"
#include "screenshot.h"
#include "screencast.h"
#include "background.h"
#include "filechooser.h"
#include "wallpaper.h"
#include "notification.h"
#include "waylandintegration.h"

DDesktopPortal::DDesktopPortal(QObject *parent)
    : QObject(parent)
    , m_appChooser(new AppChooserPortal(this))
    , m_fileChooser(new FileChooserPortal(this))
    , m_wallpaper(new WallPaperPortal(this))
    , m_notification(new NotificationPortal(this))
{
    const QByteArray &xdgCurrentDesktop = qgetenv("XDG_CURRENT_DESKTOP").toUpper();
    if (xdgCurrentDesktop == "DDE" || xdgCurrentDesktop == "DEEPIN") {
        m_screenshot = new ScreenshotPortal(this);
        m_screencast = new ScreenCastPortal(this);
        m_background = new BackgroundPortal(this);
        m_settings = new SettingsPortal(this);
        m_inhibit = new InhibitPortal(this);
        m_account = new AccountPortal(this);
        m_session = new SessionPortal(this);
        m_remote = new RemoteDesktopPortal(this);
        m_shortcut = new GlobalShortcutPortal(this);
        m_lockdown = new LockdownPortal(this);
        m_secret = new SecretPortal(this);
        WaylandIntegration::init();
    }
}
