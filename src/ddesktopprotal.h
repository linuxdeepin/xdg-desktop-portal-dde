// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "secret.h"
#include "lockdown.h"
#include "globalshotcut.h"
#include "session.h"
#include "account.h"
#include "inhibit.h"
#include "settings.h"
#include "screenshot.h"
#include "screencast.h"
#include "background.h"
#include "filechooser.h"
#include "wallpaper.h"
#include "notification.h"
#include <QDBusContext>
#include <QObject>

class DDestkopPortal : public QObject, public QDBusContext
{
    Q_OBJECT

public:
    explicit DDestkopPortal(QObject *parent = nullptr);
    ~DDestkopPortal() = default;

private:
    ScreenshotPortal *m_screenshot = nullptr;
    ScreenCastPortal *m_screencast = nullptr;
    BackgroundPortal *m_background = nullptr;
    InhibitPortal *m_inhibit = nullptr;
    SettingsPortal *m_settings = nullptr;
    AccountPortal *m_account = nullptr;
    SessionPortal *m_session = nullptr;
    GlobalShotcutProtal *m_shotcut = nullptr;
    LockdownProtal *m_lockdown = nullptr;
    SecretPortal *m_secret = nullptr;
    WallPaperPortal *const m_wallpaper;
    NotificationProtal *const m_notification;
    FileChooserProtal *const m_filechooser;
};
