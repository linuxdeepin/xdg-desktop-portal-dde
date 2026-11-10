// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "globalshortcut.h"
#include "globalshortcutscontroller.h"
#include "globalshortcutsession.h"
#include "globalshortcuttypes.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QXmlStreamReader>

#include <algorithm>
#include <functional>
#include <utility>

namespace {

constexpr auto BackendService = "org.freedesktop.impl.portal.desktop.dde";
constexpr auto BackendPath = "/org/freedesktop/portal/desktop";
constexpr auto BackendInterface = "org.freedesktop.impl.portal.GlobalShortcuts";
constexpr auto PortalBrokerService = "org.freedesktop.portal.Desktop";
constexpr auto ShortcutService = "org.deepin.dde.Keybinding1";
constexpr auto ShortcutServicePath = "/org/deepin/dde/Keybinding1";
constexpr auto ShortcutServiceInterface = "org.deepin.dde.Keybinding1";
constexpr auto ProviderService = "org.deepin.dde.GlobalShortcutsProvider1";
constexpr auto ManagementInterface =
        "org.deepin.dde.impl.portal.GlobalShortcutsManagement1";

PortalShortcutInfo shortcutRecord(const QString &appId,
                                  const QString &shortcutId,
                                  bool active = true)
{
    PortalShortcutInfo record;
    record.id = QStringLiteral("portal-test-") + shortcutId;
    record.appId = appId;
    record.shortcutId = shortcutId;
    record.description = QStringLiteral("Test action");
    record.hotkeys = {QStringLiteral("Ctrl+K")};
    record.enabled = true;
    record.active = active;
    return record;
}

XdgShortcutList declaredShortcuts(const QString &shortcutId)
{
    return {{shortcutId,
             {{QStringLiteral("description"), QStringLiteral("Test action")}}}};
}

QDBusPendingCall asyncCall(const QDBusConnection &connection,
                           const QString &path,
                           const QString &interface,
                           const QString &method,
                           const QVariantList &arguments = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(
            QString::fromLatin1(BackendService), path, interface, method);
    message.setArguments(arguments);
    return connection.asyncCall(message);
}

struct InterfaceContract
{
    QHash<QString, QString> methods;
    QHash<QString, QString> signalEntries;
    QHash<QString, QString> properties;
};

InterfaceContract readInterface(const QString &xmlText, const QString &interfaceName)
{
    QXmlStreamReader xml(xmlText);
    InterfaceContract contract;
    bool inInterface = false;
    QString currentKind;
    QString currentName;
    QString inputSignature;
    QString outputSignature;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QStringView element = xml.name();
            if (element == u"interface") {
                inInterface = xml.attributes().value(u"name") == interfaceName;
            } else if (inInterface && (element == u"method" || element == u"signal")) {
                currentKind = element.toString();
                currentName = xml.attributes().value(u"name").toString();
                inputSignature.clear();
                outputSignature.clear();
            } else if (inInterface && element == u"arg" && !currentKind.isEmpty()) {
                const QString type = xml.attributes().value(u"type").toString();
                const QString direction = xml.attributes().value(u"direction").toString();
                if (currentKind == u"signal" || direction == u"out")
                    outputSignature += type;
                else
                    inputSignature += type;
            } else if (inInterface && element == u"property") {
                contract.properties.insert(xml.attributes().value(u"name").toString(),
                                           xml.attributes().value(u"type").toString());
            }
        } else if (xml.isEndElement()) {
            const QStringView element = xml.name();
            if (inInterface && element == u"method" && currentKind == u"method") {
                contract.methods.insert(currentName,
                                        inputSignature + QStringLiteral("->") + outputSignature);
                currentKind.clear();
            } else if (inInterface && element == u"signal" && currentKind == u"signal") {
                contract.signalEntries.insert(currentName, outputSignature);
                currentKind.clear();
            } else if (element == u"interface" && inInterface) {
                inInterface = false;
            }
        }
    }
    return contract;
}

} // namespace

class FakeShortcutService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.dde.Keybinding1")

public:
    explicit FakeShortcutService(const QDBusConnection &connection, QObject *parent = nullptr)
        : QObject(parent)
        , m_connection(connection)
    {
    }

    void reset()
    {
        m_history.clear();
        m_commitResult.clear();
        m_applyResult.clear();
        m_restoreResult.clear();
        m_delayList = false;
        m_delayCommit = false;
        m_delayRestore = false;
        m_busyRestoreRemaining = 0;
        m_listCalls = 0;
        m_commitCalls = 0;
        m_applyCalls = 0;
        m_restoreCalls = 0;
        m_deactivateCalls = 0;
        m_listMessages.clear();
        m_commitMessage = {};
        m_restoreMessage = {};
        m_lastRestoredDeclared.clear();
        m_lastRestoredActiveIds.clear();
        m_lastRestoreToken.clear();
    }

    void setHistory(const PortalShortcutInfoList &history) { m_history = history; }
    void setCommitResult(const PortalShortcutInfoList &result) { m_commitResult = result; }
    void setApplyResult(const PortalShortcutInfoList &result) { m_applyResult = result; }
    void setRestoreResult(const PortalShortcutInfoList &result) { m_restoreResult = result; }
    void setDelayList(bool delay) { m_delayList = delay; }
    void setDelayCommit(bool delay) { m_delayCommit = delay; }
    void setDelayRestore(bool delay) { m_delayRestore = delay; }
    void setBusyRestores(int count) { m_busyRestoreRemaining = count; }
    int listCalls() const { return m_listCalls; }
    int commitCalls() const { return m_commitCalls; }
    int applyCalls() const { return m_applyCalls; }
    int restoreCalls() const { return m_restoreCalls; }
    int deactivateCalls() const { return m_deactivateCalls; }
    PortalShortcutUpdateList lastRestoredDeclared() const { return m_lastRestoredDeclared; }
    QStringList lastRestoredActiveIds() const { return m_lastRestoredActiveIds; }
    QString lastRestoreToken() const { return m_lastRestoreToken; }

    void replyList()
    {
        const QList<QDBusMessage> messages = std::exchange(m_listMessages, {});
        for (const QDBusMessage &message : messages) {
            if (message.type() == QDBusMessage::MethodCallMessage)
                m_connection.send(message.createReply({QVariant::fromValue(m_history)}));
        }
    }

    void replyNextList(const PortalShortcutInfoList &history)
    {
        if (m_listMessages.isEmpty())
            return;
        const QDBusMessage message = m_listMessages.takeFirst();
        m_connection.send(message.createReply({QVariant::fromValue(history)}));
    }

    void replyCommit()
    {
        if (m_commitMessage.type() != QDBusMessage::MethodCallMessage)
            return;
        m_connection.send(m_commitMessage.createReply({QVariant::fromValue(m_commitResult)}));
        m_commitMessage = {};
    }

    void replyRestore(const PortalShortcutInfoList &result)
    {
        if (m_restoreMessage.type() != QDBusMessage::MethodCallMessage)
            return;
        m_connection.send(m_restoreMessage.createReply({QVariant::fromValue(result)}));
        m_restoreMessage = {};
    }

    void sendPressed(const QString &appId, const QString &shortcutId, qulonglong timestamp)
    {
        Q_EMIT PortalShortcutPressed(appId, shortcutId, timestamp);
    }

    void sendChanged(const QString &appId, const PortalShortcutInfoList &shortcuts)
    {
        Q_EMIT PortalShortcutsChanged(appId, shortcuts);
    }

    void sendSessionChanged(const QString &appId, const QString &token,
                            const PortalShortcutInfoList &shortcuts)
    {
        Q_EMIT PortalSessionShortcutsChanged(appId, token, shortcuts);
    }

    void sendReleased(const QString &appId, const QString &shortcutId, qulonglong timestamp)
    {
        Q_EMIT PortalShortcutReleased(appId, shortcutId, timestamp);
    }

