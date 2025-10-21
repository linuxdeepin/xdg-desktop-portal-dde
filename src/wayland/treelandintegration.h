// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "portalcommon.h"
#include "abstractpipewirestream.h"
#include "toplevelmodel.h"

#include <QDBusArgument>

struct Stream {
    AbstractPipeWireStream *stream = nullptr;
    uint nodeId;
    QVariantMap map;
    bool isValid() const { return stream; }
    void close() { stream->deleteLater(); }
};

typedef QList<Stream> Streams;

QDebug operator<<(QDebug dbg, const Stream &c);

const QDBusArgument &operator<<(QDBusArgument &arg, const Stream &stream);
const QDBusArgument &operator>>(const QDBusArgument &arg, Stream &stream);

class ScreencastPortalWayland;

class TreelandIntergration : public QObject
{
    Q_OBJECT
public:
    explicit TreelandIntergration(QObject *parent = nullptr);
    ~TreelandIntergration() override;

    void init();
    bool isStreamingEnbled() const;
    bool isStreamingAvailable() const;

    Stream startStreamingOutput(QScreen *screen, PortalCommon::CursorModes mode);
    // Stream startStreamingRegion(const QRect &region, PortalCommon::CursorModes mode);
    Stream startStreamingToplevel(ToplevelInfo *toplevel, PortalCommon::CursorModes mode);

    Stream startStreaming(AbstractPipeWireStream *stream, const QVariantMap &streamOptions);
    void stopStreaming(uint nodeId);

private:
    friend class ScreencastPortalWayland;
    QList<Stream> m_streams;
    QPointer<ScreenCastContext> m_context;
};

Q_DECLARE_METATYPE(Stream)
Q_DECLARE_METATYPE(Streams)
