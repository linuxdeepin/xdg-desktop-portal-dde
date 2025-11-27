// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QDBusContext>
#include <QObject>

class ScreenshotPortal;
class BackgroundPortal;
class InhibitPortal;
class SettingsPortal;
class AccountPortal;
class GlobalShortcutPortal;
class LockdownPortal;
class SecretPortal;
class NotificationPortal;
class FileChooserPortal;
class AppChooserPortal;
class AccessPortal;

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
    BackgroundPortal *m_background = nullptr;
    InhibitPortal *m_inhibit = nullptr;
    SettingsPortal *m_settings = nullptr;
    AccountPortal *m_account = nullptr;
    GlobalShortcutPortal *m_shortcut = nullptr;
    LockdownPortal *m_lockdown = nullptr;
    SecretPortal *m_secret = nullptr;
    NotificationPortal *const m_notification;
    AccessPortal *const m_access;
};