public Q_SLOTS:
    Q_SCRIPTABLE PortalShortcutInfoList ListPortalShortcuts(const QString &appId)
    {
        Q_UNUSED(appId)
        ++m_listCalls;
        if (m_delayList) {
            setDelayedReply(true);
            m_listMessages.append(message());
            return {};
        }
        return m_history;
    }

    Q_SCRIPTABLE PortalTriggerList NormalizeXdgPortalTriggers(
            const PortalTriggerList &requested)
    {
        return requested;
    }

    Q_SCRIPTABLE PortalShortcutInfoList CommitPortalSessionBindings(
            const QString &appId,
            const QString &sessionToken,
            const PortalShortcutUpdateList &requested)
    {
        Q_UNUSED(appId)
        Q_UNUSED(sessionToken)
        Q_UNUSED(requested)
        ++m_commitCalls;
        if (m_delayCommit) {
            setDelayedReply(true);
            m_commitMessage = message();
            return {};
        }
        return m_commitResult;
    }

    Q_SCRIPTABLE void DeactivatePortalShortcuts(const QString &appId,
                                                const QString &sessionToken)
    {
        Q_UNUSED(appId)
        Q_UNUSED(sessionToken)
        ++m_deactivateCalls;
    }

    Q_SCRIPTABLE PortalShortcutInfoList ApplyPortalSessionUpdates(
            const QString &appId,
            const QString &sessionToken,
            const PortalShortcutUpdateList &updates)
    {
        Q_UNUSED(updates)
        ++m_applyCalls;
        Q_EMIT PortalSessionShortcutsChanged(appId, sessionToken, m_applyResult);
        Q_EMIT PortalShortcutsChanged(appId, m_applyResult);
        return m_applyResult;
    }

    Q_SCRIPTABLE PortalShortcutInfoList RestorePortalSessionBindings(
            const QString &appId,
            const QString &sessionToken,
            const PortalShortcutUpdateList &declared,
            const QStringList &activeShortcutIds)
    {
        Q_UNUSED(appId)
        ++m_restoreCalls;
        m_lastRestoredDeclared = declared;
        m_lastRestoredActiveIds = activeShortcutIds;
        m_lastRestoreToken = sessionToken;
        if (m_busyRestoreRemaining > 0) {
            --m_busyRestoreRemaining;
            sendErrorReply(QStringLiteral("org.deepin.dde.Keybinding1.Error.Busy"),
                           QStringLiteral("Injected busy write"));
            return {};
        }
        if (m_delayRestore) {
            setDelayedReply(true);
            m_restoreMessage = message();
            return {};
        }
        return m_restoreResult;
    }

    Q_SCRIPTABLE PortalShortcutConflictList CheckPortalShortcutConflicts(
            const QString &appId,
            const PortalShortcutUpdateList &shortcuts)
    {
        Q_UNUSED(appId)
        Q_UNUSED(shortcuts)
        return {};
    }

Q_SIGNALS:
    Q_SCRIPTABLE void PortalShortcutPressed(const QString &appId,
                                            const QString &shortcutId,
                                            qulonglong timestamp);
    Q_SCRIPTABLE void PortalShortcutReleased(const QString &appId,
                                             const QString &shortcutId,
                                             qulonglong timestamp);
    Q_SCRIPTABLE void PortalShortcutsChanged(const QString &appId,
                                             const PortalShortcutInfoList &shortcuts);
    Q_SCRIPTABLE void PortalSessionShortcutsChanged(const QString &appId,
                                                    const QString &sessionToken,
                                                    const PortalShortcutInfoList &shortcuts);

private:
    QDBusConnection m_connection;
    PortalShortcutInfoList m_history;
    PortalShortcutInfoList m_commitResult;
    PortalShortcutInfoList m_applyResult;
    PortalShortcutInfoList m_restoreResult;
    bool m_delayList = false;
    bool m_delayCommit = false;
    bool m_delayRestore = false;
    int m_busyRestoreRemaining = 0;
    int m_listCalls = 0;
    int m_commitCalls = 0;
    int m_applyCalls = 0;
    int m_restoreCalls = 0;
    int m_deactivateCalls = 0;
    QList<QDBusMessage> m_listMessages;
    QDBusMessage m_commitMessage;
    QDBusMessage m_restoreMessage;
    PortalShortcutUpdateList m_lastRestoredDeclared;
    QStringList m_lastRestoredActiveIds;
    QString m_lastRestoreToken;
};

class TestGlobalShortcutsController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void exportedIntrospectionMatchesContracts();
    void rejectsNonBrokerCaller();
    void sessionCloseFinishesPendingBind();
    void sessionCloseDuringCommitRepliesOnceAndDeactivatesLateSuccess();
    void listBeforeBindOmitsDisabledHistory();
    void requestCloseCompletesPrivateBindWithoutPublicResponse();
    void requestCloseCompletesPrivateListWithoutPublicResponse();
    void invalidParentDoesNotRetainOversizedDeclarations();
    void acceptsSupplementaryUnicodeDeclarations();
    void cancelledListCannotCompleteReusedRequestPath();
    void serviceSignalsReachBoundSession();
    void configureUpdatesAreScopedAndDeduplicated();
    void applicationSnapshotDoesNotPromoteUnboundSession();
    void replaySerializesAndRetriesBusy();
    void replayBusyRetriesAreBounded();
    void closeDuringReplayContinuesQueue();
    void configureInFlightIsBounded();
    void serviceRestartRestoresOnlyPreviouslyBoundSubset();
    void rapidServiceRestartPreservesReplaySetAndIgnoresOldReply();
    void activeSessionLimitIsEnforced();
    void pendingRequestLimitIsEnforced();

private:
    void createSession(const QString &requestPath,
                       const QString &sessionPath,
                       const QString &appId);
    void closeSession(const QString &sessionPath);
    void bindSession(const QString &sessionPath, const XdgShortcutList &declared);
    void withService(const std::function<void(FakeShortcutService *)> &operation) const;
    int serviceValue(const std::function<int(FakeShortcutService *)> &operation) const;

    QDBusConnection m_serviceConnection{QStringLiteral("shortcut-controller-service")};
    QDBusConnection m_brokerConnection{QStringLiteral("shortcut-controller-broker")};
    QDBusConnection m_providerConnection{QStringLiteral("shortcut-controller-provider")};
    QThread m_serviceThread;
    FakeShortcutService *m_service = nullptr;
    QObject *m_backendRoot = nullptr;
    GlobalShortcutPortal *m_portal = nullptr;
};

void TestGlobalShortcutsController::initTestCase()
{
    registerGlobalShortcutMetaTypes();

    m_serviceConnection = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, QStringLiteral("shortcut-controller-service"));
    m_brokerConnection = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, QStringLiteral("shortcut-controller-broker"));
    m_providerConnection = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, QStringLiteral("shortcut-controller-provider"));
    QVERIFY(m_serviceConnection.isConnected());
    QVERIFY(m_brokerConnection.isConnected());
    QVERIFY(m_providerConnection.isConnected());

    m_service = new FakeShortcutService(m_serviceConnection);
    m_service->moveToThread(&m_serviceThread);
    connect(&m_serviceThread, &QThread::finished, m_service, &QObject::deleteLater);
    m_serviceThread.start();
    QVERIFY(m_serviceConnection.registerService(QString::fromLatin1(ShortcutService)));
    bool serviceObjectRegistered = false;
    QVERIFY(QMetaObject::invokeMethod(
            m_service,
            [this, &serviceObjectRegistered] {
                serviceObjectRegistered = m_serviceConnection.registerObject(
                        QString::fromLatin1(ShortcutServicePath),
                        m_service,
                        QDBusConnection::ExportScriptableContents);
            },
            Qt::BlockingQueuedConnection));
    QVERIFY(serviceObjectRegistered);
    QVERIFY(m_brokerConnection.registerService(QString::fromLatin1(PortalBrokerService)));
    QVERIFY(m_providerConnection.registerService(QString::fromLatin1(ProviderService)));

    QDBusConnection backendConnection = QDBusConnection::sessionBus();
    QVERIFY(backendConnection.registerService(QString::fromLatin1(BackendService)));
    m_backendRoot = new QObject(this);
    m_portal = new GlobalShortcutPortal(m_backendRoot);
    QVERIFY(backendConnection.registerObject(QString::fromLatin1(BackendPath),
                                             m_backendRoot,
                                             QDBusConnection::ExportAdaptors));
}

