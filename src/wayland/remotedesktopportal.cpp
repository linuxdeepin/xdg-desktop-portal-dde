#include "remotedesktopportal.h"
#include "remotedesktopsession.h"
#include "screencastportal.h"
#include "dbushelpers.h"
#include "loggings.h"
#include "amhelper.h"
#include "treelandintegration.h"
#include "restoredata.h"
#include "protocols/virtualinput.h"
#include "remotedesktopchooser.h"
#include "request2.h"
#include "utils.h"
#include "screenlistmodel.h"

RemoteDesktopPortal::RemoteDesktopPortal(PortalWaylandContext *context)
    : AbstractWaylandPortal(context)
{
}

RemoteDesktopPortal::~RemoteDesktopPortal()
{
}

uint RemoteDesktopPortal::CreateSession(const QDBusObjectPath &handle,
                                        const QDBusObjectPath &session_handle,
                                        const QString &app_id,
                                        const QVariantMap &options,
                                        QVariantMap &results)
{
    Q_UNUSED(results);
    qCDebug(REMOTEDESKTOP) << "CreateSession called with parameters:";
    qCDebug(REMOTEDESKTOP) << "    handle: " << handle.path();
    qCDebug(REMOTEDESKTOP) << "    session_handle: " << session_handle.path();
    qCDebug(REMOTEDESKTOP) << "    app_id: " << app_id;
    qCDebug(REMOTEDESKTOP) << "    options: " << options;

    auto *session = qobject_cast<RemoteDesktopSession *>(
        Session::createSession(this, Session::RemoteDesktop, app_id, session_handle.path()));
    if (!session)
        return PortalResponse::OtherError;

    connect(session, &Session::closed, [session] {
        for (const Stream &stream : session->streams())
            globalIntergration->stopStreaming(stream.nodeId);
    });

    return PortalResponse::Success;
}

uint RemoteDesktopPortal::SelectDevices(const QDBusObjectPath &handle,
                                        const QDBusObjectPath &session_handle,
                                        const QString &app_id,
                                        const QVariantMap &options,
                                        QVariantMap &results)
{
    Q_UNUSED(results);
    qCDebug(REMOTEDESKTOP) << "SelectDevices called with parameters:";
    qCDebug(REMOTEDESKTOP) << "    handle: " << handle.path();
    qCDebug(REMOTEDESKTOP) << "    session_handle: " << session_handle.path();
    qCDebug(REMOTEDESKTOP) << "    app_id: " << app_id;
    qCDebug(REMOTEDESKTOP) << "    options: " << options;

    const auto supportedTypes = static_cast<PortalCommon::DeviceTypes>(AvailableDeviceTypes());
    // The types option is optional and defaults to all available device types.
    // Do not interpret a missing option as an explicit request for no devices.
    const auto requestedTypes = static_cast<PortalCommon::DeviceTypes>(
        options.value(QStringLiteral("types"), static_cast<uint>(supportedTypes)).toUInt());
    const auto unsupportedTypes = requestedTypes & ~supportedTypes;
    if (unsupportedTypes) {
        qCWarning(REMOTEDESKTOP) << "Ignoring unsupported remote desktop device types:" << unsupportedTypes;
    }
    const auto types = requestedTypes & supportedTypes;

    // A request containing only unsupported devices would otherwise open a
    // chooser with no input controls and an Allow button that can never be
    // enabled. Fail the SelectDevices request instead. An explicit value of 0
    // is valid for screen-sharing-only sessions.
    if (requestedTypes != PortalCommon::None && types == PortalCommon::None) {
        qCWarning(REMOTEDESKTOP) << "No supported remote desktop device types were requested:" << requestedTypes;
        return PortalResponse::OtherError;
    }

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());

    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to select sources on non-existing session " << session_handle.path();
        return PortalResponse::OtherError;
    }

    session->setDeviceTypes(types);
    session->setPersistMode(PortalCommon::PersistMode(options.value(QStringLiteral("persist_mode")).toUInt()));
    session->setRestoreData(options.value(QStringLiteral("restore_data")));

    return PortalResponse::Success;
}

