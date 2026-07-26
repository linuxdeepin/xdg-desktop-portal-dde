// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wayland/abstractpipewirestream.h"
#include "wayland/screencastsession.h"

#include <QCoreApplication>

#include <iostream>

namespace {

class FakePipeWireStream final : public AbstractPipeWireStream
{
public:
    FakePipeWireStream()
        : AbstractPipeWireStream({}, PortalCommon::Hidden)
    {
    }

    int startScreencast() override
    {
        return 0;
    }

    void closeFromBackend(uint32_t nodeId)
    {
        Q_EMIT closed(nodeId);
    }
};

Stream makeStream(FakePipeWireStream *stream, uint32_t nodeId)
{
    Stream result;
    result.stream = stream;
    result.nodeId = nodeId;
    return result;
}

bool expect(bool condition, const char *message)
{
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    FakePipeWireStream first;
    FakePipeWireStream second;
    auto *session = new ScreenCastSession(
            QStringLiteral("test.application"),
            QStringLiteral("/org/freedesktop/portal/desktop/session/test/lifecycle"),
            QString());

    int closedCount = 0;
    QObject::connect(session, &Session::closed, [&closedCount] {
        ++closedCount;
    });

    if (!expect(session->types() == PortalCommon::Monitor,
                "The default source type is not MONITOR")
        || !expect(session->cursorMode() == PortalCommon::Hidden,
                   "The default cursor mode is not Hidden")
        || !expect(!session->multipleSources(),
                   "Multiple source selection is enabled by default")) {
        delete session;
        return 1;
    }

    session->setOptions({
            {QStringLiteral("types"),
             static_cast<uint>(PortalCommon::Monitor | PortalCommon::Window)},
            {QStringLiteral("cursor_mode"),
             static_cast<uint>(PortalCommon::Embedded)},
            {QStringLiteral("multiple"), true},
    });
    if (!expect(session->types()
                        == (PortalCommon::Monitor | PortalCommon::Window),
                "Combined source types were not retained")
        || !expect(session->cursorMode() == PortalCommon::Embedded,
                   "The requested embedded cursor mode was not retained")
        || !expect(session->multipleSources(),
                   "The requested multiple-source mode was not retained")
        || !expect(!session->setStreams(
                           {makeStream(&first, SPA_ID_INVALID)}),
                   "The session accepted a stream without a PipeWire node")) {
        delete session;
        return 1;
    }

    if (!expect(session->setStreams(
                        {makeStream(&first, 10), makeStream(&second, 11)}),
                "The session rejected its initial valid streams")) {
        delete session;
        return 1;
    }

    first.closeFromBackend(10);
    if (!expect(closedCount == 0,
                "The session closed while another stream was still active")
        || !expect(session->streams().size() == 1,
                   "The terminated stream was not removed from the session")) {
        delete session;
        return 1;
    }

    first.closeFromBackend(10);
    if (!expect(closedCount == 0,
                "A duplicate stream-closed signal closed the session")
        || !expect(session->streams().size() == 1,
                   "A duplicate stream-closed signal removed another stream")) {
        delete session;
        return 1;
    }

    second.closeFromBackend(11);
    if (!expect(closedCount == 1,
                "The session did not close after its final stream ended")
        || !expect(session->streams().isEmpty(),
                   "The final terminated stream remained in the session")
        || !expect(session->isClosed(),
                   "The session did not record its closed state")) {
        delete session;
        return 1;
    }

    FakePipeWireStream lateStream;
    if (!expect(!session->setStreams({makeStream(&lateStream, 12)}),
                "A logically closed session accepted a late stream")
        || !expect(session->streams().isEmpty(),
                   "A late stream was retained by a closed session")) {
        delete session;
        return 1;
    }

    session->close();
    if (!expect(closedCount == 1,
                "Closing an already closed session emitted Closed again")) {
        delete session;
        return 1;
    }

    session->setPersistMode(PortalCommon::NoPersist);
    session->setRestoreData(QVariantMap{
            {QStringLiteral("restore"), QStringLiteral("once")}
    });
    if (!expect(session->restoreRequested(),
                "Restore data was incorrectly gated by the no-persist mode")) {
        delete session;
        return 1;
    }

    delete session;
    return 0;
}