void TestGlobalShortcutsController::cleanupTestCase()
{
    QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(BackendPath));
    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(BackendService));
    withService([this](FakeShortcutService *) {
        m_serviceConnection.unregisterObject(QString::fromLatin1(ShortcutServicePath));
    });
    m_serviceConnection.unregisterService(QString::fromLatin1(ShortcutService));
    m_brokerConnection.unregisterService(QString::fromLatin1(PortalBrokerService));
    m_providerConnection.unregisterService(QString::fromLatin1(ProviderService));
    m_serviceThread.quit();
    QVERIFY(m_serviceThread.wait(3000));
    m_service = nullptr;
    QDBusConnection::disconnectFromBus(QStringLiteral("shortcut-controller-broker"));
    QDBusConnection::disconnectFromBus(QStringLiteral("shortcut-controller-provider"));
    QDBusConnection::disconnectFromBus(QStringLiteral("shortcut-controller-service"));
}

void TestGlobalShortcutsController::init()
{
    withService([](FakeShortcutService *service) { service->reset(); });
    QCOMPARE(m_portal->controller()->sessionCount(), 0);
}

void TestGlobalShortcutsController::withService(
        const std::function<void(FakeShortcutService *)> &operation) const
{
    QVERIFY(QMetaObject::invokeMethod(
            m_service,
            [service = m_service, operation] { operation(service); },
            Qt::BlockingQueuedConnection));
}

int TestGlobalShortcutsController::serviceValue(
        const std::function<int(FakeShortcutService *)> &operation) const
{
    int value = 0;
    const bool invoked = QMetaObject::invokeMethod(
            m_service,
            [service = m_service, operation, &value] { value = operation(service); },
            Qt::BlockingQueuedConnection);
    Q_ASSERT(invoked);
    return value;
}

void TestGlobalShortcutsController::createSession(const QString &requestPath,
                                                  const QString &sessionPath,
                                                  const QString &appId)
{
    QDBusPendingCallWatcher watcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("CreateSession"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath)),
             appId,
             QVariantMap{}}));
    QTRY_VERIFY_WITH_TIMEOUT(watcher.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> reply = watcher;
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    QCOMPARE(reply.argumentAt<0>(), uint(GlobalShortcutsController::Success));
}

void TestGlobalShortcutsController::closeSession(const QString &sessionPath)
{
    const int remainingSessions = m_portal->controller()->sessionCount() - 1;
    QVERIFY(m_portal->controller()->session(QDBusObjectPath(sessionPath)));
    QDBusPendingCallWatcher watcher(asyncCall(
            m_brokerConnection,
            sessionPath,
            QStringLiteral("org.freedesktop.impl.portal.Session"),
            QStringLiteral("Close")));
    QTRY_VERIFY_WITH_TIMEOUT(watcher.isFinished(), 3000);
    QDBusPendingReply<> reply = watcher;
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    QTRY_COMPARE(m_portal->controller()->sessionCount(), remainingSessions);
    QVERIFY(!m_portal->controller()->session(QDBusObjectPath(sessionPath)));
}

void TestGlobalShortcutsController::bindSession(const QString &sessionPath,
                                               const XdgShortcutList &declared)
{
    QDBusPendingCallWatcher watcher(asyncCall(
            m_brokerConnection, QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface), QStringLiteral("BindShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(sessionPath + QStringLiteral("_request"))),
             QVariant::fromValue(QDBusObjectPath(sessionPath)), QVariant::fromValue(declared),
             QString(), QVariantMap{}}));
    QTRY_VERIFY_WITH_TIMEOUT(watcher.isFinished(), 3000);
    const QDBusPendingReply<uint, QVariantMap> reply = watcher;
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    QCOMPARE(reply.argumentAt<0>(), uint(GlobalShortcutsController::Success));
}

void TestGlobalShortcutsController::exportedIntrospectionMatchesContracts()
{
    QDBusPendingCallWatcher watcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QStringLiteral("org.freedesktop.DBus.Introspectable"),
            QStringLiteral("Introspect")));
    QTRY_VERIFY_WITH_TIMEOUT(watcher.isFinished(), 3000);
    QDBusPendingReply<QString> reply = watcher;
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));

    const InterfaceContract backend = readInterface(
            reply.value(), QString::fromLatin1(BackendInterface));
    QCOMPARE(backend.methods.value(QStringLiteral("CreateSession")),
             QStringLiteral("oosa{sv}->ua{sv}"));
    QCOMPARE(backend.methods.value(QStringLiteral("BindShortcuts")),
             QStringLiteral("ooa(sa{sv})sa{sv}->ua{sv}"));
    QCOMPARE(backend.methods.value(QStringLiteral("ListShortcuts")),
             QStringLiteral("oo->ua{sv}"));
    QCOMPARE(backend.methods.value(QStringLiteral("ConfigureShortcuts")),
             QStringLiteral("osa{sv}->"));
    QCOMPARE(backend.signalEntries.value(QStringLiteral("Activated")),
             QStringLiteral("osta{sv}"));
    QCOMPARE(backend.signalEntries.value(QStringLiteral("Deactivated")),
             QStringLiteral("osta{sv}"));
    QCOMPARE(backend.signalEntries.value(QStringLiteral("ShortcutsChanged")),
             QStringLiteral("oa(sa{sv})"));
    QCOMPARE(backend.properties.value(QStringLiteral("version")), QStringLiteral("u"));

    const InterfaceContract management = readInterface(
            reply.value(),
            QStringLiteral("org.deepin.dde.impl.portal.GlobalShortcutsManagement1"));
    QCOMPARE(management.methods.value(QStringLiteral("CompleteBind")),
             QStringLiteral("sua(ssasasb)->"));
    QCOMPARE(management.methods.value(QStringLiteral("ApplySessionShortcuts")),
             QStringLiteral("oa(ssasasb)->a(ssssasasbb)"));
    QCOMPARE(management.methods.value(QStringLiteral("CheckSessionShortcutConflicts")),
             QStringLiteral("oa(ssasasb)->a(sssss)"));
    QCOMPARE(management.properties.value(QStringLiteral("version")), QStringLiteral("u"));

    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/introspection");
    createSession(QStringLiteral(
                          "/org/freedesktop/portal/desktop/request/test/introspection_create"),
                  sessionPath,
                  QStringLiteral("org.example.Introspection"));
    QDBusPendingCallWatcher sessionWatcher(asyncCall(
            m_brokerConnection,
            sessionPath,
            QStringLiteral("org.freedesktop.DBus.Introspectable"),
            QStringLiteral("Introspect")));
    QTRY_VERIFY_WITH_TIMEOUT(sessionWatcher.isFinished(), 3000);
    QDBusPendingReply<QString> sessionReply = sessionWatcher;
    QVERIFY2(!sessionReply.isError(), qPrintable(sessionReply.error().message()));
    const InterfaceContract session = readInterface(
            sessionReply.value(), QStringLiteral("org.freedesktop.impl.portal.Session"));
    QCOMPARE(session.methods.value(QStringLiteral("Close")), QStringLiteral("->"));
    QCOMPARE(session.signalEntries.value(QStringLiteral("Closed")), QString());
    QCOMPARE(session.signalEntries.size(), 1);
    QCOMPARE(session.properties.value(QStringLiteral("version")), QStringLiteral("u"));
    closeSession(sessionPath);
}

void TestGlobalShortcutsController::rejectsNonBrokerCaller()
{
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/forged_create");
    QDBusPendingCallWatcher watcher(asyncCall(
            m_providerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("CreateSession"),
            {QVariant::fromValue(QDBusObjectPath(
                     QStringLiteral("/org/freedesktop/portal/desktop/request/test/forged_create"))),
             QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QStringLiteral("org.example.Forged"),
             QVariantMap{}}));
    QTRY_VERIFY_WITH_TIMEOUT(watcher.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> reply = watcher;
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    QCOMPARE(reply.argumentAt<0>(), uint(GlobalShortcutsController::OtherError));
    QCOMPARE(m_portal->controller()->sessionCount(), 0);
}

void TestGlobalShortcutsController::sessionCloseFinishesPendingBind()
{
    const QString appId = QStringLiteral("org.example.PendingClose");
    const QString requestPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/test/pending_close");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/pending_close");
    createSession(requestPath + QStringLiteral("_create"), sessionPath, appId);

    withService([](FakeShortcutService *service) { service->setDelayList(true); });
    QDBusPendingCallWatcher bindWatcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("BindShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QVariant::fromValue(declaredShortcuts(QStringLiteral("action"))),
             QString(),
             QVariantMap{}}));
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) { return service->listCalls(); }), 1);

    closeSession(sessionPath);
    QTRY_VERIFY_WITH_TIMEOUT(bindWatcher.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> reply = bindWatcher;
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    QCOMPARE(reply.argumentAt<0>(), uint(GlobalShortcutsController::OtherError));
    QCOMPARE(serviceValue([](FakeShortcutService *service) {
                 return service->deactivateCalls();
             }),
             1);

    withService([](FakeShortcutService *service) { service->replyList(); });
    QTest::qWait(20);
    QCOMPARE(serviceValue([](FakeShortcutService *service) { return service->commitCalls(); }), 0);
}

