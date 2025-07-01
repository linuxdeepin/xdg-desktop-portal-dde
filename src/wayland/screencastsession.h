// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "session.h"
#include "portalcommon.h"
#include "treelandintegration.h"

class ScreenCastSession : public Session
{
    Q_OBJECT
public:
    explicit ScreenCastSession(const QString &appId,
                               const QString &path,
                               const QString &iconName,
                               QObject *parent = nullptr);
    ~ScreenCastSession() override;

    void setOptions(const QVariantMap &options);

    PortalCommon::CursorModes cursorMode() const;
    bool multipleSources() const;
    PortalCommon::SourceType types() const;

    SessionType type() const override { return SessionType::ScreenCast; }

    void setRestoreData(const QVariant &restoreData) { m_restoreData = restoreData; }

    QVariant restoreData() const { return m_restoreData; }

    void setPersistMode(PortalCommon::PersistMode persistMode);

    PortalCommon::PersistMode persistMode() const { return m_persistMode; }

    Streams streams() const
    {
        return m_streams;
    }
    void setStreams(const Streams &streams);
    virtual void refreshDescription() { }

protected:
    void setDescription(const QString &description);

private:
    bool m_multipleSources = false;
    PortalCommon::CursorModes m_cursorMode = PortalCommon::Hidden;
    PortalCommon::SourceType m_types = PortalCommon::Any;
    PortalCommon::PersistMode m_persistMode = PortalCommon::NoPersist;
    QVariant m_restoreData;

    void streamClosed();
    Streams m_streams;
    friend class RemoteDesktopPortal;
};
