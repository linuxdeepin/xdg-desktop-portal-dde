// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "accessdialogtypes.h"

#include <QDBusAbstractAdaptor>
#include <QDBusMessage>
#include <QDBusObjectPath>

class AccessPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Access")

public:
    explicit AccessPortal(QObject *parent);
    ~AccessPortal() = default;

public Q_SLOTS:
    void AccessDialog(const QDBusObjectPath &handle,
                      const QString &app_id,
                      const QString &parent_window,
                      const QString &title,
                      const QString &subtitle,
                      const QString &body,
                      const QVariantMap &options,
                      const QDBusMessage &message,
                      uint &replyResponse,
                      QVariantMap &replyResults);
};
