// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "remotedesktopsession.h"
#include "screencastportal.h"

RemoteDesktopSession::RemoteDesktopSession(const QString &appId, const QString &path, QObject *parent)
    :ScreenCastSession(appId, path, "", parent)
    , m_screenSharingEnabled(false)
    , m_clipboardEnabled(false)
    , m_deviceTypes(PortalCommon::None)
    , m_virtualInput(std::make_unique<VirtualInput>())
{
}

RemoteDesktopSession::~RemoteDesktopSession()
{
}

void RemoteDesktopSession::setOpations(const QVariantMap &options)
{
}

PortalCommon::DeviceTypes RemoteDesktopSession::deviceTypes() const
{
    return m_deviceTypes;
}

void RemoteDesktopSession::setDeviceTypes(PortalCommon::DeviceTypes deviceTypes)
{
    if (m_deviceTypes == deviceTypes){
        return;
    }

    m_deviceTypes = deviceTypes;
    Q_EMIT deviceTypesChanged();
}

bool RemoteDesktopSession::screenSharingEnabled() const
{
    return m_screenSharingEnabled;
}

void RemoteDesktopSession::setScreenSharingEnabled(bool enabled)
{
    if (m_screenSharingEnabled == enabled) {
        return;
    }

    m_screenSharingEnabled = enabled;
    Q_EMIT screenSharingEnabledChanged();
}

bool RemoteDesktopSession::clipboardEnabled() const
{
    return m_clipboardEnabled;;
}

void RemoteDesktopSession::setClipboardEnabled(bool enabled)
{
    if (m_clipboardEnabled == enabled) {
        return;
    }

    m_clipboardEnabled = enabled;
    Q_EMIT clipboardEnabledChanged();
}

void RemoteDesktopSession::acquireStreamingInput()
{
    if (m_acquired)
        return;
    m_acquired = m_deviceTypes == PortalCommon::None
        || m_virtualInput->initialize(m_deviceTypes);
}

void RemoteDesktopSession::refreshDescription()
{
}

void RemoteDesktopSession::setEisCookie(int cookie)
{
    m_cookie = cookie;
}

int RemoteDesktopSession::eisCookie() const
{
    return m_cookie;
}

Session::SessionType RemoteDesktopSession::type() const
{
    return SessionType::RemoteDesktop;
}
