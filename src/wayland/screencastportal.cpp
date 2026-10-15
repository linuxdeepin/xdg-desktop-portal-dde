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
#include "amhelper.h"

#include <algorithm>

namespace {
constexpr auto ApplicationNameProperty = "xdp-application-name";
}

struct ToplevelsRestoreInfo {
    QString appId;
    QString identifier;
};

ScreencastPortalWayland::ScreencastPortalWayland(PortalWaylandContext *context)
    : AbstractWaylandPortal(context)
    , m_tray(new QSystemTrayIcon(QIcon::fromTheme("portal-screencast"), this))
{
    m_tray->setContextMenu(&m_menu);
    globalIntergration->init();
}

ScreencastPortalWayland::~ScreencastPortalWayland()
{
}

uint ScreencastPortalWayland::AvailableSourceTypes() const
{
    uint sourceTypes = 0;
    if (globalIntergration->isOutputStreamingAvailable()) {
        sourceTypes |= PortalCommon::Monitor;
    }
    if (globalIntergration->isToplevelStreamingAvailable()) {
        sourceTypes |= PortalCommon::Window;
    }
    return sourceTypes;
}

uint ScreencastPortalWayland::AvailableCursorModes() const
{
    return PortalCommon::Hidden |
            PortalCommon::Embedded;
}

uint ScreencastPortalWayland::CreateSession(const QDBusObjectPath &handle,
                                            const QDBusObjectPath &session_handle,
                                            const QString &app_id,
                                            const QVariantMap &options,
                                            QVariantMap &results)
{
    Q_UNUSED(results)

    qCDebug(SCREENCAST) << "CreateSession called with parameters:";
    qCDebug(SCREENCAST) << "    handle: " << handle.path();
    qCDebug(SCREENCAST) << "    session_handle: " << session_handle.path();
    qCDebug(SCREENCAST) << "    app_id: " << app_id;
    qCDebug(SCREENCAST) << "    options: " << options;

    if (!globalIntergration->isStreamingAvailable()) {
        qCWarning(SCREENCAST) << "xdpw: SHM image-copy capture is unavailable";
        return PortalResponse::OtherError;
    }

    Session *session = Session::createSession(this, Session::ScreenCast, app_id, session_handle.path());

    if (!session) {
        return PortalResponse::OtherError;
    }

    ScreenCastSession *screenCastSession = qobject_cast<ScreenCastSession *>(session);
    if (!screenCastSession) {
        qCCritical(SCREENCAST) << "Created session has an unexpected type"
                               << session_handle.path();
        session->close();
        return PortalResponse::OtherError;
    }

    connect(screenCastSession, &Session::closed, [screenCastSession, this] {
        // A session can be closed before Start succeeds, in which case it has
        // never acquired a tray action.
        if (QAction *action = m_sessionActions.take(screenCastSession)) {
            m_menu.removeAction(action);
            action->deleteLater();
            qCInfo(SCREENCAST) << "xdpw: screen sharing indicator deactivated for"
                               << screenCastSession->handle();
        }

        if (m_sessionActions.isEmpty()) {
            m_tray->hide();
            m_tray->setToolTip(QString());
        } else {
            updateTrayToolTip();
        }

        const auto streams = screenCastSession->streams();
        for (const Stream &stream : streams) {
            globalIntergration->stopStreaming(stream.stream.data());
        }
    });

    return PortalResponse::Success;
}

