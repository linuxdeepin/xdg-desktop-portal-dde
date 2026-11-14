// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "portalcommon.h"
#include "abstractpipewirestream.h"
#include "toplevelmodel.h"

#include <QDBusArgument>

struct Stream {
    QPointer<AbstractPipeWireStream> stream;
    uint nodeId = SPA_ID_INVALID;
    QVariantMap map;
    bool isValid() const { return !stream.isNull() && nodeId != SPA_ID_INVALID; }
    void close() const
    {
        if (stream) {
            stream->deleteLater();
        }
    }
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
    bool isOutputStreamingAvailable() const;
    bool isToplevelStreamingAvailable() const;
    bool isStreamActive(AbstractPipeWireStream *stream) const;
    bool isToplevelAvailable(const ToplevelInfoPtr &toplevel) const;

    Stream startStreamingOutput(QScreen *screen, PortalCommon::CursorModes mode);
    // Stream startStreamingRegion(const QRect &region, PortalCommon::CursorModes mode);
    Stream startStreamingToplevel(const ToplevelInfoPtr &toplevel,
                                  PortalCommon::CursorModes mode);

    Stream startStreaming(AbstractPipeWireStream *stream, const QVariantMap &streamOptions);
    void stopStreaming(AbstractPipeWireStream *stream);

private:
    friend class ScreencastPortalWayland;
    QList<Stream> m_streams;
    QPointer<ScreenCastContext> m_context;
};

Q_DECLARE_METATYPE(Stream)
Q_DECLARE_METATYPE(Streams)
