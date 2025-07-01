// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "screencastsession.h"
#include "portalcommon.h"

#include <QVariantMap>

class RemoteDesktopSession : public ScreenCastSession
{
    Q_OBJECT
    Q_PROPERTY(PortalCommon::DeviceTypes deviceTypes READ deviceTypes WRITE setDeviceTypes NOTIFY deviceTypesChanged FINAL)
    Q_PROPERTY(bool screenSharingEnabled READ screenSharingEnabled WRITE setScreenSharingEnabled NOTIFY screenSharingEnabledChanged FINAL)
    Q_PROPERTY(bool clipboardEnabled READ clipboardEnabled WRITE setClipboardEnabled NOTIFY clipboardEnabledChanged FINAL)
public:
    explicit RemoteDesktopSession(const QString &appId,
                                  const QString &path,
                                  QObject *parent = nullptr);
    ~RemoteDesktopSession() override;

    void setOpations(const QVariantMap &options);
    PortalCommon::DeviceTypes deviceTypes() const;
    void setDeviceTypes(PortalCommon::DeviceTypes deviceTypes);

    bool screenSharingEnabled() const;
    void setScreenSharingEnabled(bool enabled);

    bool clipboardEnabled() const;
    void setClipboardEnabled(bool enabled);

    void acquireStreamingInput();
    void refreshDescription() override;

    SessionType type() const override;

Q_SIGNALS:
    void deviceTypesChanged();
    void screenSharingEnabledChanged();
    void clipboardEnabledChanged();

private:
    bool m_screenSharingEnabled;
    bool m_clipboardEnabled;
    PortalCommon::DeviceTypes m_deviceTypes;
    bool m_acquired = false;
};
