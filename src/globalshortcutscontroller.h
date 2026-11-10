// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "globalshortcuttypes.h"

#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QSharedPointer>

class GlobalShortcutRequest;
class GlobalShortcutSession;
class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

class GlobalShortcutsController : public QObject
{
    Q_OBJECT

public:
    enum Response : uint {
        Success = 0,
        Cancelled = 1,
        OtherError = 2,
    };
    Q_ENUM(Response)

    explicit GlobalShortcutsController(QObject *parent = nullptr);

    uint createSession(const QDBusObjectPath &handle,
                       const QDBusObjectPath &sessionHandle,
                       const QString &appId,
                       const QVariantMap &options,
                       QVariantMap &results,
                       const QDBusMessage &message);
    uint bindShortcuts(const QDBusObjectPath &handle,
                       const QDBusObjectPath &sessionHandle,
                       const XdgShortcutList &shortcuts,
                       const QString &parentWindow,
                       const QVariantMap &options,
                       QVariantMap &results,
                       const QDBusMessage &message);
    uint listShortcuts(const QDBusObjectPath &handle,
                       const QDBusObjectPath &sessionHandle,
                       QVariantMap &results,
                       const QDBusMessage &message);
    void configureShortcuts(const QDBusObjectPath &sessionHandle,
                            const QString &parentWindow,
                            const QVariantMap &options,
                            const QDBusMessage &message);

    bool completeBind(const QString &requestId,
                      uint response,
                      const PortalShortcutUpdateList &selected,
                      const QDBusMessage &message,
                      QString *errorName,
                      QString *errorText);
    void applySessionShortcuts(const QDBusObjectPath &sessionHandle,
                               const PortalShortcutUpdateList &updates,
                               const QDBusMessage &message);
    void checkSessionShortcutConflicts(const QDBusObjectPath &sessionHandle,
                                       const PortalShortcutUpdateList &shortcuts,
                                       const QDBusMessage &message);

    GlobalShortcutSession *session(const QDBusObjectPath &handle) const;
    int sessionCount() const { return m_sessions.size(); }

Q_SIGNALS:
    void activated(const QDBusObjectPath &sessionHandle,
                   const QString &shortcutId,
                   qulonglong timestamp,
                   const QVariantMap &options);
    void deactivated(const QDBusObjectPath &sessionHandle,
                     const QString &shortcutId,
                     qulonglong timestamp,
                     const QVariantMap &options);
    void shortcutsChanged(const QDBusObjectPath &sessionHandle,
                          const XdgShortcutList &shortcuts);

private Q_SLOTS:
    void onPortalShortcutPressed(const QString &appId,
                                 const QString &shortcutId,
                                 qulonglong timestamp);
    void onPortalShortcutReleased(const QString &appId,
                                  const QString &shortcutId,
                                  qulonglong timestamp);
    void onPortalShortcutsChanged(const QString &appId,
                                  const PortalShortcutInfoList &shortcuts);
    void onPortalSessionShortcutsChanged(const QString &appId,
                                         const QString &sessionToken,
                                         const PortalShortcutInfoList &shortcuts);
    void onBrokerOwnerVanished(const QString &service);
    void onProviderVanished();
    void onServiceVanished();
    void replayBoundSessions();

private:
    enum class BindPhase {
        LoadingHistory,
        Normalizing,
        WaitingProvider,
        Committing,
    };

    struct PendingBind {
        QString requestId;
        QString requestPath;
        QString sessionPath;
        QString appId;
        QString sessionToken;
        QString parentWindow;
        QDBusMessage replyMessage;
        QPointer<GlobalShortcutRequest> request;
        QPointer<GlobalShortcutSession> session;
        XdgShortcutList declared;
        PortalShortcutInfoList history;
        PortalShortcutUpdateList updates;
        QSet<QString> pendingNewIds;
        BindPhase phase = BindPhase::LoadingHistory;
        bool requestClosed = false;
        bool sessionClosed = false;
    };

    struct PendingList {
        QString requestPath;
        QDBusMessage replyMessage;
        QPointer<GlobalShortcutRequest> request;
        QPointer<GlobalShortcutSession> session;
        bool closed = false;
    };

