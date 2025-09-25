// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screencastportal.h"
#include "loggings.h"
#include "remotedesktopsession.h"
#include "screencastchooser.h"
#include "request2.h"
#include "utils.h"
#include "restoredata.h"
#include "screenlistmodel.h"

ScreencastPortalWayland::ScreencastPortalWayland(PortalWaylandContext *context)
    : AbstractWaylandPortal(context)
{
    globalIntergration->init();
}

ScreencastPortalWayland::~ScreencastPortalWayland()
{

}

uint ScreencastPortalWayland::AvailableSourceTypes() const
{
    return PortalCommon::Monitor | PortalCommon::Window;
}

uint ScreencastPortalWayland::AvailableCursorModes() const
{
    return PortalCommon::Hidden |
            PortalCommon::Embedded |
            PortalCommon::Metadata;
}

uint ScreencastPortalWayland::CreateSession(const QDBusObjectPath &handle,
                                            const QDBusObjectPath &session_handle,
                                            const QString &app_id,
                                            const QVariantMap &options,
                                            QVariantMap &results)
{
    qCDebug(SCREENCAST) << "CreateSession called with parameters:";
    qCDebug(SCREENCAST) << "    handle: " << handle.path();
    qCDebug(SCREENCAST) << "    session_handle: " << session_handle.path();
    qCDebug(SCREENCAST) << "    app_id: " << app_id;
    qCDebug(SCREENCAST) << "    options: " << options;

    Session *session = Session::createSession(this, Session::ScreenCast, app_id, session_handle.path());

    if (!session) {
        return PortalResponse::OtherError;
    }

    if (!globalIntergration->isStreamingAvailable()) {
        qCWarning(SCREENCAST) << "wlf-screencopy-unstable-v1 does not seem to be available";
        return PortalResponse::OtherError;
    }

    connect(session, &Session::closed, [session] {
        auto screencastSession = qobject_cast<ScreenCastSession *>(session);
        const auto streams = screencastSession->streams();
        for (const Stream &stream : streams) {
            globalIntergration->stopStreaming(stream.nodeId);
        }
    });

    return PortalResponse::Success;
}

uint ScreencastPortalWayland::SelectSources(const QDBusObjectPath &handle,
                                            const QDBusObjectPath &session_handle,
                                            const QString &app_id,
                                            const QVariantMap &options,
                                            QVariantMap &results)
{
    Q_UNUSED(results)

    qCDebug(SCREENCAST) << "SelectSource called with parameters:";
    qCDebug(SCREENCAST) << "    handle: " << handle.path();
    qCDebug(SCREENCAST) << "    session_handle: " << session_handle.path();
    qCDebug(SCREENCAST) << "    app_id: " << app_id;
    qCDebug(SCREENCAST) << "    options: " << options;

    ScreenCastSession *session = Session::getSession<ScreenCastSession>(session_handle.path());

    if (!session) {
        qCWarning(SCREENCAST) << "Tried to select sources on non-existing session " << session_handle.path();
        return PortalResponse::OtherError;
    }

    session->setOptions(options);
    if (session->type() == Session::RemoteDesktop) {
        RemoteDesktopSession *remoteDesktopSession = qobject_cast<RemoteDesktopSession *>(session);
        if (remoteDesktopSession) {
            remoteDesktopSession->setScreenSharingEnabled(true);
        }
    } else {
        session->setPersistMode(PortalCommon::PersistMode(options.value(QStringLiteral("persist_mode")).toUInt()));
        session->setRestoreData(options.value(QStringLiteral("restore_data")));
    }

    return PortalResponse::Success;
}

std::pair<PortalResponse::Response, QVariantMap> ScreencastPortalWayland::continueStartAfterDialog(ScreenCastSession *session,
                                                                                                   const QList<QPointer<QScreen>> &selectedOutputs,
                                                                                                   const QRect &selectedRegion,
                                                                                                   bool allowRestore)
{
    Streams streams;
    QPointer<ScreenCastSession> guardedSession(session);
    for (const auto &output : std::as_const(selectedOutputs)) {
        if (!QGuiApplication::screens().contains(output)) {
            qCWarning(SCREENCAST) << "screen removed!";
            return {PortalResponse::OtherError, {}};
        }
        Stream outputStream = globalIntergration->startStreamingOutput(output, session->cursorMode());
        if (!outputStream.isValid()) {
            qCWarning(SCREENCAST) << "Invalid screen!" << output->name();
            return {PortalResponse::OtherError, {}};
        }
        streams.append(outputStream);
    }
    if (streams.isEmpty()) {
        qCWarning(SCREENCAST) << "Pipewire stream is not ready to be streamed";
        return {PortalResponse::OtherError, {}};
    }

    if (!guardedSession) {
        return {PortalResponse::OtherError, {}};
    }

    session->setStreams(streams);
    QVariantMap results;
    results.insert(QStringLiteral("streams"), QVariant::fromValue<Streams>(streams));
    if (allowRestore) {
        results.insert(QStringLiteral("persist_mode"), quint32(session->persistMode()));
        if (session->persistMode() != PortalCommon::NoPersist) {
            QVariantList outputNames;
            for (const QPointer<QScreen> &screen : selectedOutputs) {
                if (screen)
                    outputNames << screen->name();
            }

            QVariantMap restoreMap;
            restoreMap.insert(QStringLiteral("outputs"), outputNames);
            restoreMap.insert(QStringLiteral("region"), selectedRegion);

            const RestoreData restoreData(QStringLiteral("DDE"),
                                          RestoreData::currentRestoreDataVersion(),
                                          restoreMap);
            results.insert(QStringLiteral("restore_data"), QVariant::fromValue(restoreData));
        }
    }
    qCDebug(SCREENCAST) << "Screencast started successfully";
    return {PortalResponse::Success, results};
}