static std::pair<PortalResponse::Response, QVariantMap>
continueStart(RemoteDesktopSession *session,
              const QList<QPointer<QScreen>> &selectedOutputs,
              const QList<ToplevelInfo *> &selectedToplevels,
              bool allowRestore)
{
    QVariantMap results;
    QPointer<RemoteDesktopSession> guardedSession(session);
    session->acquireStreamingInput();
    if (!session->inputAcquired())
        return {PortalResponse::OtherError, {}};

    if (session->screenSharingEnabled()) {
        Streams streams;
        for (const auto &screen : selectedOutputs) {
            if (!screen || !qGuiApp->screens().contains(screen)) {
                qCWarning(REMOTEDESKTOP) << "Selected screen disappeared";
                return {PortalResponse::OtherError, {}};
            }
            const Stream stream = globalIntergration->startStreamingOutput(screen, session->cursorMode());
            if (!stream.isValid() || !guardedSession)
                return {PortalResponse::OtherError, {}};
            streams.push_back(stream);
        }
        for (ToplevelInfo *toplevel : selectedToplevels) {
            const Stream stream = globalIntergration->startStreamingToplevel(toplevel, session->cursorMode());
            if (!stream.isValid() || !guardedSession)
                return {PortalResponse::OtherError, {}};
            streams.push_back(stream);
        }

        if (!guardedSession || streams.isEmpty()) {
            return {PortalResponse::OtherError, {}};
        }

        session->setStreams(std::move(streams));
        QList<std::pair<uint, QVariantMap>> dbusResultForStreams;
        for (const Stream &stream : session->streams())
            dbusResultForStreams.append({stream.nodeId, stream.map});
        results.insert(QStringLiteral("streams"), QVariant::fromValue(dbusResultForStreams));
    } else {
        qCWarning(REMOTEDESKTOP()) << "Only stream input";
        session->refreshDescription();
    }
    results.insert(QStringLiteral("devices"), QVariant::fromValue<uint>(session->deviceTypes()));
    results.insert(QStringLiteral("clipboard_enabled"), session->clipboardEnabled());
    if (!allowRestore)
        session->setPersistMode(PortalCommon::NoPersist);
    results.insert(QStringLiteral("persist_mode"), quint32(session->persistMode()));
    if (session->persistMode() != PortalCommon::NoPersist) {
        QVariantList outputNames;
        for (const QPointer<QScreen> &screen : selectedOutputs)
            if (screen)
                outputNames.append(screen->name());
        QVariantList toplevelIdentifiers;
        for (const ToplevelInfo *toplevel : selectedToplevels)
            if (toplevel)
                toplevelIdentifiers.append(toplevel->identifier);
        const RestoreData restoreData = {QStringLiteral("DDE"),
                                          RestoreData::currentRestoreDataVersion(),
                                          QVariantMap{{QStringLiteral("screenShareEnabled"), session->screenSharingEnabled()},
                                                       {QStringLiteral("devices"), static_cast<quint32>(session->deviceTypes())},
                                                       {QStringLiteral("clipboardEnabled"), session->clipboardEnabled()},
                                                       {QStringLiteral("outputs"), outputNames},
                                                       {QStringLiteral("toplevels"), toplevelIdentifiers}}};
        results.insert(QStringLiteral("restore_data"), QVariant::fromValue<RestoreData>(restoreData));
    }
    return {PortalResponse::Success, results};
}

