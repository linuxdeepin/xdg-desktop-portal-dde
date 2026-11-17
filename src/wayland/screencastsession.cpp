// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screencastsession.h"

#include <algorithm>
#include <utility>

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

PortalCommon::SourceTypes ScreenCastSession::types() const
{
    return m_types;
}

void ScreenCastSession::setPersistMode(PortalCommon::PersistMode persistMode)
{
    m_persistMode = persistMode;
}

bool ScreenCastSession::setStreams(const Streams &streams)
{
    if (isClosed()
        || streams.isEmpty()
        || !m_streams.isEmpty()
        || std::any_of(streams.cbegin(),
                       streams.cend(),
                       [](const Stream &stream) {
        return !stream.isValid();
    })) {
        return false;
    }

    m_streams = streams;
    for (const Stream &stream : std::as_const(m_streams)) {
        AbstractPipeWireStream *streamObject = stream.stream.data();
        Q_ASSERT(streamObject);
        if (!streamObject) {
            continue;
        }

        connect(streamObject,
                &AbstractPipeWireStream::closed,
                this,
                [this, streamObject](uint32_t) {
            streamClosed(streamObject);
        });
    }
    return true;
}

void ScreenCastSession::streamClosed(AbstractPipeWireStream *stream)
{
    const auto iterator = std::find_if(m_streams.cbegin(),
                                       m_streams.cend(),
                                       [stream](const Stream &candidate) {
        return candidate.stream.data() == stream;
    });
    if (iterator == m_streams.cend()) {
        return;
    }

    m_streams.erase(iterator);
    if (m_streams.isEmpty()) {
        close();
    }
}

PortalCommon::CursorModes ScreenCastSession::cursorMode() const
{
    return m_cursorMode;
}

void ScreenCastSession::setOptions(const QVariantMap &options)
{
    m_multipleSources = options.value(QStringLiteral("multiple")).toBool();
    m_cursorMode = PortalCommon::CursorModes(
            options.value(QStringLiteral("cursor_mode"),
                          static_cast<uint>(PortalCommon::Hidden)).toUInt());
    m_types = PortalCommon::SourceTypes::fromInt(
            options.value(QStringLiteral("types")).toUInt());

    if (!m_types) {
        m_types = PortalCommon::Monitor;
    }
}
