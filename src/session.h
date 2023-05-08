// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once


#include <QObject>
#include <QDBusVirtualObject>

#include "remotedesktop.h"
#include "screencast.h"
#include <QDBusObjectPath>
#include <QDBusAbstractAdaptor>

class SessionPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Session")
    Q_PROPERTY(uint version READ version CONSTANT)
    inline uint version() const { return 1; }

public:
    explicit SessionPortal(QObject *parent);
    ~SessionPortal() = default;

public slots:
    void Close();

signals:
    void Closed();
};

class Session : public QDBusVirtualObject
{
    Q_OBJECT
public:
    explicit Session(QObject *parent = nullptr, const QString &appId = QString(), const QString &path = QString());
    ~Session();

    enum SessionType {
        ScreenCast = 0,
        RemoteDesktop = 1
    };

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;
    QString introspect(const QString &path) const override;

    bool close();
    virtual SessionType type() const = 0;

    static Session *createSession(QObject *parent, SessionType type, const QString &appId, const QString &path);
    static Session *getSession(const QString &sessionHandle);

Q_SIGNALS:
    void closed();

private:
    const QString m_appId;
    const QString m_path;
};

class ScreenCastSession : public Session
{
    Q_OBJECT
public:
    explicit ScreenCastSession(QObject *parent = nullptr, const QString &appId = QString(), const QString &path = QString());
    ~ScreenCastSession();

    bool multipleSources() const;
    void setMultipleSources(bool multipleSources);
    ScreenCastPortal::SourceTypes  sourceTypes() const;
    void setSourceTypes(ScreenCastPortal::SourceTypes sourcetypes);
    SessionType type() const override { return SessionType::ScreenCast; }

private:
    bool m_multipleSources;
    ScreenCastPortal::SourceTypes m_sourceTypes = ScreenCastPortal::SourceTypes(ScreenCastPortal::SourceType::Any);
    // TODO type
};

class RemoteDesktopSession : public ScreenCastSession
{
    Q_OBJECT
public:
    explicit RemoteDesktopSession(QObject *parent = nullptr, const QString &appId = QString(), const QString &path = QString());
    ~RemoteDesktopSession();

    RemoteDesktopPortal::DeviceTypes deviceTypes() const;
    void setDeviceTypes(RemoteDesktopPortal::DeviceTypes deviceTypes);

    bool screenSharingEnabled() const;
    void setScreenSharingEnabled(bool enabled);

    SessionType type() const override { return SessionType::RemoteDesktop; }

private:
    bool m_screenSharingEnabled;
    RemoteDesktopPortal::DeviceTypes m_deviceTypes;
};


