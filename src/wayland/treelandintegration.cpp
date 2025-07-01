// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "treelandintegration.h"
#include "loggings.h"

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
    return m_context->linuxDmaBufInterfaceActive() &&
            m_context->screenCopyManagerActive();
}

Stream TreelandIntergration::startStreamingOutput(QScreen *screen, PortalCommon::CursorModes mode)
{
    auto stream = new PipeWireStream(m_context, screen, mode, this);
    if (!stream) {
        qCWarning(SCREENCAST) << "Cannot stream, output not found" << screen->name();
        // TODO：system notify
        return Stream{};
    }
    return startStreaming(stream,
                          {
                                  {QLatin1String("size"), screen->size()},
                                  {QLatin1String("source_type"), static_cast<uint>(PortalCommon::Monitor)}
                          });
}

Stream TreelandIntergration::startStreamingRegion(const QRect &region, PortalCommon::CursorModes mode)
{
    auto stream = new PipeWireStream(m_context, region, mode, this);
    if (stream) {
        qCWarning(SCREENCAST) << "Cannot stream for region" << region;
        return Stream{};
    }
    return startStreaming(stream,
                          {
                                  {QLatin1String("size"), region.size()},
                                  {QLatin1String("source_type"), static_cast<uint>(PortalCommon::Monitor)}
                          });
}

Stream TreelandIntergration::startStreaming(PipeWireStream *stream, const QVariantMap &streamOptions)
{
    stream->startScreencast();
    qCWarning(SCREENCAST) << "startStreaming";
    QEventLoop loop;
    Stream ret;

    connect(stream, &PipeWireStream::failed, &loop, [&](const QString &error) {
        qCWarning(SCREENCAST) << "failed to start streaming" << stream << error;
        loop.quit();
    });
    if (stream->nodeId() != SPA_ID_INVALID) {
        ret.stream = stream;
        ret.nodeId = stream->nodeId();
        ret.map = streamOptions;
        m_streams.append(ret);
        loop.quit();
        connect(stream, &PipeWireStream::closed, this, [this](uint32_t nodeid) {
            stopStreaming(nodeid);
        });
        Q_ASSERT(ret.isValid());
        loop.quit();
    } else {
        connect(stream, &PipeWireStream::ready, &loop, [&](uint32_t nodeid) {
            ret.stream = stream;
            ret.nodeId = nodeid;
            ret.map = streamOptions;
            m_streams.append(ret);

            connect(stream, &PipeWireStream::closed, this, [this, nodeid] {
                stopStreaming(nodeid);
            });
            Q_ASSERT(ret.isValid());

            loop.quit();
        });
    }
    QTimer::singleShot(3000, &loop, [&loop, stream] {
        qCWarning(SCREENCAST) << "no nodeIdChanged, failed to streaming";
        stream->deleteLater();
        loop.quit();
    });
    loop.exec();
    return ret;
}

void TreelandIntergration::stopStreaming(uint nodeId)
{
    for (auto it = m_streams.begin(), itEnd = m_streams.end(); it != itEnd; ++it) {
        if (it->nodeId == nodeId) {
            m_streams.erase(it);
            it->close();
            break;
        }
    }
}