void RemoteDesktopPortal::Start(const QDBusObjectPath &handle,
                                const QDBusObjectPath &session_handle,
                                const QString &app_id,
                                const QString &parent_window,
                                const QVariantMap &options,
                                const QDBusMessage &message,
                                uint &replyResponse,
                                QVariantMap &replyResults)
{
    qCDebug(REMOTEDESKTOP) << "Start called with parameters:";
    qCDebug(REMOTEDESKTOP) << "    handle: " << handle.path();
    qCDebug(REMOTEDESKTOP) << "    session_handle: " << session_handle.path();
    qCDebug(REMOTEDESKTOP) << "    app_id: " << app_id;
    qCDebug(REMOTEDESKTOP) << "    parent_window: " << parent_window;
    qCDebug(REMOTEDESKTOP) << "    options: " << options;

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());

    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to call start on non-existing session " << session_handle.path();
        replyResponse = PortalResponse::OtherError;
        return;
    }

    if (!session->screenSharingEnabled() && session->deviceTypes() == PortalCommon::None) {
        qCWarning(REMOTEDESKTOP) << "Cannot start a remote desktop session without an input device";
        replyResponse = PortalResponse::OtherError;
        return;
    }

    if (session->screenSharingEnabled() && QGuiApplication::screens().isEmpty()) {
        qCWarning(REMOTEDESKTOP) << "Failed to show dialog as there is no screen to select";
        replyResponse = PortalResponse::OtherError;
        return;
    }
    bool outputStreamingAvailable = false;
    bool toplevelStreamingAvailable = false;
    if (session->screenSharingEnabled()) {
        const PortalCommon::SourceTypes requestedTypes = session->types();
        const bool screenRequested = requestedTypes == PortalCommon::Any
            || requestedTypes.testFlag(PortalCommon::Monitor);
        const bool windowRequested = requestedTypes == PortalCommon::Any
            || requestedTypes.testFlag(PortalCommon::Window);
        outputStreamingAvailable = screenRequested && globalIntergration->isOutputStreamingAvailable();
        toplevelStreamingAvailable = windowRequested && globalIntergration->isToplevelStreamingAvailable();

        if (!outputStreamingAvailable && !toplevelStreamingAvailable) {
            qCWarning(REMOTEDESKTOP) << "Screen sharing is not available for the requested source types"
                                      << requestedTypes;
            replyResponse = PortalResponse::OtherError;
            return;
        }
    }

    bool restored = false;
    QList<QPointer<QScreen>> selectedOutputs;
    QList<ToplevelInfo *> selectedToplevels;

    if (session->restoreData().isValid()) {
        const RestoreData restoreData = qdbus_cast<RestoreData>(session->restoreData().value<QDBusArgument>());
        if (restoreData.session == QLatin1String("DDE") && restoreData.version == RestoreData::currentRestoreDataVersion()) {
            // check we asked for the same key content both times; if not, don't restore
            // some settings (like ScreenCast multipleSources or cursorMode) don't involve user prompts so use whatever was explicitly
            // requested this time
            const auto devices = static_cast<PortalCommon::DeviceTypes>(restoreData.payload[QStringLiteral("devices")].toUInt());
            if (session->deviceTypes() != devices) {
                qCDebug(REMOTEDESKTOP) << "Not restoring session as requested devices don't match";
            } else if (session->screenSharingEnabled() != restoreData.payload[QStringLiteral("screenShareEnabled")].toBool()) {
                qCDebug(REMOTEDESKTOP) << "Not restoring session as requested screen sharing doesn't match";
            } else if (session->clipboardEnabled() != restoreData.payload[QStringLiteral("clipboardEnabled")].toBool()) {
                qCDebug(REMOTEDESKTOP) << "Not restoring session as clipboard enabled doesn't match";
            } else {
                restored = true;
            }
            if (restored && session->screenSharingEnabled()) {
                const QVariantList outputNames = restoreData.payload.value(QStringLiteral("outputs")).toList();
                const QVariantList toplevelIdentifiers = restoreData.payload.value(QStringLiteral("toplevels")).toList();
                ScreenListModel screenModel(this);
                for (const QVariant &outputName : outputNames) {
                    for (int i = 0; i < screenModel.rowCount(); ++i) {
                        QScreen *screen = screenModel.outputAt(i);
                        if (screen && screen->name() == outputName.toString())
                            selectedOutputs.append(screen);
                    }
                }
                for (const QVariant &identifier : toplevelIdentifiers) {
                    for (ToplevelInfo *info : globalIntergration->toplevels()) {
                        if (info && info->identifier == identifier.toString())
                            selectedToplevels.append(info);
                    }
                }
                restored = selectedOutputs.size() == outputNames.size()
                    && selectedToplevels.size() == toplevelIdentifiers.size()
                    && (!selectedOutputs.isEmpty() || !selectedToplevels.isEmpty());
            }
        }
    }

    if (restored) {
        std::tie(replyResponse, replyResults) =
            continueStart(session, selectedOutputs, selectedToplevels, true);
        return;
    }

    PortalCommon::SourceTypes availableSourceTypes;
    if (outputStreamingAvailable)
        availableSourceTypes |= PortalCommon::Monitor;
    if (toplevelStreamingAvailable)
        availableSourceTypes |= PortalCommon::Window;

    auto *dialog = new RemoteDesktopChooser(app_id,
                                            session->deviceTypes(),
                                            session->screenSharingEnabled(),
                                            availableSourceTypes,
                                            this);
    Utils::setParentWindow(dialog->windowHandle(), parent_window);
    dialog->showWindow();
    Request2::makeClosableDialogRequestWithSession(handle, dialog, session);
    delayReply(message, dialog, this,
               [session, dialog](RemoteDesktopChooser::DialogResult result) -> QVariantList {
        if (result == RemoteDesktopChooser::Rejected)
            return {PortalResponse::Cancelled, QVariantMap{}};
        session->setDeviceTypes(dialog->selectedDevices());
        auto [response, results] = continueStart(session,
                                                 dialog->selectedOutputs(),
                                                 dialog->selectedToplevels(),
                                                 dialog->allowRestore());
        return {response, results};
    });
}

