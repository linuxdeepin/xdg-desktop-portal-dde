// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QAction>
#include <QDBusPendingReply>
#include <QDBusVirtualObject>
#include <QObject>

class Session : public QDBusVirtualObject
{
    Q_OBJECT
public:
    explicit Session(const QString &appId = QString(),
                     const QString &path = QString(),
                     QObject *parent = nullptr);
    ~Session() override;

    enum SessionType {
        ScreenCast = 0,
        RemoteDesktop = 1,
        GlobalShortcuts = 2,
        InputCapture = 3,
    };

    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override;
    QString introspect(const QString &path) const override;

    bool close();
    virtual SessionType type() const = 0;

    static Session *createSession(QObject *parent,
                                  SessionType type,
                                  const QString &appId,
                                  const QString &path);
    static Session *getSession(const QString &sessionHandle);
    template<typename T>
    static T *getSession(const QString &sessionHandle)
    {
        return qobject_cast<T *>(getSession(sessionHandle));
    }

    QString handle() const { return m_path; }
    QString appId() const { return m_appId; }

Q_SIGNALS:
    void closed();

protected:
    const QString m_appId;
    const QString m_path;
};
