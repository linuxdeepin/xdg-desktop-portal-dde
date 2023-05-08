// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screencast.h"
#include "screenchooserdialog.h"
#include "session.h"
#include "waylandintegration.h"
#include "utils.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDEScreenCastProtal, "xdg-dde-screencast")

ScreenCastPortal::ScreenCastPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qCDebug(XdgDesktopDDEScreenCastProtal) << "init screencast";
}

ScreenCastPortal::~ScreenCastPortal()
{
}

uint ScreenCastPortal::CreateSession(const QDBusObjectPath &handle,
                                     const QDBusObjectPath &session_handle,
                                     const QString &app_id,
                                     const QVariantMap &options,
                                     QVariantMap &results)
{
    Q_UNUSED(results)

    qCDebug(XdgDesktopDDEScreenCastProtal) << "CreateSession called with parameters:";
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    handle: " << handle.path();
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    app_id: " << app_id;
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    options: " << options;

    Session *session = Session::createSession(this, Session::ScreenCast, app_id, session_handle.path());

    if (!session) {
        return 2;
    }

    if (!WaylandIntegration::isStreamingAvailable()) {
        qCWarning(XdgDesktopDDEScreenCastProtal) << "zkde_screencast_unstable_v1 does not seem to be available";
        return 2;
    }

    connect(session, &Session::closed, [] () {
        WaylandIntegration::stopAllStreaming();
    });

    return 0;
}

uint ScreenCastPortal::SelectSources(const QDBusObjectPath &handle,
                                     const QDBusObjectPath &session_handle,
                                     const QString &app_id,
                                     const QVariantMap &options,
                                     QVariantMap &results)
{
    Q_UNUSED(results)

    qCDebug(XdgDesktopDDEScreenCastProtal) << "SelectSource called with parameters:";
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    handle: " << handle.path();
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    app_id: " << app_id;
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    options: " << options;

    ScreenCastSession *session = qobject_cast<ScreenCastSession*>(Session::getSession(session_handle.path()));

    if (!session) {
        qCWarning(XdgDesktopDDEScreenCastProtal) << "Tried to select sources on non-existing session " << session_handle.path();
        return 2;
    }

    session->setMultipleSources(options.value(QStringLiteral("multiple")).toBool());
    if (options.contains(QStringLiteral("types"))) {
        session->setSourceTypes(SourceTypes(options.value(QStringLiteral("types")).toUInt()));
    }

    // Might be also a RemoteDesktopSession
    if (session->type() == Session::RemoteDesktop) {
        RemoteDesktopSession *remoteDesktopSession = qobject_cast<RemoteDesktopSession*>(session);
        if (remoteDesktopSession) {
            remoteDesktopSession->setScreenSharingEnabled(true);
        }
    }

    return 0;
}

uint ScreenCastPortal::Start(const QDBusObjectPath &handle,
                             const QDBusObjectPath &session_handle,
                             const QString &app_id,
                             const QString &parent_window,
                             const QVariantMap &options,
                             QVariantMap &results)
{
    Q_UNUSED(results)

    qCDebug(XdgDesktopDDEScreenCastProtal) << "Start called with parameters:";
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    handle: " << handle.path();
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    app_id: " << app_id;
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    parent_window: " << parent_window;
    qCDebug(XdgDesktopDDEScreenCastProtal) << "    options: " << options;

    ScreenCastSession *session = qobject_cast<ScreenCastSession*>(Session::getSession(session_handle.path()));

    if (!session) {
        qCWarning(XdgDesktopDDEScreenCastProtal) << "Tried to call start on non-existing session " << session_handle.path();
        return 2;
    }

    if (WaylandIntegration::screens().isEmpty()) {
        qCWarning(XdgDesktopDDEScreenCastProtal) << "Failed to show dialog as there is no screen to select";
        return 2;
    }

    QScopedPointer<ScreenChooserDialog, QScopedPointerDeleteLater> screenDialog(new ScreenChooserDialog(app_id, session->multipleSources()));
    Utils::setParentWindow(screenDialog.data(), parent_window);
    screenDialog->setSourceTypes(session->sourceTypes());
    connect(session, &Session::closed, screenDialog.data(), &ScreenChooserDialog::reject);

    if (screenDialog->exec()) {
        const auto selectedScreens = screenDialog->selectedScreens();
        for (quint32 outputid : selectedScreens) {
            if (!WaylandIntegration::startStreamingOutput(outputid, Screencasting::Hidden)) {
                return 2;
            }
        }
        const auto selectedWindows = screenDialog->selectedWindows();
        for (const QByteArray &winid : selectedWindows) {
            if (!WaylandIntegration::startStreamingWindow(winid)) {
                return 2;
            }
        }

        QVariant streams = WaylandIntegration::streams();

        if (!streams.isValid()) {
            qCWarning(XdgDesktopDDEScreenCastProtal) << "Pipewire stream is not ready to be streamed";
            return 2;
        }

        results.insert(QStringLiteral("streams"), streams);

        return 0;
    }

    return 1;
}