void RemoteDesktopPortal::NotifyPointerMotion(const QDBusObjectPath &session_handle, const QVariantMap &options, double dx, double dy)
{
    qCDebug(REMOTEDESKTOP) << "NotifyPointerMotion called with parameters:";
    qCDebug(REMOTEDESKTOP) << "    session_handle: " << session_handle.path();
    qCDebug(REMOTEDESKTOP) << "    options: " << options;
    qCDebug(REMOTEDESKTOP) << "    dx: " << dx;
    qCDebug(REMOTEDESKTOP) << "    dy: " << dy;

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());

    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to call NotifyPointerMotion on non-existing session " << session_handle.path();
        return;
    }

    if (!session->inputAcquired() || !session->deviceTypes().testFlag(PortalCommon::Pointer))
        return;
    session->virtualInput()->pointerMotion(dx, dy);
}

void RemoteDesktopPortal::NotifyPointerMotionAbsolute(const QDBusObjectPath &session_handle, const QVariantMap &options, uint stream, double x, double y)
{
    qCDebug(REMOTEDESKTOP) << "NotifyPointerMotionAbsolute called with parameters:";
    qCDebug(REMOTEDESKTOP) << "    session_handle: " << session_handle.path();
    qCDebug(REMOTEDESKTOP) << "    options: " << options;
    qCDebug(REMOTEDESKTOP) << "    stream: " << stream;
    qCDebug(REMOTEDESKTOP) << "    x: " << x;
    qCDebug(REMOTEDESKTOP) << "    y: " << y;

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());

    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to call NotifyPointerMotionAbsolute on non-existing session " << session_handle.path();
        return;
    }

    const Streams streams = session->streams();
    const auto it = std::find_if(streams.cbegin(), streams.cend(), [stream](const Stream &candidate) { return candidate.nodeId == stream; });
    if (it == streams.cend()) {
        qCWarning(REMOTEDESKTOP) << "Unknown PipeWire stream" << stream;
        return;
    }
    const QSize size = it->map.value(QStringLiteral("size")).toSize();
    if (!session->inputAcquired() || !session->deviceTypes().testFlag(PortalCommon::Pointer))
        return;
    session->virtualInput()->pointerMotionAbsolute(x, y, size.width(), size.height());
}

void RemoteDesktopPortal::NotifyPointerButton(const QDBusObjectPath &session_handle, const QVariantMap &options, int button, uint state)
{
    qCDebug(REMOTEDESKTOP) << "NotifyPointerButton called with parameters:";
    qCDebug(REMOTEDESKTOP) << "    session_handle: " << session_handle.path();
    qCDebug(REMOTEDESKTOP) << "    options: " << options;
    qCDebug(REMOTEDESKTOP) << "    button: " << button;
    qCDebug(REMOTEDESKTOP) << "    state: " << state;

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());

    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to call NotifyPointerButton on non-existing session " << session_handle.path();
        return;
    }

    if (!session->inputAcquired() || !session->deviceTypes().testFlag(PortalCommon::Pointer))
        return;
    session->virtualInput()->pointerButton(static_cast<uint32_t>(button), state != 0);
}

void RemoteDesktopPortal::NotifyPointerAxis(const QDBusObjectPath &session_handle, const QVariantMap &options, double dx, double dy)
{
    qCDebug(REMOTEDESKTOP) << "NotifyPointerAxis called with parameters:";
    qCDebug(REMOTEDESKTOP) << "    session_handle: " << session_handle.path();
    qCDebug(REMOTEDESKTOP) << "    options: " << options;
    qCDebug(REMOTEDESKTOP) << "    dx: " << dx;
    qCDebug(REMOTEDESKTOP) << "    dy: " << dy;

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());

    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to call NotifyKeyboardKeysym on non-existing session " << session_handle.path();
        return;
    }

    if (!session->inputAcquired() || !session->deviceTypes().testFlag(PortalCommon::Pointer))
        return;
    session->virtualInput()->pointerAxis(dx, dy);
}