    static constexpr qsizetype MaximumShortcutsPerApplication = 100;
    static constexpr qsizetype MaximumActiveSessions = 256;
    static constexpr qsizetype MaximumPendingRequests = 256;
    static constexpr qsizetype MaximumAppIdLength = 255;
    static constexpr qsizetype MaximumShortcutIdLength = 128;
    static constexpr qsizetype MaximumDescriptionLength = 256;
    static constexpr qsizetype MaximumPreferredTriggerLength = 128;
    static constexpr qsizetype MaximumParentWindowLength = 4096;
    static constexpr qsizetype MaximumActivationTokenLength = 4096;

    QString caller(const QDBusMessage &message) const;
    bool verifyBrokerCaller(const QDBusMessage &message) const;
    GlobalShortcutSession *checkedSession(const QDBusObjectPath &handle,
                                          const QDBusMessage &message) const;
    bool validateShortcuts(const XdgShortcutList &shortcuts) const;
    bool validateSessionUpdates(const GlobalShortcutSession *session,
                                const PortalShortcutUpdateList &updates) const;
    bool verifyProviderCaller(const QDBusMessage &message,
                              QString *errorName,
                              QString *errorText) const;
    GlobalShortcutRequest *createRequest(const QDBusObjectPath &handle,
                                         const QString &owner);
    void closeSession(GlobalShortcutSession *session);
    void closeRequest(GlobalShortcutRequest *request);
    void cancelBind(const QString &requestId, bool closeProvider);
    void startBindHistory(const QString &requestId);
    void startTriggerNormalization(const QString &requestId);
    void continueBindAfterNormalization(const QString &requestId,
                                        const PortalTriggerList &normalized);
    void startProviderBind(const QString &requestId);
    void commitBind(const QString &requestId);
    void finishBind(const QString &requestId,
                    uint response,
                    const PortalShortcutInfoList &bound = {});
    void finishList(const QString &requestPath,
                    uint response,
                    const PortalShortcutInfoList &shortcuts = {});
    void callDeactivate(const QString &appId, const QString &sessionToken);
    void cancelProviderRequest(const QString &requestId);
    void cancelProviderSession(const QString &sessionPath);
    void sendBackendReply(const QDBusMessage &message,
                          uint response,
                          const QVariantMap &results) const;
    void sendVoidReply(const QDBusMessage &message) const;
    void sendError(const QDBusMessage &message,
                   const QString &name,
                   const QString &text) const;
    void sendTypedReply(const QDBusMessage &message, const QVariant &result) const;
    XdgShortcutList toXdgShortcuts(const PortalShortcutInfoList &shortcuts,
                                   const GlobalShortcutSession *session,
                                   bool requireActive) const;
    PortalShortcutUpdateList initialUpdates(const XdgShortcutList &shortcuts) const;
    static QString triggerDescription(const PortalShortcutInfo &shortcut);
    static QHash<QString, PortalShortcutInfo> byShortcutId(
            const PortalShortcutInfoList &shortcuts);
    void publishSessionShortcuts(GlobalShortcutSession *session,
                                 const XdgShortcutList &shortcuts);
    void replayNextSession(quint64 generation, int retryCount = 0);

    QHash<QString, QPointer<GlobalShortcutSession>> m_sessions;
    QHash<QString, QSharedPointer<PendingBind>> m_bindRequests;
    QHash<QString, QString> m_bindRequestByPath;
    QHash<QString, QSharedPointer<PendingList>> m_listRequests;
    QHash<QString, QPointer<GlobalShortcutSession>> m_configureRequests;
    QHash<QString, PortalShortcutUpdateList> m_sessionCommitRequests;
    QHash<QString, QSet<QString>> m_sessionReplayIds;
    QHash<QString, QSet<QString>> m_sessionPressedIds;
    QQueue<QPointer<GlobalShortcutSession>> m_replayQueue;
    bool m_replayInProgress = false;
    quint64 m_serviceGeneration = 0;
    QDBusServiceWatcher *m_brokerWatcher = nullptr;
    QDBusServiceWatcher *m_providerWatcher = nullptr;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
};