void TestGlobalShortcutsController::sessionCloseDuringCommitRepliesOnceAndDeactivatesLateSuccess()
{
    const QString appId = QStringLiteral("org.example.CommitClose");
    const QString shortcutId = QStringLiteral("action");
    const QString requestPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/test/commit_close");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/commit_close");
    const PortalShortcutInfo record = shortcutRecord(appId, shortcutId);
    withService([record](FakeShortcutService *service) {
        service->setHistory({record});
        service->setCommitResult({record});
        service->setDelayCommit(true);
    });
    createSession(requestPath + QStringLiteral("_create"), sessionPath, appId);

    QDBusPendingCallWatcher bindWatcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("BindShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QVariant::fromValue(declaredShortcuts(shortcutId)),
             QString(),
             QVariantMap{}}));
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) { return service->commitCalls(); }), 1);

    closeSession(sessionPath);
    QTRY_VERIFY_WITH_TIMEOUT(bindWatcher.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> reply = bindWatcher;
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    QCOMPARE(reply.argumentAt<0>(), uint(GlobalShortcutsController::OtherError));
    QCOMPARE(serviceValue([](FakeShortcutService *service) {
                 return service->deactivateCalls();
             }),
             1);

    withService([](FakeShortcutService *service) { service->replyCommit(); });
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) {
                     return service->deactivateCalls();
                 }),
                 2);
}

void TestGlobalShortcutsController::listBeforeBindOmitsDisabledHistory()
{
    const QString appId = QStringLiteral("org.example.DisabledHistory");
    const QString requestPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/test/disabled_history");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/disabled_history");
    PortalShortcutInfo record = shortcutRecord(appId, QStringLiteral("action"), false);
    record.enabled = false;
    withService([record](FakeShortcutService *service) { service->setHistory({record}); });
    createSession(requestPath + QStringLiteral("_create"), sessionPath, appId);

    QDBusPendingCallWatcher listWatcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("ListShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath))}));
    QTRY_VERIFY_WITH_TIMEOUT(listWatcher.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> listReply = listWatcher;
    QVERIFY2(!listReply.isError(), qPrintable(listReply.error().message()));
    QCOMPARE(listReply.argumentAt<0>(), uint(GlobalShortcutsController::Success));
    const QVariantMap results = listReply.argumentAt<1>();
    QCOMPARE(qvariant_cast<XdgShortcutList>(results.value(QStringLiteral("shortcuts"))).size(), 0);
    closeSession(sessionPath);
}

void TestGlobalShortcutsController::requestCloseCompletesPrivateBindWithoutPublicResponse()
{
    const QString appId = QStringLiteral("org.example.RequestClose");
    const QString requestPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/test/request_close");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/request_close");
    createSession(requestPath + QStringLiteral("_create"), sessionPath, appId);

    withService([](FakeShortcutService *service) { service->setDelayList(true); });
    QDBusPendingCallWatcher bindWatcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("BindShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QVariant::fromValue(declaredShortcuts(QStringLiteral("action"))),
             QString(),
             QVariantMap{}}));
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) { return service->listCalls(); }), 1);

    QDBusPendingCallWatcher closeWatcher(asyncCall(
            m_brokerConnection,
            requestPath,
            QStringLiteral("org.freedesktop.impl.portal.Request"),
            QStringLiteral("Close")));
    QTRY_VERIFY_WITH_TIMEOUT(closeWatcher.isFinished(), 3000);
    QDBusPendingReply<> closeReply = closeWatcher;
    QVERIFY2(!closeReply.isError(), qPrintable(closeReply.error().message()));

    QTRY_VERIFY_WITH_TIMEOUT(bindWatcher.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> bindReply = bindWatcher;
    QVERIFY2(!bindReply.isError(), qPrintable(bindReply.error().message()));
    QCOMPARE(bindReply.argumentAt<0>(), uint(GlobalShortcutsController::Cancelled));

    withService([](FakeShortcutService *service) { service->replyList(); });
    QTest::qWait(20);
    QCOMPARE(serviceValue([](FakeShortcutService *service) { return service->commitCalls(); }), 0);
    closeSession(sessionPath);
}

void TestGlobalShortcutsController::requestCloseCompletesPrivateListWithoutPublicResponse()
{
    const QString appId = QStringLiteral("org.example.ListRequestClose");
    const QString requestPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/test/list_request_close");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/list_request_close");
    createSession(requestPath + QStringLiteral("_create"), sessionPath, appId);

    withService([](FakeShortcutService *service) { service->setDelayList(true); });
    QDBusPendingCallWatcher listWatcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("ListShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath))}));
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) { return service->listCalls(); }), 1);

    QDBusPendingCallWatcher closeWatcher(asyncCall(
            m_brokerConnection,
            requestPath,
            QStringLiteral("org.freedesktop.impl.portal.Request"),
            QStringLiteral("Close")));
    QTRY_VERIFY_WITH_TIMEOUT(closeWatcher.isFinished(), 3000);
    QDBusPendingReply<> closeReply = closeWatcher;
    QVERIFY2(!closeReply.isError(), qPrintable(closeReply.error().message()));

    QTRY_VERIFY_WITH_TIMEOUT(listWatcher.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> listReply = listWatcher;
    QVERIFY2(!listReply.isError(), qPrintable(listReply.error().message()));
    QCOMPARE(listReply.argumentAt<0>(), uint(GlobalShortcutsController::Cancelled));

    withService([](FakeShortcutService *service) { service->replyList(); });
    QTest::qWait(20);
    closeSession(sessionPath);
}

void TestGlobalShortcutsController::invalidParentDoesNotRetainOversizedDeclarations()
{
    const QString requestPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/test/invalid_parent");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/invalid_parent");
    createSession(requestPath + QStringLiteral("_create"), sessionPath,
                  QStringLiteral("org.example.InvalidParent"));
    XdgShortcutList declarations;
    for (int i = 0; i < 101; ++i)
        declarations.append(declaredShortcuts(QString::number(i)).constFirst());

    QDBusPendingCallWatcher watcher(asyncCall(
            m_brokerConnection, QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface), QStringLiteral("BindShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath)), QVariant::fromValue(declarations),
             QString(4097, u'x'), QVariantMap{}}));
    QTRY_VERIFY_WITH_TIMEOUT(watcher.isFinished(), 3000);
    const QDBusPendingReply<uint, QVariantMap> reply = watcher;
    QVERIFY(!reply.isError());
    QCOMPARE(reply.argumentAt<0>(), uint(GlobalShortcutsController::OtherError));
    const auto *session = m_portal->controller()->session(QDBusObjectPath(sessionPath));
    QVERIFY(session);
    QVERIFY(session->bindAttempted());
    QVERIFY(session->declaredOrder().isEmpty());
    QVERIFY(session->declaredProperties().isEmpty());
    QCOMPARE(serviceValue([](FakeShortcutService *service) { return service->listCalls(); }), 0);

    QDBusPendingCallWatcher retry(asyncCall(
            m_brokerConnection, QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface), QStringLiteral("BindShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QVariant::fromValue(declaredShortcuts(QStringLiteral("valid"))),
             QString(), QVariantMap{}}));
    QTRY_VERIFY_WITH_TIMEOUT(retry.isFinished(), 3000);
    const QDBusPendingReply<uint, QVariantMap> retryReply = retry;
    QCOMPARE(retryReply.argumentAt<0>(), uint(GlobalShortcutsController::OtherError));
    closeSession(sessionPath);
}

