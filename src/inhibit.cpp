// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "inhibit.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDEInhibit, "xdg-dde-inhibit")

InhibitPortal::InhibitPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qCDebug(XdgDesktopDDEInhibit) << "Inhibit init";
}

void InhibitPortal::Inhibit(
    const QDBusObjectPath &handle, const QString &app_id, const QString &window, uint flags, const QVariantMap &options)
{
    qCDebug(XdgDesktopDDEInhibit) << "Handle: " << handle;
    qCDebug(XdgDesktopDDEInhibit) << app_id << "request Inhibit";
}
