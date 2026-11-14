// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "treelandintegration.h"
#include "loggings.h"
#include "outputpipewirestream.h"
#include "toplevelpipewirestream.h"

#include <QRect>
#include <QTimer>
#include <QEventLoop>

#include <QDBusMetaType>

#include <algorithm>

QDebug operator<<(QDebug dbg, const Stream &c)
{
    dbg.nospace() << "Stream(" << c.map << ", " << c.nodeId << ")";
    return dbg.space();
}

const QDBusArgument &operator>>(const QDBusArgument &arg, Stream &stream)
{
    arg.beginStructure();
    arg >> stream.nodeId;

    arg.beginMap();
    while (!arg.atEnd()) {
        QString key;
        QVariant map;
        arg.beginMapEntry();
        arg >> key >> map;
        arg.endMapEntry();
        stream.map.insert(key, map);
    }
    arg.endMap();
    arg.endStructure();

    return arg;
}

const QDBusArgument &operator<<(QDBusArgument &arg, const Stream &stream)
{
    arg.beginStructure();
    arg << stream.nodeId;
    arg << stream.map;
    arg.endStructure();

    return arg;
}

TreelandIntergration::TreelandIntergration(QObject *parent)
    : QObject(parent)
    , m_context(new ScreenCastContext(this))
{
    qDBusRegisterMetaType<Stream>();
    qDBusRegisterMetaType<Streams>();
}

TreelandIntergration::~TreelandIntergration()
{
    const QList<Stream> streams = m_streams;
    m_streams.clear();
    for (const Stream &stream : streams) {
        if (stream.stream) {
            delete stream.stream.data();
        }
    }
}

void TreelandIntergration::init()
{

}

bool TreelandIntergration::isStreamingEnbled() const
{
    return !m_streams.isEmpty();
}

bool TreelandIntergration::isStreamingAvailable() const
{
    return isOutputStreamingAvailable() || isToplevelStreamingAvailable();
}

bool TreelandIntergration::isOutputStreamingAvailable() const
{
    return m_context
            && m_context->pipeWireAvailable()
            && m_context->shmInterfaceActive()
            && m_context->imageCopyCaptureManagerActive()
            && m_context->outputImageCaptureSourceManagerActive();
}

bool TreelandIntergration::isToplevelStreamingAvailable() const
{
    return m_context
            && m_context->pipeWireAvailable()
            && m_context->shmInterfaceActive()
            && m_context->imageCopyCaptureManagerActive()
            && m_context->foreignToplevelListActive()
            && m_context->foreignToplevelImageCaptureSourceManagerActive();
}

bool TreelandIntergration::isStreamActive(AbstractPipeWireStream *stream) const
{
    return stream
            && std::any_of(m_streams.cbegin(),
                           m_streams.cend(),
                           [stream](const Stream &candidate) {
        return candidate.stream.data() == stream;
    });
}

bool TreelandIntergration::isToplevelAvailable(const ToplevelInfoPtr &toplevel) const
{
    return m_context
            && toplevel
            && toplevel->handle
            && toplevel->handle->isInitialized()
            && m_context->containsToplevel(toplevel->identifier);
}

Stream TreelandIntergration::startStreamingOutput(QScreen *screen, PortalCommon::CursorModes mode)
{
    if (!screen || !isOutputStreamingAvailable()) {
        return {};
    }

    qCWarning(SCREENCAST) << "start streaming output:" << screen->name();
    auto stream = new OutputPipeWireStream(m_context, mode, screen, this);
    const QRect geometry = screen->geometry();

    return startStreaming(stream,
                          {
                                  {QLatin1String("position"), geometry.topLeft()},
                                  {QLatin1String("size"), geometry.size()},
                                  {QLatin1String("source_type"), static_cast<uint>(PortalCommon::Monitor)}
                          });
}

// Stream TreelandIntergration::startStreamingRegion(const QRect &region, PortalCommon::CursorModes mode)
// {
//     auto stream = new PipeWireStream(m_context, region, mode, this);
//     if (stream) {
//         qCWarning(SCREENCAST) << "Cannot stream for region" << region;
//         return Stream{};
//     }
//     return startStreaming(stream,
//                           {
//                                   {QLatin1String("size"), region.size()},
//                                   {QLatin1String("source_type"), static_cast<uint>(PortalCommon::Monitor)}
//                           });
// }

