// Copyright © 2016 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_DESKTOP_PORTAL_H
#define XDG_DESKTOP_PORTAL_KDE_DESKTOP_PORTAL_H

#include <QObject>
#include <QDBusVirtualObject>
#include <QDBusContext>

#include "access.h"
#include "appchooser.h"
#include "email.h"
#include "filechooser.h"
#include "inhibit.h"
#include "notification.h"
#include "print.h"
#if SCREENCAST_ENABLED
#include "screencast.h"
#include "remotedesktop.h"
#include "waylandintegration.h"
#endif
#include "screenshot.h"
#include "settings.h"

class DesktopPortal : public QObject, public QDBusContext
{
    Q_OBJECT
public:
    explicit DesktopPortal(QObject *parent = nullptr);
    ~DesktopPortal();

private:
    AccessPortal *m_access;
    AppChooserPortal *m_appChooser;
    EmailPortal *m_email;
    FileChooserPortal *m_fileChooser;
    InhibitPortal *m_inhibit;
    NotificationPortal *m_notification;
    PrintPortal *m_print;
#if SCREENCAST_ENABLED
    ScreenCastPortal *m_screenCast;
    RemoteDesktopPortal *m_remoteDesktop;
#endif
    ScreenshotPortal *m_screenshot;
    SettingsPortal *m_settings;
};

#endif // XDG_DESKTOP_PORTAL_KDE_DESKTOP_PORTAL_H

