// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "session.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDESession, "xdg-dde-session")

SessionPortal::SessionPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

void SessionPortal::Close()
{
    qCDebug(XdgDesktopDDESession) << "Closed";
}
