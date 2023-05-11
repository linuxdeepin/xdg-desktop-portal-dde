// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QDBusContext>
#include <QObject>

class ScreenshotPortal;
class ScreenCastPortal;
class BackgroundPortal;
class InhibitPortal;
class SettingsPortal;
class AccountPortal;
class SessionPortal;
class RemoteDesktopPortal;
class GlobalShortcutPortal;
class LockdownPortal;
class SecretPortal;
class WallPaperPortal;
class NotificationPortal;
class FileChooserPortal;
class AppChooserPortal;

class DDesktopPortal : public QObject, public QDBusContext
{
    Q_OBJECT

public:
    explicit DDesktopPortal(QObject *parent = nullptr);
    ~DDesktopPortal() = default;

private:
    AppChooserPortal *const m_appChooser;
    FileChooserPortal *const m_fileChooser;
    ScreenshotPortal *m_screenshot = nullptr;
    ScreenCastPortal *m_screencast = nullptr;
    BackgroundPortal *m_background = nullptr;
    InhibitPortal *m_inhibit = nullptr;
    SettingsPortal *m_settings = nullptr;
    AccountPortal *m_account = nullptr;
    SessionPortal *m_session = nullptr;
    RemoteDesktopPortal *m_remote = nullptr;
    GlobalShortcutPortal *m_shortcut = nullptr;
    LockdownPortal *m_lockdown = nullptr;
    SecretPortal *m_secret = nullptr;
    WallPaperPortal *const m_wallpaper;
    NotificationPortal *const m_notification;
};