void ScreencastPortalWayland::activateSessionIndicator(ScreenCastSession *session)
{
    if (!session || m_sessionActions.contains(session)) {
        return;
    }

    if (Session::getSession<ScreenCastSession>(session->handle()) != session) {
        return;
    }

    if (session->streams().isEmpty()) {
        qCWarning(SCREENCAST) << "xdpw: refusing to activate a sharing indicator without streams"
                              << session->handle();
        return;
    }

    const QString appName = session->appId();
    auto *action = new QAction(tr("Stop [%1] Sharing").arg(appName), &m_menu);
    action->setProperty(ApplicationNameProperty, appName);

    const QPointer<ScreenCastSession> guardedSession(session);
    connect(action, &QAction::triggered, this, [guardedSession] {
        if (guardedSession) {
            guardedSession->close();
        }
    });

    m_menu.addAction(action);
    m_sessionActions.insert(session, action);
    updateTrayToolTip();
    m_tray->show();

    qCInfo(SCREENCAST) << "xdpw: screen sharing indicator activated for"
                       << session->handle();

    const QPointer<QAction> guardedAction(action);
    // ApplicationManager may need activation itself. Resolve the friendly
    // name asynchronously so it cannot block portal or capture processing.
    AMHelpers::nameFromAMAsync(session->appId(),
                               action,
                               [this, guardedSession, guardedAction](QString resolvedName) {
        if (!guardedSession
            || !guardedAction
            || m_sessionActions.value(guardedSession.data()) != guardedAction.data()) {
            return;
        }

        if (resolvedName.isEmpty()) {
            resolvedName = guardedSession->appId();
        }
        guardedAction->setProperty(ApplicationNameProperty, resolvedName);
        guardedAction->setText(tr("Stop [%1] Sharing").arg(resolvedName));
        updateTrayToolTip();
    });
}

void ScreencastPortalWayland::updateTrayToolTip()
{
    const QList<QAction *> actions = m_menu.actions();
    if (actions.isEmpty()) {
        m_tray->setToolTip(QString());
        return;
    }

    const QString appName = actions.constLast()->property(ApplicationNameProperty).toString();
    m_tray->setToolTip(tr("Sharing Screen to [%1]").arg(appName));
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
    if (session->cursorMode() != PortalCommon::Hidden
        && session->cursorMode() != PortalCommon::Embedded) {
        qCWarning(SCREENCAST) << "xdpw: requested cursor mode is unavailable"
                              << session->cursorMode();
        session->close();
        return PortalResponse::OtherError;
    }

    if (session->type() == Session::RemoteDesktop) {
        RemoteDesktopSession *remoteDesktopSession = qobject_cast<RemoteDesktopSession *>(session);
        if (remoteDesktopSession) {
            remoteDesktopSession->setScreenSharingEnabled(true);
        }
    } else {
        const uint persistMode = options.value(QStringLiteral("persist_mode")).toUInt();
        if (persistMode > static_cast<uint>(PortalCommon::PersistUntilRevoked)) {
            qCWarning(SCREENCAST) << "xdpw: invalid persist mode" << persistMode;
            session->close();
            return PortalResponse::OtherError;
        }
        session->setPersistMode(PortalCommon::PersistMode(persistMode));
        session->setRestoreData(options.value(QStringLiteral("restore_data")));
    }

    // SelectSources only configures the session. In particular, do not query
    // PipeWire or compositor runtime state here: availability can change
    // between SelectSources and Start, and Start is the authoritative point
    // where the requested source types are intersected with what is currently
    // usable. Keeping this path free of external work also lets the frontend
    // advance its session state as soon as the configuration is accepted.
    return PortalResponse::Success;
}

