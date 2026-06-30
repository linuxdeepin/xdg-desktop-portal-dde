// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "abstractwaylandportal.h"
#include "portalcommon.h"

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>

class QDBusMessage;

class RemoteDesktopPortal : public AbstractWaylandPortal
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.RemoteDesktop")
    Q_PROPERTY(uint version READ version CONSTANT)
    Q_PROPERTY(uint AvailableDeviceTypes READ AvailableDeviceTypes CONSTANT)
public:
    explicit RemoteDesktopPortal(PortalWaylandContext *context);
    ~RemoteDesktopPortal() override;

    uint version() const { return 2; }
    uint AvailableDeviceTypes() const { return PortalCommon::Keyboard | PortalCommon::Pointer; };

public Q_SLOTS:
    uint CreateSession(const QDBusObjectPath &handle,
                       const QDBusObjectPath &session_handle,
                       const QString &app_id,
                       const QVariantMap &options,
                       QVariantMap &results);

    uint SelectDevices(const QDBusObjectPath &handle,
                       const QDBusObjectPath &session_handle,
                       const QString &app_id,
                       const QVariantMap &options,
                       QVariantMap &results);

    void Start(const QDBusObjectPath &handle,
               const QDBusObjectPath &session_handle,
               const QString &app_id,
               const QString &parent_window,
               const QVariantMap &options,
               const QDBusMessage &message,
               uint &replyResponse,
               QVariantMap &replyResults);

    void NotifyPointerMotion(const QDBusObjectPath &session_handle, const QVariantMap &options, double dx, double dy);
    void NotifyPointerMotionAbsolute(const QDBusObjectPath &session_handle, const QVariantMap &options, uint stream, double x, double y);
    void NotifyPointerButton(const QDBusObjectPath &session_handle, const QVariantMap &options, int button, uint state);
    void NotifyPointerAxis(const QDBusObjectPath &session_handle, const QVariantMap &options, double dx, double dy);
    void NotifyPointerAxisDiscrete(const QDBusObjectPath &session_handle, const QVariantMap &options, uint axis, int steps);

    void NotifyKeyboardKeycode(const QDBusObjectPath &session_handle, const QVariantMap &options, int keycode, uint state);
    void NotifyKeyboardKeysym(const QDBusObjectPath &session_handle, const QVariantMap &options, int keysym, uint state);

    void NotifyTouchDown(const QDBusObjectPath &session_handle, const QVariantMap &options, uint stream, uint slot, double x, double y);
    void NotifyTouchMotion(const QDBusObjectPath &session_handle, const QVariantMap &options, uint stream, uint slot, double x, double y);
    void NotifyTouchUp(const QDBusObjectPath &session_handle, const QVariantMap &options, uint slot);

    QDBusUnixFileDescriptor ConnectToEIS(const QDBusObjectPath &session_handle, const QString &app_id, const QVariantMap &options, const QDBusMessage &message);

};
