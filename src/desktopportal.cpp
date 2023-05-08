// Copyright © 2016 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "desktopportal.h"

#include <QDialog>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopPortalKdeDesktopPortal, "xdp-kde-desktop-portal")

DesktopPortal::DesktopPortal(QObject *parent)
    : QObject(parent)
    , m_access(new AccessPortal(this))
    , m_appChooser(new AppChooserPortal(this))
    , m_email(new EmailPortal(this))
    , m_fileChooser(new FileChooserPortal(this))
    , m_inhibit(new InhibitPortal(this))
    , m_notification(new NotificationPortal(this))
    , m_print(new PrintPortal(this))
#if SCREENCAST_ENABLED
    , m_screenCast(new ScreenCastPortal(this))
    , m_remoteDesktop(new RemoteDesktopPortal(this))
#endif
    , m_screenshot(new ScreenshotPortal(this))
    , m_settings(new SettingsPortal(this))
{
#if SCREENCAST_ENABLED
    WaylandIntegration::init();
#endif
}

DesktopPortal::~DesktopPortal()
{
}