void TestGlobalShortcutsController::acceptsSupplementaryUnicodeDeclarations()
{
    const QString appId = QStringLiteral("org.example.Unicode");
    const QString shortcutId = QStringLiteral("action-\U0001f399");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/unicode");
    PortalShortcutInfo record = shortcutRecord(appId, shortcutId);
    record.description = QStringLiteral("Toggle microphone \U0001f399");
    withService([record](FakeShortcutService *service) {
        service->setHistory({record});
        service->setCommitResult({record});
    });
    createSession(sessionPath + QStringLiteral("_create"), sessionPath, appId);
    bindSession(sessionPath,
                {{shortcutId, {{QStringLiteral("description"), record.description}}}});
    const auto *session = m_portal->controller()->session(QDBusObjectPath(sessionPath));
    QVERIFY(session);
    QCOMPARE(session->declaredOrder(), QStringList{shortcutId});
    QCOMPARE(session->activeShortcuts().size(), 1);
    closeSession(sessionPath);
}

void TestGlobalShortcutsController::cancelledListCannotCompleteReusedRequestPath()
{
    const QString appId = QStringLiteral("org.example.ReusedList");
    const QString requestPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/test/reused_list");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/reused_list");
    createSession(requestPath + QStringLiteral("_create"), sessionPath, appId);
    withService([](FakeShortcutService *service) { service->setDelayList(true); });
    const QVariantList arguments = {
        QVariant::fromValue(QDBusObjectPath(requestPath)),
        QVariant::fromValue(QDBusObjectPath(sessionPath)),
    };
    QDBusPendingCallWatcher first(asyncCall(
            m_brokerConnection, QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface), QStringLiteral("ListShortcuts"), arguments));
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) { return service->listCalls(); }), 1);
    QDBusPendingCallWatcher close(asyncCall(
            m_brokerConnection, requestPath, QStringLiteral("org.freedesktop.impl.portal.Request"),
            QStringLiteral("Close")));
    QTRY_VERIFY_WITH_TIMEOUT(close.isFinished(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(first.isFinished(), 3000);
    const QDBusPendingReply<uint, QVariantMap> cancelled = first;
    QCOMPARE(cancelled.argumentAt<0>(), uint(GlobalShortcutsController::Cancelled));
    QTest::qWait(20);

    QDBusPendingCallWatcher replacement(asyncCall(
            m_brokerConnection, QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface), QStringLiteral("ListShortcuts"), arguments));
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) { return service->listCalls(); }), 2);
    withService([appId](FakeShortcutService *service) {
        service->replyNextList({shortcutRecord(appId, QStringLiteral("old"))});
    });
    QTest::qWait(20);
    QVERIFY(!replacement.isFinished());
    withService([appId](FakeShortcutService *service) {
        service->replyNextList({shortcutRecord(appId, QStringLiteral("new"))});
    });
    QTRY_VERIFY_WITH_TIMEOUT(replacement.isFinished(), 3000);
    const QDBusPendingReply<uint, QVariantMap> reply = replacement;
    QVERIFY(!reply.isError());
    QCOMPARE(reply.argumentAt<0>(), uint(GlobalShortcutsController::Success));
    const XdgShortcutList listed = qdbus_cast<XdgShortcutList>(
            reply.argumentAt<1>().value(QStringLiteral("shortcuts")));
    QCOMPARE(listed.size(), 1);
    QCOMPARE(listed.constFirst().id, QStringLiteral("new"));
    closeSession(sessionPath);
}

void TestGlobalShortcutsController::serviceSignalsReachBoundSession()
{
    const QString appId = QStringLiteral("org.example.Signals");
    const QString shortcutId = QStringLiteral("action");
    const QString requestPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/test/signals");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/signals");
    const PortalShortcutInfo record = shortcutRecord(appId, shortcutId);
    withService([record](FakeShortcutService *service) {
        service->setHistory({record});
        service->setCommitResult({record});
    });
    createSession(requestPath + QStringLiteral("_create"), sessionPath, appId);

    QDBusPendingCallWatcher bindWatcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("BindShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QVariant::fromValue(declaredShortcuts(shortcutId)),
             QString(),
             QVariantMap{}}));
    QTRY_VERIFY_WITH_TIMEOUT(bindWatcher.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> bindReply = bindWatcher;
    QVERIFY2(!bindReply.isError(), qPrintable(bindReply.error().message()));
    QCOMPARE(bindReply.argumentAt<0>(), uint(GlobalShortcutsController::Success));

    QSignalSpy activatedSpy(m_portal, &GlobalShortcutPortal::Activated);
    QSignalSpy changedSpy(m_portal, &GlobalShortcutPortal::ShortcutsChanged);
    withService([appId, shortcutId](FakeShortcutService *service) {
        service->sendPressed(appId, shortcutId, 42);
    });
    QTRY_COMPARE(activatedSpy.size(), 1);
    QCOMPARE(activatedSpy.constFirst().at(0).value<QDBusObjectPath>().path(), sessionPath);
    QCOMPARE(activatedSpy.constFirst().at(1).toString(), shortcutId);
    QCOMPARE(activatedSpy.constFirst().at(2).toULongLong(), qulonglong(42));

    PortalShortcutInfo changedRecord = record;
    changedRecord.hotkeys = {QStringLiteral("Ctrl+L")};
    withService([appId, changedRecord](FakeShortcutService *service) {
        service->sendChanged(appId, {changedRecord});
    });
    QTRY_COMPARE(changedSpy.size(), 1);
    const XdgShortcutList changed = changedSpy.constFirst().at(1).value<XdgShortcutList>();
    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.constFirst().id, shortcutId);

    closeSession(sessionPath);
}