std::pair<PortalResponse::Response, QVariantMap> ScreencastPortalWayland::continueStartAfterDialog(ScreenCastSession *session,
                                                                                                   const QList<QPointer<QScreen>> &selectedOutputs,
                                                                                                   const QRect &selectedRegion,
                                                                                                   const QList<ToplevelInfoPtr> &selectedToplevels,
                                                                                                   bool allowRestore)
{
    if (!session
        || session->isClosed()
        || Session::getSession<ScreenCastSession>(session->handle()) != session) {
        qCWarning(SCREENCAST) << "xdpw: session closed before starting the selected sources";
        return {PortalResponse::OtherError, {}};
    }

    const QString sessionHandle = session->handle();
    const PortalCommon::SourceTypes requestedTypes = session->types();
    const bool multipleSources = session->multipleSources();
    const PortalCommon::CursorModes cursorMode = session->cursorMode();
    const PortalCommon::PersistMode requestedPersistMode = session->persistMode();
    const QPointer<ScreenCastSession> guardedSession(session);
    const auto sessionIsActive = [&guardedSession, &sessionHandle] {
        return guardedSession
                && !guardedSession->isClosed()
                && Session::getSession<ScreenCastSession>(sessionHandle)
                        == guardedSession.data();
    };

    const PortalCommon::SourceTypes selectedTypes =
            (selectedOutputs.isEmpty() ? PortalCommon::SourceTypes{} : PortalCommon::Monitor)
            | (selectedToplevels.isEmpty() ? PortalCommon::SourceTypes{} : PortalCommon::Window);
    if (!selectedTypes
        || (selectedTypes & ~requestedTypes)
        || (!multipleSources
            && selectedOutputs.size() + selectedToplevels.size() != 1)) {
        qCWarning(SCREENCAST) << "xdpw: selection does not match SelectSources"
                              << "requested" << requestedTypes.toInt()
                              << "selected" << selectedTypes.toInt()
                              << "multiple" << multipleSources;
        return {PortalResponse::OtherError, {}};
    }

    Streams streams;
    const auto stopStartedStreams = [&streams] {
        for (const Stream &stream : std::as_const(streams)) {
            globalIntergration->stopStreaming(stream.stream.data());
        }
        streams.clear();
    };
    const auto outputIsAvailable = [](const QPointer<QScreen> &output) {
        return output
                && QGuiApplication::screens().contains(output.data());
    };
    const auto allSourcesAreAvailable = [&selectedOutputs, &selectedToplevels, &outputIsAvailable] {
        return std::all_of(selectedOutputs.cbegin(),
                           selectedOutputs.cend(),
                           outputIsAvailable)
                && std::all_of(selectedToplevels.cbegin(),
                               selectedToplevels.cend(),
                               [](const ToplevelInfoPtr &toplevel) {
            return globalIntergration->isToplevelAvailable(toplevel);
        });
    };

    for (const auto &output : std::as_const(selectedOutputs)) {
        if (!sessionIsActive() || !outputIsAvailable(output)) {
            qCWarning(SCREENCAST) << "xdpw: session closed or screen removed before stream start";
            stopStartedStreams();
            return {PortalResponse::OtherError, {}};
        }
        Stream outputStream =
                globalIntergration->startStreamingOutput(output.data(), cursorMode);
        if (!outputStream.isValid()) {
            qCWarning(SCREENCAST) << "Invalid screen!" << output->name();
            stopStartedStreams();
            return {PortalResponse::OtherError, {}};
        }
        streams.append(outputStream);
        if (!sessionIsActive() || !outputIsAvailable(output)) {
            qCWarning(SCREENCAST)
                    << "xdpw: session closed or screen removed while its stream was starting";
            stopStartedStreams();
            return {PortalResponse::OtherError, {}};
        }
    }

    for (const ToplevelInfoPtr &toplevel : selectedToplevels) {
        if (!sessionIsActive()
            || !globalIntergration->isToplevelAvailable(toplevel)) {
            qCWarning(SCREENCAST)
                    << "xdpw: session closed or window removed before stream start";
            stopStartedStreams();
            return {PortalResponse::OtherError, {}};
        }

        Stream toplevelStream =
                globalIntergration->startStreamingToplevel(toplevel, cursorMode);
        if (!toplevelStream.isValid()) {
            qCWarning(SCREENCAST) << "Invalid toplevel!" << toplevel->appID;
            stopStartedStreams();
            return {PortalResponse::OtherError, {}};
        }

        streams << toplevelStream;
        if (!sessionIsActive()
            || !globalIntergration->isToplevelAvailable(toplevel)) {
            qCWarning(SCREENCAST)
                    << "xdpw: session closed or window removed while its stream was starting";
            stopStartedStreams();
            return {PortalResponse::OtherError, {}};
        }
    }

    if (streams.isEmpty()) {
        qCWarning(SCREENCAST) << "Pipewire stream is not ready to be streamed";
        return {PortalResponse::OtherError, {}};
    }

    const bool allStreamsActive =
            std::all_of(streams.cbegin(),
                        streams.cend(),
                        [](const Stream &stream) {
        return stream.isValid()
                && globalIntergration->isStreamActive(stream.stream.data());
    });
    if (!allStreamsActive || !allSourcesAreAvailable()) {
        qCWarning(SCREENCAST)
                << "xdpw: a selected source closed while the remaining streams were starting";
        stopStartedStreams();
        return {PortalResponse::OtherError, {}};
    }

    if (!sessionIsActive()
        || !guardedSession->setStreams(streams)) {
        qCWarning(SCREENCAST)
                << "xdpw: session closed before the selected streams could be attached";
        stopStartedStreams();
        return {PortalResponse::OtherError, {}};
    }

    QVariantMap results;
    results.insert(QStringLiteral("streams"), QVariant::fromValue<Streams>(streams));
    const PortalCommon::PersistMode effectivePersistMode =
            allowRestore ? requestedPersistMode : PortalCommon::NoPersist;
    guardedSession->setPersistMode(effectivePersistMode);
    results.insert(QStringLiteral("persist_mode"), quint32(effectivePersistMode));
    if (effectivePersistMode != PortalCommon::NoPersist) {
        QVariantList outputNames;
        for (const QPointer<QScreen> &screen : selectedOutputs) {
            if (screen) {
                outputNames << screen->name();
            }
        }

        QVariantList toplevelIdentifiers;
        for (const ToplevelInfoPtr &toplevel : selectedToplevels) {
            toplevelIdentifiers << toplevel->identifier;
        }

        QVariantMap restoreMap;
        restoreMap.insert(QStringLiteral("outputs"), outputNames);
        restoreMap.insert(QStringLiteral("region"), selectedRegion);
        restoreMap.insert(QStringLiteral("toplevels"), QVariant::fromValue(toplevelIdentifiers));

        const RestoreData restoreData(QStringLiteral("DDE"),
                                      RestoreData::currentRestoreDataVersion(),
                                      restoreMap);
        results.insert(QStringLiteral("restore_data"), QVariant::fromValue(restoreData));
    }
    activateSessionIndicator(guardedSession.data());
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

    const PortalCommon::SourceTypes availableTypes =
            PortalCommon::SourceTypes::fromInt(AvailableSourceTypes());
    const PortalCommon::SourceTypes selectableTypes = session->types() & availableTypes;
    if (!selectableTypes) {
        qCWarning(SCREENCAST) << "xdpw: no requested source type is currently available";
        replyResponse = PortalResponse::OtherError;
        return;
    }

    bool valid = false;
    QList<QPointer<QScreen>> selectedOutputs;
    QList<ToplevelInfoPtr> selectedToplevels;
    QRect selectedRegion;
    const QVariant restoreDataOption = session->restoreData();
    const bool restoreRequested = session->restoreRequested();
    if (restoreRequested) {
        const RestoreData restoreData =
                qdbus_cast<RestoreData>(restoreDataOption.value<QDBusArgument>());
        if (restoreData.session == QLatin1String("DDE") && restoreData.version == RestoreData::currentRestoreDataVersion()) {
            const QVariantMap restoreDataPayload = restoreData.payload;
            const QVariantList restoreOutputs = restoreDataPayload[QStringLiteral("outputs")].toList();
            const QVariantList restoreToplevels = restoreDataPayload[QStringLiteral("toplevels")].toList();
            bool restoreAttempted = false;
            bool restoreValid = true;
            if (!restoreOutputs.isEmpty()) {
                restoreAttempted = true;
                if (!selectableTypes.testFlag(PortalCommon::Monitor)) {
                    restoreValid = false;
                } else {
                    ScreenListModel model(this);
                    for (const auto &outputUniqueId : restoreOutputs) {
                        for (int i = 0, c = model.rowCount(); i < c; ++i) {
                            QScreen *iOutput = model.outputAt(i);
                            if (iOutput->name() == outputUniqueId) {
                                selectedOutputs << iOutput;
                                break;
                            }
                        }
                    }
                    restoreValid = restoreValid
                            && selectedOutputs.count() == restoreOutputs.count();
                }
            }

            const QRect restoreRegion = restoreDataPayload[QStringLiteral("region")].value<QRect>();
            if (restoreRegion.isValid()) {
                // Region capture is not implemented by this backend. Never
                // silently turn restored region data into a monitor stream.
                restoreAttempted = true;
                restoreValid = false;
            }

            if (!restoreToplevels.isEmpty()) {
                restoreAttempted = true;
                if (!selectableTypes.testFlag(PortalCommon::Window)) {
                    restoreValid = false;
                } else {
                    const QList<ToplevelInfoPtr> availableToplevels =
                            globalIntergration->m_context->toplevels();
                    for (const auto &toplevelIdentifier : restoreToplevels) {
                        for (const ToplevelInfoPtr &info : availableToplevels) {
                            if (info && toplevelIdentifier == info->identifier) {
                                selectedToplevels << info;
                                break;
                            }
                        }
                    }
                    restoreValid = restoreValid
                            && selectedToplevels.count() == restoreToplevels.count();
                }
            }

            const qsizetype restoredSourceCount =
                    selectedOutputs.size() + selectedToplevels.size();
            valid = restoreAttempted
                    && restoreValid
                    && restoredSourceCount > 0
                    && (session->multipleSources() || restoredSourceCount == 1);
        }
    }

    if (valid) {
        qCInfo(SCREENCAST) << "xdpw: restoring source selection"
                           << "outputs" << selectedOutputs.size()
                           << "toplevels" << selectedToplevels.size();
        auto [response, results] =
                continueStartAfterDialog(session,
                                         selectedOutputs,
                                         selectedRegion,
                                         selectedToplevels,
                                         true);
        replyResponse = response;
        replyResults = results;
        return;
    }

    if (restoreRequested) {
        qCInfo(SCREENCAST) << "xdpw: restore data is unavailable; showing source chooser";
    }

    auto screenCastDialog = new ScreenCastChooser(app_id,
                                                  selectableTypes,
                                                  session->multipleSources(),
                                                  session->persistMode() != PortalCommon::NoPersist,
                                                  this);
    if (!screenCastDialog->windowHandle()) {
        qCWarning(SCREENCAST) << "xdpw: failed to create the source chooser";
        screenCastDialog->deleteLater();
        replyResponse = PortalResponse::OtherError;
        return;
    }
    screenCastDialog->showWindow();
    Utils::setParentWindow(screenCastDialog->windowHandle(), parent_window);
    Request2::makeClosableDialogRequestWithSession(handle, screenCastDialog, session);
    delayReply(message, screenCastDialog, this, [this, screenCastDialog, session](ScreenCastChooser::DialogResult result) -> QVariantList {
        if (result == ScreenCastChooser::DialogResult::Rejected) {
            return {PortalResponse::fromDialogResult(result), QVariantMap{}};
        }
        QList<QPointer<QScreen>> screens = screenCastDialog->selectedOutputs();
        QRect region = screenCastDialog->selectedRegion();
        const QList<ToplevelInfoPtr> toplevels = screenCastDialog->selectedToplevels();
        bool allowRestore = screenCastDialog->allowRestore();
        auto [response, results] = continueStartAfterDialog(session,
                                                            screens,
                                                            region,
                                                            toplevels,
                                                            allowRestore);
        return {response, results};
    });
}
