// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "globalshortcuttypes.h"

#include <QDBusObjectPath>
#include <QHash>
#include <QObject>
#include <QSet>

#include <functional>
#include <utility>

class QDBusMessage;

class GlobalShortcutSession : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Session")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    enum class State {
        Created,
        BindPending,
        Bound,
        DeclaredOnly,
        Closed,
    };
    Q_ENUM(State)

    explicit GlobalShortcutSession(const QDBusObjectPath &handle,
                                   const QString &appId,
                                   const QString &brokerOwner,
                                   QObject *parent = nullptr);
    ~GlobalShortcutSession() override;

    bool registerObject();
    uint version() const { return 1; }
    QString path() const { return m_handle.path(); }
    QString appId() const { return m_appId; }
    QString brokerOwner() const { return m_brokerOwner; }
    QString sessionToken() const { return m_sessionToken; }
    State state() const { return m_state; }
    bool bindAttempted() const { return m_bindAttempted; }
    const QStringList &declaredOrder() const { return m_declaredOrder; }
    const QHash<QString, QVariantMap> &declaredProperties() const { return m_declaredProperties; }
    const QSet<QString> &boundIds() const { return m_boundIds; }

    void beginBind(const XdgShortcutList &shortcuts);
    void finishBind(const XdgShortcutList &boundShortcuts);
    void updateBoundShortcuts(const XdgShortcutList &boundShortcuts);
    void failBind();
    XdgShortcutList activeShortcuts() const;
    void setCloseHandler(std::function<void(GlobalShortcutSession *)> handler)
    {
        m_closeHandler = std::move(handler);
    }

public Q_SLOTS:
    Q_SCRIPTABLE void Close(const QDBusMessage &message);

Q_SIGNALS:
    Q_SCRIPTABLE void Closed();

private:
    QDBusObjectPath m_handle;
    QString m_appId;
    QString m_brokerOwner;
    QString m_sessionToken;
    State m_state = State::Created;
    bool m_bindAttempted = false;
    bool m_registered = false;
    QStringList m_declaredOrder;
    QHash<QString, QVariantMap> m_declaredProperties;
    QSet<QString> m_boundIds;
    QHash<QString, QVariantMap> m_boundProperties;
    std::function<void(GlobalShortcutSession *)> m_closeHandler;
};
