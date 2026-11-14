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
    for (auto it = m_streams.begin(), itEnd = m_streams.end(); it != itEnd; ++it) {
        it->close();
    }
    m_streams.clear();
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
    if (!m_context) {
        return false;
    }

    const bool available = m_context->outputImageCaptureSourceManagerActive()
        && m_context->imageCopyCaptureManagerActive()
        && (m_context->linuxDmaBufInterfaceActive() || m_context->shmInterfaceActive());
    if (!available) {
        qCWarning(SCREENCAST) << "Output capture capabilities:"
                               << "linuxDmaBuf=" << m_context->linuxDmaBufInterfaceActive()
                               << "shm=" << m_context->shmInterfaceActive()
                               << "outputImageCaptureSource=" << m_context->outputImageCaptureSourceManagerActive()
                               << "imageCopyCapture=" << m_context->imageCopyCaptureManagerActive();
    }
    return available;
}

bool TreelandIntergration::isToplevelStreamingAvailable() const
{
    if (!m_context) {
        return false;
    }

    const bool available = m_context->foreignToplevelImageCaptureSourceManagerActive()
        && m_context->imageCopyCaptureManagerActive()
        && (m_context->linuxDmaBufInterfaceActive() || m_context->shmInterfaceActive());
    if (!available) {
        qCWarning(SCREENCAST) << "Toplevel capture capabilities:"
                               << "linuxDmaBuf=" << m_context->linuxDmaBufInterfaceActive()
                               << "shm=" << m_context->shmInterfaceActive()
                               << "foreignToplevelImageCaptureSource=" << m_context->foreignToplevelImageCaptureSourceManagerActive()
                               << "imageCopyCapture=" << m_context->imageCopyCaptureManagerActive();
    }
    return available;
}

Stream TreelandIntergration::startStreamingOutput(QScreen *screen, PortalCommon::CursorModes mode)
{
    qCWarning(SCREENCAST) << "start streaming output:" << screen->name();
    QPointer<QScreen> guardedScreen(screen);
    auto stream = new OutputPipeWireStream(m_context, mode, screen, this);

    Stream result = startStreaming(stream,
                                   {{QStringLiteral("size"), screen->size()},
                                    {QStringLiteral("position"), screen->geometry().topLeft()},
                                    {QStringLiteral("source_type"), static_cast<uint>(PortalCommon::Monitor)}});
    result.output = guardedScreen;
    return result;
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

Stream TreelandIntergration::startStreamingToplevel(ToplevelInfo *toplevel, PortalCommon::CursorModes mode)
{
    qCDebug(SCREENCAST) << "start streaming toplevel:" << toplevel->appID;
    auto stream = new ToplevelPipeWireStream(m_context, mode, toplevel, this);

    return startStreaming(stream,
                          {
                                  {QLatin1String("source_type"), static_cast<uint>(PortalCommon::Window)}
                          });
}

QList<ToplevelInfo *> TreelandIntergration::toplevels() const
{
    return m_context ? m_context->toplevels() : QList<ToplevelInfo *>{};
}

Stream TreelandIntergration::startStreaming(AbstractPipeWireStream *stream, const QVariantMap &streamOptions)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QPointer<AbstractPipeWireStream> guardedStream(stream);
    bool failed = false;
    connect(stream, &AbstractPipeWireStream::failed, &loop, [&](const QString &error) {
        qCWarning(SCREENCAST) << "Failed to start streaming:" << error;
        failed = true;
        loop.quit();
    });
    connect(stream, &AbstractPipeWireStream::closed, &loop, [&] {
        failed = true;
        loop.quit();
    });
    connect(stream, &QObject::destroyed, &loop, [&] {
        failed = true;
        loop.quit();
    });
    connect(stream, &AbstractPipeWireStream::ready, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, [&] {
        qCWarning(SCREENCAST) << "Timed out waiting for a PipeWire node";
        failed = true;
        loop.quit();
    });
    timeout.start(3000);
    if (stream->startScreencast() < 0)
        failed = true;
    // ready/failed may have been emitted synchronously in startScreencast.
    if (!failed && guardedStream && stream->nodeId() == SPA_ID_INVALID)
        loop.exec();
    if (failed || !guardedStream || stream->nodeId() == SPA_ID_INVALID) {
        if (guardedStream)
            guardedStream->deleteLater();
        return {};
    }
    Stream result;
    result.stream = stream;
    result.nodeId = stream->nodeId();
    result.map = streamOptions;
    m_streams.append(result);
    connect(stream, &AbstractPipeWireStream::closed, this, [this, nodeId = result.nodeId] {
        stopStreaming(nodeId);
    });
    return result;
}

void TreelandIntergration::stopStreaming(uint nodeId)
{
    for (auto it = m_streams.begin(), itEnd = m_streams.end(); it != itEnd; ++it) {
        if (it->nodeId == nodeId) {
            const Stream stream = *it;
            m_streams.erase(it);
            stream.close();
            break;
        }
    }
}