Stream TreelandIntergration::startStreamingToplevel(const ToplevelInfoPtr &toplevel,
                                                    PortalCommon::CursorModes mode)
{
    if (!toplevel || !isToplevelStreamingAvailable()) {
        return {};
    }

    qCDebug(SCREENCAST) << "start streaming toplevel:" << toplevel->appID;
    auto stream = new ToplevelPipeWireStream(m_context, mode, toplevel, this);

    return startStreaming(stream,
                          {
                                  {QLatin1String("source_type"), static_cast<uint>(PortalCommon::Window)}
                          });
}

Stream TreelandIntergration::startStreaming(AbstractPipeWireStream *stream, const QVariantMap &streamOptions)
{
    if (!stream) {
        return {};
    }

    qCInfo(SCREENCAST) << "xdpw: starting stream" << stream;
    QEventLoop loop;
    Stream ret;
    QString failureReason;
    QPointer<AbstractPipeWireStream> guardedStream(stream);
    QTimer timeout;
    timeout.setSingleShot(true);

    connect(stream, &AbstractPipeWireStream::failed, &loop, [&](const QString &error) {
        failureReason = error;
        ret = {};
        loop.quit();
    });
    connect(stream, &AbstractPipeWireStream::ready, &loop, [&](uint32_t nodeId) {
        ret.stream = stream;
        ret.nodeId = nodeId;
        ret.map = streamOptions;
        loop.quit();
    });
    connect(&timeout, &QTimer::timeout, &loop, [&] {
        failureReason = QStringLiteral("Timed out while waiting for the PipeWire node");
        loop.quit();
    });

    if (stream->startScreencast() != 0) {
        failureReason = QStringLiteral("Failed to create the image-copy capture source");
    } else if (!failureReason.isEmpty()) {
        // A synchronous initialization callback already reported the error.
    } else if (stream->nodeId() != SPA_ID_INVALID) {
        ret.stream = stream;
        ret.nodeId = stream->nodeId();
        ret.map = streamOptions;
    } else {
        timeout.start(3000);
        loop.exec();
    }

    timeout.stop();
    if (!ret.isValid()) {
        qCWarning(SCREENCAST) << "xdpw: failed to start stream" << stream
                              << failureReason;
        if (guardedStream) {
            guardedStream->deleteLater();
        }
        return {};
    }

    m_streams.append(ret);
    const QPointer<AbstractPipeWireStream> activeStream(stream);
    connect(stream, &AbstractPipeWireStream::closed, this, [this, activeStream](uint32_t) {
        if (activeStream) {
            stopStreaming(activeStream.data());
        }
    });
    connect(stream, &AbstractPipeWireStream::failed, this,
            [this, nodeId = ret.nodeId](const QString &error) {
        qCWarning(SCREENCAST) << "xdpw: active stream failed" << nodeId << error;
    });
    connect(stream, &QObject::destroyed, this, [this, nodeId = ret.nodeId] {
        const auto iterator = std::find_if(m_streams.begin(),
                                           m_streams.end(),
                                           [nodeId](const Stream &candidate) {
                                               return candidate.nodeId == nodeId
                                                       && candidate.stream.isNull();
                                           });
        if (iterator != m_streams.end()) {
            m_streams.erase(iterator);
        }
    });

    qCInfo(SCREENCAST) << "xdpw: stream ready" << ret.nodeId;
    return ret;
}

void TreelandIntergration::stopStreaming(AbstractPipeWireStream *streamObject)
{
    if (!streamObject) {
        return;
    }

    const auto iterator = std::find_if(m_streams.begin(),
                                       m_streams.end(),
                                       [streamObject](const Stream &candidate) {
        return candidate.stream.data() == streamObject;
    });
    if (iterator == m_streams.end()) {
        return;
    }

    const Stream stream = *iterator;
    m_streams.erase(iterator);
    stream.close();
}