void TestGlobalShortcutsController::configureUpdatesAreScopedAndDeduplicated()
{
    const QString appId = QStringLiteral("org.example.Configure");
    const QString shortcutId = QStringLiteral("action");
    const QString requestPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/test/configure");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/configure");
    const PortalShortcutInfo initial = shortcutRecord(appId, shortcutId);
    PortalShortcutInfo updated = initial;
    updated.hotkeys = {QStringLiteral("Ctrl+L")};
    withService([initial, updated](FakeShortcutService *service) {
        service->setHistory({initial});
        service->setCommitResult({initial});
        service->setApplyResult({updated});
    });
    createSession(requestPath + QStringLiteral("_create"), sessionPath, appId);

    QDBusPendingCallWatcher bindWatcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("BindShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QVariant::fromValue(declaredShortcuts(shortcutId)),
             QString(),
             QVariantMap{}}));
    QTRY_VERIFY_WITH_TIMEOUT(bindWatcher.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> bindReply = bindWatcher;
    QVERIFY2(!bindReply.isError(), qPrintable(bindReply.error().message()));
    QCOMPARE(bindReply.argumentAt<0>(), uint(GlobalShortcutsController::Success));

    PortalShortcutUpdate update;
    update.shortcutId = shortcutId;
    update.description = initial.description;
    update.hotkeys = updated.hotkeys;
    update.enabled = true;

    QSignalSpy changedSpy(m_portal, &GlobalShortcutPortal::ShortcutsChanged);
    QDBusPendingCallWatcher applyWatcher(asyncCall(
            m_providerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(ManagementInterface),
            QStringLiteral("ApplySessionShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QVariant::fromValue(PortalShortcutUpdateList{update})}));
    QTRY_VERIFY_WITH_TIMEOUT(applyWatcher.isFinished(), 3000);
    QDBusPendingReply<PortalShortcutInfoList> applyReply = applyWatcher;
    QVERIFY2(!applyReply.isError(), qPrintable(applyReply.error().message()));
    QCOMPARE(applyReply.value().size(), 1);
    QTRY_COMPARE(changedSpy.size(), 1);
    QTest::qWait(20);
    QCOMPARE(changedSpy.size(), 1);
    QCOMPARE(serviceValue([](FakeShortcutService *service) { return service->applyCalls(); }), 1);

    PortalShortcutUpdate undeclared = update;
    undeclared.shortcutId = QStringLiteral("outside-session");
    QDBusPendingCallWatcher rejectedWatcher(asyncCall(
            m_providerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(ManagementInterface),
            QStringLiteral("ApplySessionShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QVariant::fromValue(PortalShortcutUpdateList{undeclared})}));
    QTRY_VERIFY_WITH_TIMEOUT(rejectedWatcher.isFinished(), 3000);
    QDBusPendingReply<PortalShortcutInfoList> rejectedReply = rejectedWatcher;
    QVERIFY(rejectedReply.isError());
    QCOMPARE(rejectedReply.error().name(), QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"));
    QCOMPARE(serviceValue([](FakeShortcutService *service) { return service->applyCalls(); }), 1);

    QDBusPendingCallWatcher forgedWatcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(ManagementInterface),
            QStringLiteral("ApplySessionShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QVariant::fromValue(PortalShortcutUpdateList{update})}));
    QTRY_VERIFY_WITH_TIMEOUT(forgedWatcher.isFinished(), 3000);
    QDBusPendingReply<PortalShortcutInfoList> forgedReply = forgedWatcher;
    QVERIFY(forgedReply.isError());
    QCOMPARE(forgedReply.error().name(), QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"));

    closeSession(sessionPath);
}

void TestGlobalShortcutsController::applicationSnapshotDoesNotPromoteUnboundSession()
{
    const QString appId = QStringLiteral("org.example.Isolation");
    const auto record = shortcutRecord(appId, QStringLiteral("talk"));
    const QString firstPath = QStringLiteral("/test/isolation/first");
    const QString secondPath = QStringLiteral("/test/isolation/second");
    auto *controller = m_portal->controller();
    withService([record](FakeShortcutService *service) { service->setHistory({record}); });
    createSession(QStringLiteral("/test/isolation/create_first"), firstPath, appId);
    bindSession(firstPath, declaredShortcuts(record.shortcutId));
    auto *first = controller->session(QDBusObjectPath(firstPath));
    QVERIFY(first->boundIds().isEmpty());
    withService([record](FakeShortcutService *service) { service->setCommitResult({record}); });
    createSession(QStringLiteral("/test/isolation/create_second"), secondPath, appId);
    bindSession(secondPath, declaredShortcuts(record.shortcutId));
    QSignalSpy activated(m_portal, &GlobalShortcutPortal::Activated);
    QSignalSpy released(m_portal, &GlobalShortcutPortal::Deactivated);
    QSignalSpy changed(m_portal, &GlobalShortcutPortal::ShortcutsChanged);
    withService([record](FakeShortcutService *service) {
        service->sendChanged(record.appId, {record});
        service->sendPressed(record.appId, record.shortcutId, 42);
    });
    QTRY_COMPARE(activated.size(), 1);
    QCOMPARE(activated.first().at(0).value<QDBusObjectPath>().path(), secondPath);
    QVERIFY(first->boundIds().isEmpty());
    QCOMPARE(changed.size(), 0);

    // Only the authoritative per-session notification can expand membership.
    const QString token = first->sessionToken();
    withService([record, token](FakeShortcutService *service) {
        service->sendSessionChanged(QStringLiteral("org.example.Other"), token, {record});
        service->sendSessionChanged(record.appId, QStringLiteral("unknown-token"), {record});
    });
    QTest::qWait(30);
    QVERIFY(first->boundIds().isEmpty());
    withService([record, token](FakeShortcutService *service) {
        service->sendSessionChanged(record.appId, token, {record});
        service->sendReleased(record.appId, record.shortcutId, 43);
    });
    QTRY_VERIFY(first->boundIds().contains(record.shortcutId));
    QTRY_COMPARE(released.size(), 1);
    QCOMPARE(released.first().at(0).value<QDBusObjectPath>().path(), secondPath);
    withService([record](FakeShortcutService *service) {
        service->sendPressed(record.appId, record.shortcutId, 44);
        service->sendReleased(record.appId, record.shortcutId, 45);
        service->sendReleased(record.appId, record.shortcutId, 45);
    });
    QTRY_COMPARE(activated.size(), 3);
    QTRY_COMPARE(released.size(), 3);

    const QString secondToken = controller->session(QDBusObjectPath(secondPath))->sessionToken();
    withService([record, token, secondToken](FakeShortcutService *service) {
        service->sendPressed(record.appId, record.shortcutId, 46);
        service->sendSessionChanged(record.appId, token, {});
        service->sendSessionChanged(record.appId, secondToken, {});
        service->sendReleased(record.appId, record.shortcutId, 47);
        service->sendReleased(record.appId, record.shortcutId, 47);
    });
    QTRY_COMPARE(activated.size(), 5);
    QTRY_COMPARE(released.size(), 5);
    QVERIFY(first->boundIds().isEmpty());
    QVERIFY(controller->session(QDBusObjectPath(secondPath))->boundIds().isEmpty());
    withService([record, token, secondToken](FakeShortcutService *service) {
        service->sendSessionChanged(record.appId, token, {record});
        service->sendSessionChanged(record.appId, secondToken, {record});
        service->sendChanged(record.appId, {record});
    });
    QTRY_VERIFY(first->boundIds().contains(record.shortcutId));
    QTRY_VERIFY(controller->session(QDBusObjectPath(secondPath))->boundIds()
                        .contains(record.shortcutId));
    QTRY_COMPARE(changed.size(), 5);
    closeSession(firstPath);
    closeSession(secondPath);
}

void TestGlobalShortcutsController::replaySerializesAndRetriesBusy()
{
    const QString appId = QStringLiteral("org.example.Replay");
    const auto record = shortcutRecord(appId, QStringLiteral("talk"));
    const QStringList paths = {QStringLiteral("/test/replay/first"),
                               QStringLiteral("/test/replay/second")};
    withService([record](FakeShortcutService *service) {
        service->setHistory({record});
        service->setCommitResult({record});
        service->setBusyRestores(2);
        service->setDelayRestore(true);
    });
    for (const auto &path : paths) {
        createSession(path + QStringLiteral("_create"), path, appId);
        bindSession(path, declaredShortcuts(record.shortcutId));
    }
    auto *controller = m_portal->controller();
    QVERIFY(QMetaObject::invokeMethod(controller, "onServiceVanished", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(controller, "replayBoundSessions", Qt::DirectConnection));
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) { return service->restoreCalls(); }),
                 3);
    QTest::qWait(100);
    QCOMPARE(serviceValue([](FakeShortcutService *service) { return service->restoreCalls(); }), 3);
    withService([record](FakeShortcutService *service) {
        service->sendChanged(record.appId, {record});
        service->replyRestore({record});
    });
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) { return service->restoreCalls(); }),
                 4);
    QStringList restoredIds;
    withService([&restoredIds, record](FakeShortcutService *service) {
        restoredIds = service->lastRestoredActiveIds();
        service->replyRestore({record});
    });
    QCOMPARE(restoredIds, QStringList{record.shortcutId});
    for (const auto &path : paths) {
        QTRY_VERIFY(controller->session(QDBusObjectPath(path))->boundIds()
                            .contains(record.shortcutId));
        closeSession(path);
    }
}

void TestGlobalShortcutsController::replayBusyRetriesAreBounded()
{
    const QString appId = QStringLiteral("org.example.Busy");
    const auto record = shortcutRecord(appId, QStringLiteral("talk"));
    const QStringList paths = {QStringLiteral("/test/replay/busy_first"),
                               QStringLiteral("/test/replay/busy_second")};
    withService([record](FakeShortcutService *service) {
        service->setHistory({record});
        service->setCommitResult({record});
        service->setRestoreResult({record});
        service->setBusyRestores(6);
    });
    for (const auto &path : paths) {
        createSession(path + QStringLiteral("_create"), path, appId);
        bindSession(path, declaredShortcuts(record.shortcutId));
    }
    auto *controller = m_portal->controller();
    QVERIFY(QMetaObject::invokeMethod(controller, "onServiceVanished", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(controller, "replayBoundSessions", Qt::DirectConnection));
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) { return service->restoreCalls(); }),
                 1);
    QString failedToken;
    withService([&failedToken](FakeShortcutService *service) {
        failedToken = service->lastRestoreToken();
    });
    QTRY_COMPARE_WITH_TIMEOUT(
            serviceValue([](FakeShortcutService *service) { return service->restoreCalls(); }),
            7, 5000);
    QTest::qWait(200);
    QCOMPARE(serviceValue([](FakeShortcutService *service) { return service->restoreCalls(); }), 7);
    for (const auto &path : paths) {
        auto *target = controller->session(QDBusObjectPath(path));
        QCOMPARE(target->boundIds().isEmpty(), target->sessionToken() == failedToken);
    }
    // A later service generation must still have the original recovery set.
    withService([](FakeShortcutService *service) { service->setBusyRestores(0); });
    QVERIFY(QMetaObject::invokeMethod(controller, "onServiceVanished", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(controller, "replayBoundSessions", Qt::DirectConnection));
    for (const auto &path : paths) {
        QTRY_VERIFY(controller->session(QDBusObjectPath(path))->boundIds()
                            .contains(record.shortcutId));
        closeSession(path);
    }
}

void TestGlobalShortcutsController::closeDuringReplayContinuesQueue()
{
    const QString appId = QStringLiteral("org.example.CloseReplay");
    const auto record = shortcutRecord(appId, QStringLiteral("talk"));
    QStringList paths = {QStringLiteral("/test/close_replay/first"),
                         QStringLiteral("/test/close_replay/second")};
    withService([record](FakeShortcutService *service) {
        service->setHistory({record});
        service->setCommitResult({record});
        service->setDelayRestore(true);
    });
    for (const auto &path : paths) {
        createSession(path + QStringLiteral("_create"), path, appId);
        bindSession(path, declaredShortcuts(record.shortcutId));
    }
    auto *controller = m_portal->controller();
    QVERIFY(QMetaObject::invokeMethod(controller, "onServiceVanished", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(controller, "replayBoundSessions", Qt::DirectConnection));
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) { return service->restoreCalls(); }),
                 1);
    QString token;
    withService([&token](FakeShortcutService *service) { token = service->lastRestoreToken(); });
    const bool firstIsRestoring =
            controller->session(QDBusObjectPath(paths.first()))->sessionToken() == token;
    const QString closed = firstIsRestoring ? paths.first() : paths.last();
    closeSession(closed);
    paths.removeAll(closed);
    withService([record](FakeShortcutService *service) { service->replyRestore({record}); });
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) { return service->restoreCalls(); }),
                 2);
    QTRY_VERIFY(serviceValue([](FakeShortcutService *service) {
        return service->deactivateCalls();
    }) >= 2);
    withService([record](FakeShortcutService *service) { service->replyRestore({record}); });
    QTRY_VERIFY(controller->session(QDBusObjectPath(paths.first()))->boundIds()
                        .contains(record.shortcutId));
    closeSession(paths.first());
}

