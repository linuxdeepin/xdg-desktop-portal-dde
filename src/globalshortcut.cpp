// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "globalshortcut.h"

#include "session.h"

#include <QLoggingCategory>
#include <QDBusMetaType>

Q_LOGGING_CATEGORY(XdgDesktopDDEGlobalShortCut, "xdg-dde-global-shortcut")

GlobalShortcutPortal::GlobalShortcutPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qCDebug(XdgDesktopDDEGlobalShortCut) << "init global shortcut";
    qDBusRegisterMetaType<Shortcut>();
    qDBusRegisterMetaType<Shortcuts>();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    Q_ASSERT(QLatin1String(QDBusMetaType::typeToSignature(qMetaTypeId<Shortcuts>())) == QLatin1String("a(sa{sv})"));
#else
    Q_ASSERT(QLatin1String(QDBusMetaType::typeToSignature(QMetaType(qMetaTypeId<Shortcuts>()))) == QLatin1String("a(sa{sv})"));
#endif
}

uint GlobalShortcutPortal::CreateSession(const QDBusObjectPath &handle,
                                         const QDBusObjectPath &session_handle,
                                         const QString &app_id,
                                         const QVariantMap &options,
                                         QVariantMap &results)
{
    qCDebug(XdgDesktopDDEGlobalShortCut) << "create session";
    qCDebug(XdgDesktopDDEGlobalShortCut) << "CreateSession called with parameters:";
    qCDebug(XdgDesktopDDEGlobalShortCut) << "    handle: " << handle.path();
    qCDebug(XdgDesktopDDEGlobalShortCut) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopDDEGlobalShortCut) << "    app_id: " << app_id;
    qCDebug(XdgDesktopDDEGlobalShortCut) << "    options: " << options;

    if (auto session = qobject_cast<GlobalShortcutsSession *>(
            Session::createSession(this, Session::GlobalShortcuts, app_id, session_handle.path()))) {
        session->restoreActions(options["shortcuts"]);
        connect(session, &GlobalShortcutsSession::shortcutsChanged, this, [this, session, session_handle] {
            Q_EMIT ShortcutsChanged(session_handle, session->shortcutDescriptions());
        });

        connect(session,
                &GlobalShortcutsSession::shortcutActivated,
                this,
                [this, session](const QString &shortcutName, qlonglong timestamp) {
                    Q_EMIT Activated(QDBusObjectPath(session->handle()), shortcutName, timestamp);
                });
        connect(session,
                &GlobalShortcutsSession::shortcutDeactivated,
                this,
                [this, session](const QString &shortcutName, qlonglong timestamp) {
                    Q_EMIT Deactivated(QDBusObjectPath(session->handle()), shortcutName, timestamp);
                });

        results = {
            {"shortcuts", session->shortcutDescriptionsVariant()},
        };
        return 0;
    }

    return 2;
}

uint GlobalShortcutPortal::BindShortCuts(const QDBusObjectPath &handle,
                                         const QDBusObjectPath &session_handle,
                                         const QVariantMap &shortcuts,
                                         const QString &parent_window,
                                         const QVariantMap &options,
                                         QVariantMap &results)
{
    qCDebug(XdgDesktopDDEGlobalShortCut) << "BindShortcuts called with parameters:";
    qCDebug(XdgDesktopDDEGlobalShortCut) << "    handle: " << handle.path();
    qCDebug(XdgDesktopDDEGlobalShortCut) << "    session_handle: " << session_handle.path();
    qCDebug(XdgDesktopDDEGlobalShortCut) << "    shortcuts: " << shortcuts;
    qCDebug(XdgDesktopDDEGlobalShortCut) << "    parent_window: " << parent_window;
    qCDebug(XdgDesktopDDEGlobalShortCut) << "    options: " << options;

    if (auto session = qobject_cast<GlobalShortcutsSession *>(Session::getSession(session_handle.path()))) {
        // TODO: show dialog to bind a shortcut

        results = {
            {"shortcuts", session->shortcutDescriptionsVariant()},
        };

        return 0;
    }

    return 2;
}

uint GlobalShortcutPortal::ListShortCuts(const QDBusObjectPath &handle,
                                         const QDBusObjectPath &session_handle,
                                         QVariantMap &results)
{
    qCDebug(XdgDesktopDDEGlobalShortCut) << "ListShortcuts called with parameters:";
    qCDebug(XdgDesktopDDEGlobalShortCut) << "    handle: " << handle.path();
    qCDebug(XdgDesktopDDEGlobalShortCut) << "    session_handle: " << session_handle.path();

    if (auto session = qobject_cast<GlobalShortcutsSession *>(Session::getSession(session_handle.path()))) {
        results = {
            {"shortcuts", session->shortcutDescriptionsVariant()},
        };

        return 0;
    }

    return 2;
}