void ScreencastPortalWayland::Start(const QDBusObjectPath &handle,
                                    const QDBusObjectPath &session_handle,
                                    const QString &app_id,
                                    const QString &parent_window,
                                    const QVariantMap &options,
                                    const QDBusMessage &message,
                                    uint &replyResponse,
                                    QVariantMap &replyResults)
{
    qCDebug(SCREENCAST) << "Start called with parameters:";
    qCDebug(SCREENCAST) << "    handle: " << handle.path();
    qCDebug(SCREENCAST) << "    session_handle: " << session_handle.path();
    qCDebug(SCREENCAST) << "    app_id: " << app_id;
    qCDebug(SCREENCAST) << "    parent_window: " << parent_window;
    qCDebug(SCREENCAST) << "    options: " << options;

    QPointer<ScreenCastSession> session = Session::getSession<ScreenCastSession>(session_handle.path());

    if (!session) {
        qCWarning(SCREENCAST) << "Tried to call start on non-existing session " << session_handle.path();
        replyResponse = PortalResponse::OtherError;
        return;
    }

    if (QGuiApplication::screens().isEmpty()) {
        qCWarning(SCREENCAST) << "Failed to show dialog as there is no screen to select";
        replyResponse = PortalResponse::OtherError;
        return;
    }

    const PortalCommon::PersistMode persist = session->persistMode();
    bool valid = false;
    QList<QPointer<QScreen>> selectedOutputs;
    QRect selectedRegion;
    if (persist != PortalCommon::NoPersist && session->restoreData().isValid()) {
        const RestoreData restoreData = qdbus_cast<RestoreData>(session->restoreData().value<QDBusArgument>());
        if (restoreData.session == QLatin1String("DDE") && restoreData.version == RestoreData::currentRestoreDataVersion()) {
            const QVariantMap restoreDataPayload = restoreData.payload;
            const QVariantList restoreOutputs = restoreDataPayload[QStringLiteral("outputs")].toList();
            if (!restoreOutputs.isEmpty()) {
                ScreenListModel model(this);
                for (const auto &outputUniqueId : restoreOutputs) {
                    for (int i = 0, c = model.rowCount(); i < c; ++i) {
                        QScreen *iOutput = model.outputAt(i);
                        if (iOutput->name() == outputUniqueId) {
                            selectedOutputs << iOutput;
                        }
                    }
                }
                valid = selectedOutputs.count() == restoreOutputs.count();
            }

            const QRect restoreRegion = restoreDataPayload[QStringLiteral("region")].value<QRect>();
            if (restoreRegion.isValid()) {
                selectedRegion = restoreRegion;
                const auto screens = QGuiApplication::screens();
                QRegion fullWorkspace;
                for (const auto screen : screens) {
                    fullWorkspace += screen->geometry();
                }
                valid = fullWorkspace.contains(selectedRegion);
            }
        }
    }

    if (valid) {
        std::tie(replyResponse, replyResults) = continueStartAfterDialog(session, selectedOutputs, selectedRegion, true);
        return;
    }

    auto screenCastDialog = new ScreenCastChooser(app_id,
                                             session->types(),
                                             this);
    screenCastDialog->showWindow();
    Utils::setParentWindow(screenCastDialog->windowHandle(), parent_window);
    Request2::makeClosableDialogRequestWithSession(handle, screenCastDialog, session);
    delayReply(message, screenCastDialog, this, [screenCastDialog, session](ScreenCastChooser::DialogResult result) -> QVariantList {
        if (result == ScreenCastChooser::DialogResult::Rejected) {
            return {PortalResponse::fromDialogResult(result), QVariantMap{}};
        }
        QList<QPointer<QScreen>> screens = screenCastDialog->selectedOutputs();
        QRect region = screenCastDialog->selectedRegion();
        bool allowRestore = screenCastDialog->allowRestore();
        auto [response, results] = continueStartAfterDialog(session,
                                                            screens,
                                                            region,
                                                            allowRestore);
        return {response, results};
    });
}
