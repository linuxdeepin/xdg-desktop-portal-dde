// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "account.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDestkopDDEAcount, "xdg-dde-account")

AccountPortal::AccountPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qCDebug(XdgDestkopDDEAcount) << "account init";
}

uint AccountPortal::GetUserInformation(
    const QDBusObjectPath &handle, const QString &app_id, const QString &window, const QVariantMap &options, QVariantMap &results)
{
    qCDebug(XdgDestkopDDEAcount) << "request for account";
    return 1;
}
