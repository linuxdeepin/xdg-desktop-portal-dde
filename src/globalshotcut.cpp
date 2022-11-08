// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "globalshotcut.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDEGlobalShotCut, "xdg-dde-globalshotcut")

GlobalShotcutProtal::GlobalShotcutProtal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qCDebug(XdgDesktopDDEGlobalShotCut) << "init globalshotcut";
}

uint GlobalShotcutProtal::CreateSession(const QDBusObjectPath &handle,
                                        const QDBusObjectPath &session_handle,
                                        const QString &app_id,
                                        const QVariantMap &options,
                                        QVariantMap &results)
{
    qCDebug(XdgDesktopDDEGlobalShotCut) << "create session";
    return 1;
}

QVariantMap GlobalShotcutProtal::BindShortCuts(const QDBusObjectPath &handle,
                                               const QDBusObjectPath &session_handle,
                                               const QVariantMap &shortcuts,
                                               const QString &parent_window,
                                               const QVariantMap &options)
{
    qCDebug(XdgDesktopDDEGlobalShotCut) << "BindShortCuts";
    return QVariantMap();
}

QVariantMap GlobalShotcutProtal::ListShortCuts(const QDBusObjectPath &handle, const QDBusObjectPath &session_handle)
{
    qCDebug(XdgDesktopDDEGlobalShotCut) << "get ShortCuts";
    return QVariantMap();
}
