// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screencastsession.h"

ScreenCastSession::ScreenCastSession(const QString &appId,
                                     const QString &path,
                                     const QString &iconName,
                                     QObject *parent)
    : Session(appId, path, parent)
{
}

ScreenCastSession::~ScreenCastSession()
{
}

bool ScreenCastSession::multipleSources() const
{
    return m_multipleSources;
}

PortalCommon::SourceType ScreenCastSession::types() const
{
    return m_types;
}

void ScreenCastSession::setPersistMode(PortalCommon::PersistMode persistMode)
{
    m_persistMode = persistMode;
}

void ScreenCastSession::setStreams(const Streams &streams)
{
    Q_ASSERT(!streams.isEmpty());

    m_streams = streams;
}

PortalCommon::CursorModes ScreenCastSession::cursorMode() const
{
    return m_cursorMode;
}

void ScreenCastSession::setOptions(const QVariantMap &options)
{
    m_multipleSources = options.value(QStringLiteral("multiple")).toBool();
    m_cursorMode = PortalCommon::CursorModes(options.value(QStringLiteral("cursor_mode")).toUInt());
    m_types = PortalCommon::SourceType(options.value(QStringLiteral("types")).toUInt());

    if (m_types == 0) {
        m_types = PortalCommon::Monitor;
    }
}
