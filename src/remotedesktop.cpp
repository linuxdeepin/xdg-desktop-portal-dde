// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "remotedesktop.h"
#include "session.h"
#include "remotedesktopdialog.h"
#include "utils.h"
#include "waylandintegration.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopPortalDdeRemoteDesktop, "xdp-dde-remotedesktop")

RemoteDesktopPortal::RemoteDesktopPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
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
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "CreateSession called with parameters:";
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    handle: " << handle.path();
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    app_id: " << app_id;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    options: " << options;

    Session *session = Session::createSession(this, Session::RemoteDesktop, app_id, session_handle.path());

    if (!session) {
        return 2;
    }

    connect(session, &Session::closed, [] () {
        WaylandIntegration::stopAllStreaming();
    });

    if (!WaylandIntegration::isStreamingAvailable()) {
        qCWarning(XdgDesktopPortalDdeRemoteDesktop) << "zkde_screencast_unstable_v1 does not seem to be available";
        return 2;
    }

    return 0;
}

uint RemoteDesktopPortal::SelectDevices(const QDBusObjectPath &handle,
                                        const QDBusObjectPath &session_handle,
                                        const QString &app_id,
                                        const QVariantMap &options,
                                        QVariantMap &results)
{
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "SelectDevices called with parameters:";
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    handle: " << handle.path();
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    app_id: " << app_id;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    options: " << options;

    RemoteDesktopPortal::DeviceTypes types = RemoteDesktopPortal::None;
    if (options.contains(QStringLiteral("types"))) {
        types = static_cast<RemoteDesktopPortal::DeviceTypes>(options.value(QStringLiteral("types")).toUInt());
    }

    RemoteDesktopSession *session = qobject_cast<RemoteDesktopSession*>(Session::getSession(session_handle.path()));

    if (!session) {
        qCWarning(XdgDesktopPortalDdeRemoteDesktop) << "Tried to select sources on non-existing session " << session_handle.path();
        return 2;
    }

    if (options.contains(QStringLiteral("types"))) {
        types = (DeviceTypes)(options.value(QStringLiteral("types")).toUInt());
    }
    session->setDeviceTypes(types);

    return 0;
}

uint RemoteDesktopPortal::Start(const QDBusObjectPath &handle,
                                const QDBusObjectPath &session_handle,
                                const QString &app_id,
                                const QString &parent_window,
                                const QVariantMap &options,
                                QVariantMap &results)
{
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    RemoteDesktopPortal Start called with parameters:";
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    handle: " << handle.path();
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    app_id: " << app_id;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    parent_window: " << parent_window;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    options: " << options;

    RemoteDesktopSession *session = qobject_cast<RemoteDesktopSession*>(Session::getSession(session_handle.path()));

    if (!session) {
        qCWarning(XdgDesktopPortalDdeRemoteDesktop) << "Tried to call start on non-existing session " << session_handle.path();
        return 2;
    }

    // TODO check whether we got some outputs?
    if (WaylandIntegration::screens().isEmpty()) {
        qCWarning(XdgDesktopPortalDdeRemoteDesktop) << "Failed to show dialog as there is no screen to select";
        return 2;
    }

    QScopedPointer<RemoteDesktopDialog, QScopedPointerDeleteLater> remoteDesktopDialog(new RemoteDesktopDialog(app_id, session->deviceTypes(), session->screenSharingEnabled(), session->multipleSources()));
    Utils::setParentWindow(remoteDesktopDialog.data(), QString());

    connect(session, &Session::closed, remoteDesktopDialog.data(), &RemoteDesktopDialog::reject);
    if (!parent_window.isEmpty() || remoteDesktopDialog->exec()) {
        if (session->screenSharingEnabled()) {
            if (!WaylandIntegration::startStreamingOutput(remoteDesktopDialog->selectedScreens().first(), Screencasting::Hidden)) {
                return 2;
            }

            WaylandIntegration::authenticate();

            QVariant streams = WaylandIntegration::streams();

            if (!streams.isValid()) {
                qCWarning(XdgDesktopPortalDdeRemoteDesktop()) << "Pipewire stream is not ready to be streamed";
                return 2;
            }

            results.insert(QStringLiteral("streams"), streams);
        } else {
            qCWarning(XdgDesktopPortalDdeRemoteDesktop()) << "Only stream input";
            WaylandIntegration::startStreamingInput();
            WaylandIntegration::authenticate();
        }
        results.insert(QStringLiteral("devices"), QVariant::fromValue<uint>(remoteDesktopDialog->deviceTypes()));
        return 0;
    }
    return 1;
}