void TestGlobalShortcutsController::configureInFlightIsBounded()
{
    const QString appId = QStringLiteral("org.example.ConfigureLimit");
    const QString shortcutId = QStringLiteral("action");
    const QString requestPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/test/configure_limit");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/configure_limit");
    const PortalShortcutInfo record = shortcutRecord(appId, shortcutId);
    withService([record](FakeShortcutService *service) {
        service->setHistory({record});
        service->setCommitResult({record});
    });
    createSession(requestPath + QStringLiteral("_create"), sessionPath, appId);

    QDBusPendingCallWatcher bindWatcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("BindShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QVariant::fromValue(declaredShortcuts(shortcutId)),
             QString(),
             QVariantMap{}}));
    QTRY_VERIFY_WITH_TIMEOUT(bindWatcher.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> bindReply = bindWatcher;
    QVERIFY2(!bindReply.isError(), qPrintable(bindReply.error().message()));
    QCOMPARE(bindReply.argumentAt<0>(), uint(GlobalShortcutsController::Success));

    withService([](FakeShortcutService *service) { service->setDelayList(true); });
    QDBusPendingCallWatcher first(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("ConfigureShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(sessionPath)), QString(), QVariantMap{}}));
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) { return service->listCalls(); }),
                 2);

    QDBusPendingCallWatcher duplicate(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("ConfigureShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(sessionPath)), QString(), QVariantMap{}}));
    QTRY_VERIFY_WITH_TIMEOUT(duplicate.isFinished(), 3000);
    const QDBusPendingReply<> duplicateReply = duplicate;
    QVERIFY(duplicateReply.isError());
    QCOMPARE(duplicateReply.error().name(), QStringLiteral("org.freedesktop.portal.Error.Failed"));
    QCOMPARE(serviceValue([](FakeShortcutService *service) { return service->listCalls(); }), 2);

    closeSession(sessionPath);
    withService([](FakeShortcutService *service) { service->replyList(); });
    QTRY_VERIFY_WITH_TIMEOUT(first.isFinished(), 3000);
    const QDBusPendingReply<> firstReply = first;
    QVERIFY(firstReply.isError());
}

void TestGlobalShortcutsController::serviceRestartRestoresOnlyPreviouslyBoundSubset()
{
    const QString appId = QStringLiteral("org.example.Restart");
    const PortalShortcutInfo first = shortcutRecord(appId, QStringLiteral("first"));
    PortalShortcutInfo second = shortcutRecord(appId, QStringLiteral("second"));
    second.id = QStringLiteral("portal-test-second");
    second.hotkeys = {QStringLiteral("Ctrl+M")};
    const QString requestPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/test/restart");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/restart");
    withService([first, second](FakeShortcutService *service) {
        service->setHistory({first, second});
        service->setCommitResult({first});
        service->setRestoreResult({first});
    });
    createSession(requestPath + QStringLiteral("_create"), sessionPath, appId);

    const XdgShortcutList declared = {
        {first.shortcutId,
         {{QStringLiteral("description"), QStringLiteral("First action")}}},
        {second.shortcutId,
         {{QStringLiteral("description"), QStringLiteral("Second action")}}},
    };
    QDBusPendingCallWatcher bindWatcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("BindShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QVariant::fromValue(declared),
             QString(),
             QVariantMap{}}));
    QTRY_VERIFY_WITH_TIMEOUT(bindWatcher.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> bindReply = bindWatcher;
    QVERIFY2(!bindReply.isError(), qPrintable(bindReply.error().message()));
    QCOMPARE(bindReply.argumentAt<0>(), uint(GlobalShortcutsController::Success));
    QCOMPARE(m_portal->controller()->session(QDBusObjectPath(sessionPath))
                     ->activeShortcuts()
                     .size(),
             1);

    bool serviceUnregistered = false;
    withService([this, &serviceUnregistered](FakeShortcutService *) {
        serviceUnregistered = m_serviceConnection.unregisterService(
                QString::fromLatin1(ShortcutService));
    });
    QVERIFY(serviceUnregistered);
    QTRY_VERIFY(m_portal->controller()->session(QDBusObjectPath(sessionPath))
                        ->activeShortcuts()
                        .isEmpty());

    bool serviceRegistered = false;
    withService([this, &serviceRegistered](FakeShortcutService *) {
        serviceRegistered = m_serviceConnection.registerService(
                QString::fromLatin1(ShortcutService));
    });
    QVERIFY(serviceRegistered);
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) {
                     return service->restoreCalls();
                 }),
                 1);

    PortalShortcutUpdateList restoredDeclared;
    QStringList restoredActiveIds;
    withService([&restoredDeclared, &restoredActiveIds](FakeShortcutService *service) {
        restoredDeclared = service->lastRestoredDeclared();
        restoredActiveIds = service->lastRestoredActiveIds();
    });
    QCOMPARE(restoredDeclared.size(), 2);
    QCOMPARE(restoredActiveIds, QStringList({first.shortcutId}));
    QTRY_COMPARE(m_portal->controller()->session(QDBusObjectPath(sessionPath))
                         ->activeShortcuts()
                         .size(),
                 1);

    closeSession(sessionPath);
}