void RemoteDesktopPortal::NotifyPointerAxisDiscrete(const QDBusObjectPath &session_handle, const QVariantMap &options, uint axis, int steps)
{
    qCDebug(REMOTEDESKTOP) << "NotifyPointerAxisDiscrete called with parameters:";
    qCDebug(REMOTEDESKTOP) << "    session_handle: " << session_handle.path();
    qCDebug(REMOTEDESKTOP) << "    options: " << options;
    qCDebug(REMOTEDESKTOP) << "    axis: " << axis;
    qCDebug(REMOTEDESKTOP) << "    steps: " << steps;

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());

    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to call NotifyPointerAxisDiscrete on non-existing session " << session_handle.path();
        return;
    }

    if (!session->inputAcquired() || !session->deviceTypes().testFlag(PortalCommon::Pointer))
        return;
    session->virtualInput()->pointerAxisDiscrete(axis, steps);
}

void RemoteDesktopPortal::NotifyKeyboardKeysym(const QDBusObjectPath &session_handle, const QVariantMap &options, int keysym, uint state)
{
    Q_UNUSED(options)

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());

    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to call NotifyKeyboardKeysym on non-existing session " << session_handle.path();
        return;
    }

    if (!session->inputAcquired() || !session->deviceTypes().testFlag(PortalCommon::Keyboard))
        return;
    if (!session->virtualInput()->keyboardKeysym(static_cast<uint32_t>(keysym), state != 0))
        qCWarning(REMOTEDESKTOP) << "Keysym is not present in the virtual keyboard keymap:" << keysym;
}

void RemoteDesktopPortal::NotifyKeyboardKeycode(const QDBusObjectPath &session_handle, const QVariantMap &options, int keycode, uint state)
{
    qCDebug(REMOTEDESKTOP) << "NotifyKeyboardKeycode called with parameters:";
    qCDebug(REMOTEDESKTOP) << "    session_handle: " << session_handle.path();
    qCDebug(REMOTEDESKTOP) << "    options: " << options;
    qCDebug(REMOTEDESKTOP) << "    keycode: " << keycode;
    qCDebug(REMOTEDESKTOP) << "    state: " << state;

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());

    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to call NotifyKeyboardKeycode on non-existing session " << session_handle.path();
        return;
    }

    if (!session->inputAcquired() || !session->deviceTypes().testFlag(PortalCommon::Keyboard))
        return;
    session->virtualInput()->keyboardKeycode(static_cast<uint32_t>(keycode), state != 0);
}

void RemoteDesktopPortal::NotifyTouchDown(const QDBusObjectPath &session_handle, const QVariantMap &options, uint stream, uint slot, double x, double y)
{
    Q_UNUSED(options)
    Q_UNUSED(stream)

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());
    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to call NotifyPointerAxisDiscrete on non-existing session " << session_handle.path();
        return;
    }
    qCWarning(REMOTEDESKTOP) << "Touch injection is not supported by the wlr virtual input protocols";
}

void RemoteDesktopPortal::NotifyTouchMotion(const QDBusObjectPath &session_handle, const QVariantMap &options, uint stream, uint slot, double x, double y)
{
    Q_UNUSED(options)
    Q_UNUSED(stream)

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());
    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to call NotifyPointerAxisDiscrete on non-existing session " << session_handle.path();
        return;
    }
    qCWarning(REMOTEDESKTOP) << "Touch injection is not supported by the wlr virtual input protocols";
}

void RemoteDesktopPortal::NotifyTouchUp(const QDBusObjectPath &session_handle, const QVariantMap &options, uint slot)
{
    Q_UNUSED(options)

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());
    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to call NotifyPointerAxisDiscrete on non-existing session " << session_handle.path();
        return;
    }

    qCWarning(REMOTEDESKTOP) << "Touch injection is not supported by the wlr virtual input protocols";
}

QDBusUnixFileDescriptor
RemoteDesktopPortal::ConnectToEIS(const QDBusObjectPath &session_handle, const QString &app_id, const QVariantMap &options, const QDBusMessage &message)
{
    Q_UNUSED(options)
    Q_UNUSED(app_id)

    RemoteDesktopSession *session = Session::getSession<RemoteDesktopSession>(session_handle.path());
    if (!session) {
        qCWarning(REMOTEDESKTOP) << "Tried to call ConnectToEIS on non-existing session" << session_handle.path();
        return QDBusUnixFileDescriptor();
    }

    Q_UNUSED(message)
    qCWarning(REMOTEDESKTOP) << "ConnectToEIS is unavailable when using wlr virtual input protocols";
    return {};
}