void RemoteDesktopPortal::NotifyPointerMotion(const QDBusObjectPath &session_handle,
                                              const QVariantMap &options,
                                              double dx,
                                              double dy)
{
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "NotifyPointerMotion called with parameters:";
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    options: " << options;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    dx: " << dx;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    dy: " << dy;

    RemoteDesktopSession *session = qobject_cast<RemoteDesktopSession*>(Session::getSession(session_handle.path()));

    if (!session) {
        qCWarning(XdgDesktopPortalDdeRemoteDesktop) << "Tried to call NotifyPointerMotion on non-existing session " << session_handle.path();
        return;
    }

    WaylandIntegration::requestPointerMotion(QSizeF(dx, dy));
}

void RemoteDesktopPortal::NotifyPointerMotionAbsolute(const QDBusObjectPath &session_handle,
                                                      const QVariantMap &options,
                                                      uint stream,
                                                      double x,
                                                      double y)
{
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "NotifyPointerMotionAbsolute called with parameters:";
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    options: " << options;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    stream: " << stream;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    x: " << x;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    y: " << y;

    RemoteDesktopSession *session = qobject_cast<RemoteDesktopSession*>(Session::getSession(session_handle.path()));

    if (!session) {
        qCWarning(XdgDesktopPortalDdeRemoteDesktop) << "Tried to call NotifyPointerMotionAbsolute on non-existing session " << session_handle.path();
        return;
    }

    WaylandIntegration::requestPointerMotionAbsolute(QPointF(x, y));
}

void RemoteDesktopPortal::NotifyPointerButton(const QDBusObjectPath &session_handle,
                                              const QVariantMap &options,
                                              int button,
                                              uint state)
{
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "NotifyPointerButton called with parameters:";
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    options: " << options;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    button: " << button;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    state: " << state;

    RemoteDesktopSession *session = qobject_cast<RemoteDesktopSession*>(Session::getSession(session_handle.path()));

    if (!session) {
        qCWarning(XdgDesktopPortalDdeRemoteDesktop) << "Tried to call NotifyPointerButton on non-existing session " << session_handle.path();
        return;
    }

    if (state) {
        WaylandIntegration::requestPointerButtonPress(button);
    } else {
        WaylandIntegration::requestPointerButtonRelease(button);
    }
}

void RemoteDesktopPortal::NotifyPointerAxis(const QDBusObjectPath &session_handle,
                                            const QVariantMap &options,
                                            double dx,
                                            double dy)
{
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "NotifyPointerAxis called with parameters:";
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    options: " << options;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    dx: " << dx;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    dy: " << dy;
}

void RemoteDesktopPortal::NotifyPointerAxisDiscrete(const QDBusObjectPath &session_handle,
                                                    const QVariantMap &options,
                                                    uint axis,
                                                    int steps)
{
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "NotifyPointerAxisDiscrete called with parameters:";
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    options: " << options;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    axis: " << axis;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    steps: " << steps;

    RemoteDesktopSession *session = qobject_cast<RemoteDesktopSession*>(Session::getSession(session_handle.path()));

    if (!session) {
        qCWarning(XdgDesktopPortalDdeRemoteDesktop) << "Tried to call NotifyPointerAxisDiscrete on non-existing session " << session_handle.path();
        return;
    }

    WaylandIntegration::requestPointerAxisDiscrete(!axis ? Qt::Vertical : Qt::Horizontal, steps);
}

void RemoteDesktopPortal::NotifyKeyboardKeysym(const QDBusObjectPath &session_handle,
                                               const QVariantMap &options,
                                               int keysym,
                                               uint state)
{
}

void RemoteDesktopPortal::NotifyKeyboardKeycode(const QDBusObjectPath &session_handle,
                                                const QVariantMap &options,
                                                int keycode,
                                                uint state)
{
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "NotifyKeyboardKeycode called with parameters:";
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    options: " << options;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    keycode: " << keycode;
    qCDebug(XdgDesktopPortalDdeRemoteDesktop) << "    state: " << state;

    RemoteDesktopSession *session = qobject_cast<RemoteDesktopSession*>(Session::getSession(session_handle.path()));

    if (!session) {
        qCWarning(XdgDesktopPortalDdeRemoteDesktop) << "Tried to call NotifyKeyboardKeycode on non-existing session " << session_handle.path();
        return;
    }

    WaylandIntegration::requestKeyboardKeycode(keycode, state != 0);
}

void RemoteDesktopPortal::NotifyTouchDown(const QDBusObjectPath &session_handle,
                                          const QVariantMap &options,
                                          uint stream,
                                          uint slot,
                                          int x,
                                          int y)
{
}

void RemoteDesktopPortal::NotifyTouchMotion(const QDBusObjectPath &session_handle,
                                            const QVariantMap &options,
                                            uint stream,
                                            uint slot,
                                            int x,
                                            int y)
{
}

void RemoteDesktopPortal::NotifyTouchUp(const QDBusObjectPath &session_handle,
                                        const QVariantMap &options,
                                        uint slot)
{
}
