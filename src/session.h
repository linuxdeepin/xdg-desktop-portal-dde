// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QDBusObjectPath>
#include <QDBusAbstractAdaptor>
#include <QDBusVirtualObject>
#include <QAction>
#include <QDBusInterface>

#include "utils.h"

class KGlobalAccelInterface;
class KGlobalAccelComponentInterface;

class Session : public QDBusVirtualObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Session)
public:
    explicit Session(QObject *parent = nullptr, const QString &appId = QString(), const QString &path = QString());
    ~Session() override;

    enum SessionType {
        GlobalShortcuts = 1,
    };

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;
    QString introspect(const QString &path) const override;

    bool close();
    virtual SessionType type() const = 0;

    static Session *createSession(QObject *parent, SessionType type, const QString &appId, const QString &path);
    static Session *getSession(const QString &sessionHandle);

    QString handle() const { return m_path; }

Q_SIGNALS:
    void closed();

protected:
    const QString m_appId;
    const QString m_path;
};

class GlobalShortcutsSession : public Session
{
    Q_OBJECT
public:
    explicit GlobalShortcutsSession(QObject *parent, const QString &appId, const QString &path);
    ~GlobalShortcutsSession() override;

    SessionType type() const override { return SessionType::GlobalShortcuts; }

    void restoreActions(const QVariant &shortcurts);
    QHash<QString, QAction *> shortcuts() const { return m_shortcuts; }

    QVariant shortcutDescriptionsVariant() const;
    QString componentName() const { return m_appId + m_token; }
    Shortcuts shortcutDescriptions() const;

Q_SIGNALS:
    void shortcutsChanged();
    void shortcutActivated(const QString &shortcutName, qlonglong timestamp);
    void shortcutDeactivated(const QString &shortcutName, qlonglong timestamp);

private:
    const QString m_token;
    QHash<QString, QAction *> m_shortcuts;
    KGlobalAccelInterface *const m_globalAccelInterface;
    KGlobalAccelComponentInterface *const m_component;
};