void TestGlobalShortcutsController::rapidServiceRestartPreservesReplaySetAndIgnoresOldReply()
{
    const QString appId = QStringLiteral("org.example.RapidRestart");
    const PortalShortcutInfo record = shortcutRecord(appId, QStringLiteral("action"));
    const QString requestPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/request/test/rapid_restart");
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/test/rapid_restart");
    withService([record](FakeShortcutService *service) {
        service->setHistory({record});
        service->setCommitResult({record});
        service->setRestoreResult({record});
    });
    createSession(requestPath + QStringLiteral("_create"), sessionPath, appId);

    QDBusPendingCallWatcher bindWatcher(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("BindShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(requestPath)),
             QVariant::fromValue(QDBusObjectPath(sessionPath)),
             QVariant::fromValue(declaredShortcuts(record.shortcutId)),
             QString(),
             QVariantMap{}}));
    QTRY_VERIFY_WITH_TIMEOUT(bindWatcher.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> bindReply = bindWatcher;
    QVERIFY2(!bindReply.isError(), qPrintable(bindReply.error().message()));
    QCOMPARE(bindReply.argumentAt<0>(), uint(GlobalShortcutsController::Success));

    withService([](FakeShortcutService *service) { service->setDelayRestore(true); });
    bool firstUnregistered = false;
    withService([this, &firstUnregistered](FakeShortcutService *) {
        firstUnregistered = m_serviceConnection.unregisterService(
                QString::fromLatin1(ShortcutService));
    });
    QVERIFY(firstUnregistered);
    QTRY_VERIFY(m_portal->controller()->session(QDBusObjectPath(sessionPath))
                        ->activeShortcuts()
                        .isEmpty());

    bool firstRegistered = false;
    withService([this, &firstRegistered](FakeShortcutService *) {
        firstRegistered = m_serviceConnection.registerService(
                QString::fromLatin1(ShortcutService));
    });
    QVERIFY(firstRegistered);
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) {
                     return service->restoreCalls();
                 }),
                 1);

    bool secondUnregistered = false;
    withService([this, &secondUnregistered](FakeShortcutService *service) {
        secondUnregistered = m_serviceConnection.unregisterService(
                QString::fromLatin1(ShortcutService));
        service->setDelayRestore(false);
    });
    QVERIFY(secondUnregistered);

    bool secondRegistered = false;
    withService([this, &secondRegistered](FakeShortcutService *) {
        secondRegistered = m_serviceConnection.registerService(
                QString::fromLatin1(ShortcutService));
    });
    QVERIFY(secondRegistered);
    QTRY_COMPARE(serviceValue([](FakeShortcutService *service) {
                     return service->restoreCalls();
                 }),
                 2);

    QStringList restoredActiveIds;
    withService([&restoredActiveIds](FakeShortcutService *service) {
        restoredActiveIds = service->lastRestoredActiveIds();
    });
    QCOMPARE(restoredActiveIds, QStringList({record.shortcutId}));
    QTRY_COMPARE(m_portal->controller()->session(QDBusObjectPath(sessionPath))
                         ->activeShortcuts()
                         .size(),
                 1);

    // A reply from the first service generation must not replace the state
    // confirmed by the second generation.
    withService([](FakeShortcutService *service) { service->replyRestore({}); });
    QTest::qWait(50);
    QCOMPARE(m_portal->controller()->session(QDBusObjectPath(sessionPath))
                     ->activeShortcuts()
                     .size(),
             1);

    closeSession(sessionPath);
}

void TestGlobalShortcutsController::activeSessionLimitIsEnforced()
{
    constexpr int MaximumSessions = 256;
    QStringList sessionPaths;
    sessionPaths.reserve(MaximumSessions);
    for (int index = 0; index < MaximumSessions; ++index) {
        const QString suffix = QString::number(index);
        const QString sessionPath = QStringLiteral(
                "/org/freedesktop/portal/desktop/session/limit/session_%1").arg(suffix);
        createSession(QStringLiteral(
                              "/org/freedesktop/portal/desktop/request/limit/create_%1")
                              .arg(suffix),
                      sessionPath,
                      QStringLiteral("org.example.Limit"));
        sessionPaths.append(sessionPath);
    }
    QCOMPARE(m_portal->controller()->sessionCount(), MaximumSessions);

    QDBusPendingCallWatcher overflow(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("CreateSession"),
            {QVariant::fromValue(QDBusObjectPath(QStringLiteral(
                     "/org/freedesktop/portal/desktop/request/limit/overflow"))),
             QVariant::fromValue(QDBusObjectPath(QStringLiteral(
                     "/org/freedesktop/portal/desktop/session/limit/overflow"))),
             QStringLiteral("org.example.Limit"),
             QVariantMap{}}));
    QTRY_VERIFY_WITH_TIMEOUT(overflow.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> overflowReply = overflow;
    QVERIFY2(!overflowReply.isError(), qPrintable(overflowReply.error().message()));
    QCOMPARE(overflowReply.argumentAt<0>(), uint(GlobalShortcutsController::OtherError));

    QList<QDBusPendingCallWatcher *> closeWatchers;
    closeWatchers.reserve(sessionPaths.size());
    for (const QString &sessionPath : std::as_const(sessionPaths)) {
        closeWatchers.append(new QDBusPendingCallWatcher(
                asyncCall(m_brokerConnection,
                          sessionPath,
                          QStringLiteral("org.freedesktop.impl.portal.Session"),
                          QStringLiteral("Close")),
                this));
    }
    QTRY_VERIFY_WITH_TIMEOUT(std::all_of(closeWatchers.cbegin(),
                                        closeWatchers.cend(),
                                        [](const auto *watcher) { return watcher->isFinished(); }),
                             10000);
    for (QDBusPendingCallWatcher *watcher : std::as_const(closeWatchers)) {
        const QDBusPendingReply<> reply = *watcher;
        QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
        watcher->deleteLater();
    }
    QTRY_COMPARE(m_portal->controller()->sessionCount(), 0);
}

void TestGlobalShortcutsController::pendingRequestLimitIsEnforced()
{
    constexpr int MaximumRequests = 256;
    const QString sessionPath = QStringLiteral(
            "/org/freedesktop/portal/desktop/session/limit/pending_requests");
    createSession(QStringLiteral(
                          "/org/freedesktop/portal/desktop/request/limit/pending_create"),
                  sessionPath,
                  QStringLiteral("org.example.PendingLimit"));
    withService([](FakeShortcutService *service) { service->setDelayList(true); });

    QList<QDBusPendingCallWatcher *> requests;
    requests.reserve(MaximumRequests);
    for (int index = 0; index < MaximumRequests; ++index) {
        requests.append(new QDBusPendingCallWatcher(
                asyncCall(m_brokerConnection,
                          QString::fromLatin1(BackendPath),
                          QString::fromLatin1(BackendInterface),
                          QStringLiteral("ListShortcuts"),
                          {QVariant::fromValue(QDBusObjectPath(
                                   QStringLiteral(
                                           "/org/freedesktop/portal/desktop/request/limit/list_%1")
                                           .arg(index))),
                           QVariant::fromValue(QDBusObjectPath(sessionPath))}),
                this));
    }
    QTRY_COMPARE_WITH_TIMEOUT(serviceValue([](FakeShortcutService *service) {
                                  return service->listCalls();
                              }),
                              MaximumRequests,
                              10000);

    QDBusPendingCallWatcher overflow(asyncCall(
            m_brokerConnection,
            QString::fromLatin1(BackendPath),
            QString::fromLatin1(BackendInterface),
            QStringLiteral("ListShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(QStringLiteral(
                     "/org/freedesktop/portal/desktop/request/limit/list_overflow"))),
             QVariant::fromValue(QDBusObjectPath(sessionPath))}));
    QTRY_VERIFY_WITH_TIMEOUT(overflow.isFinished(), 3000);
    QDBusPendingReply<uint, QVariantMap> overflowReply = overflow;
    QVERIFY2(!overflowReply.isError(), qPrintable(overflowReply.error().message()));
    QCOMPARE(overflowReply.argumentAt<0>(), uint(GlobalShortcutsController::OtherError));

    closeSession(sessionPath);
    QTRY_VERIFY_WITH_TIMEOUT(std::all_of(requests.cbegin(),
                                        requests.cend(),
                                        [](const auto *watcher) { return watcher->isFinished(); }),
                             10000);
    for (QDBusPendingCallWatcher *watcher : std::as_const(requests)) {
        const QDBusPendingReply<uint, QVariantMap> reply = *watcher;
        QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
        QCOMPARE(reply.argumentAt<0>(), uint(GlobalShortcutsController::OtherError));
        watcher->deleteLater();
    }
    withService([](FakeShortcutService *service) { service->replyList(); });
}

QTEST_GUILESS_MAIN(TestGlobalShortcutsController)

#include "test_global_shortcuts_controller.moc"
