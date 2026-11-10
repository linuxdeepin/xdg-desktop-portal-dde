// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "globalshortcutscontroller.h"

#include "globalshortcutrequest.h"
#include "globalshortcutsession.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QTimer>
#include <QUuid>

#include <algorithm>

Q_LOGGING_CATEGORY(GlobalShortcutsControllerLog, "xdg-dde-global-shortcut.controller")

namespace {

constexpr auto NotAllowedError = "org.freedesktop.portal.Error.NotAllowed";
constexpr auto FailedError = "org.freedesktop.portal.Error.Failed";
constexpr auto InvalidArgsError = "org.freedesktop.DBus.Error.InvalidArgs";
constexpr auto UnknownObjectError = "org.freedesktop.DBus.Error.UnknownObject";
constexpr auto AccessDeniedError = "org.freedesktop.DBus.Error.AccessDenied";

constexpr auto PortalBrokerService = "org.freedesktop.portal.Desktop";
constexpr auto ShortcutService = "org.deepin.dde.Keybinding1";
constexpr auto ShortcutServicePath = "/org/deepin/dde/Keybinding1";
constexpr auto ShortcutServiceInterface = "org.deepin.dde.Keybinding1";

constexpr auto ProviderService = "org.deepin.dde.GlobalShortcutsProvider1";
constexpr auto ProviderPath = "/org/deepin/dde/GlobalShortcutsProvider1";
constexpr auto ProviderInterface = "org.deepin.dde.GlobalShortcutsProvider1";

bool containsControlCharacter(const QString &value)
{
    if (!value.isValidUtf16())
        return true;
    for (char32_t character : value.toUcs4()) {
        const QChar::Category category = QChar::category(character);
        if (category == QChar::Other_Control || category == QChar::Other_Format
                || category == QChar::Other_Surrogate
                || category == QChar::Other_PrivateUse
                || category == QChar::Other_NotAssigned) {
            return true;
        }
    }
    return false;
}

bool shortcutsEqual(const XdgShortcutList &left, const XdgShortcutList &right)
{
    if (left.size() != right.size())
        return false;
    for (qsizetype index = 0; index < left.size(); ++index) {
        if (left.at(index).id != right.at(index).id
                || left.at(index).properties != right.at(index).properties) {
            return false;
        }
    }
    return true;
}

} // namespace

GlobalShortcutsController::GlobalShortcutsController(QObject *parent)
    : QObject(parent)
{
    registerGlobalShortcutMetaTypes();

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QString::fromLatin1(ShortcutService),
                QString::fromLatin1(ShortcutServicePath),
                QString::fromLatin1(ShortcutServiceInterface),
                QStringLiteral("PortalShortcutPressed"),
                this,
                SLOT(onPortalShortcutPressed(QString,QString,qulonglong)));
    bus.connect(QString::fromLatin1(ShortcutService),
                QString::fromLatin1(ShortcutServicePath),
                QString::fromLatin1(ShortcutServiceInterface),
                QStringLiteral("PortalShortcutReleased"),
                this,
                SLOT(onPortalShortcutReleased(QString,QString,qulonglong)));
    bus.connect(QString::fromLatin1(ShortcutService),
                QString::fromLatin1(ShortcutServicePath),
                QString::fromLatin1(ShortcutServiceInterface),
                QStringLiteral("PortalShortcutsChanged"),
                this,
                SLOT(onPortalShortcutsChanged(QString,PortalShortcutInfoList)));
    bus.connect(QString::fromLatin1(ShortcutService),
                QString::fromLatin1(ShortcutServicePath),
                QString::fromLatin1(ShortcutServiceInterface),
                QStringLiteral("PortalSessionShortcutsChanged"),
                this,
                SLOT(onPortalSessionShortcutsChanged(QString,QString,PortalShortcutInfoList)));

    m_brokerWatcher = new QDBusServiceWatcher(QString::fromLatin1(PortalBrokerService),
                                              bus,
                                              QDBusServiceWatcher::WatchForUnregistration,
                                              this);
    connect(m_brokerWatcher,
            &QDBusServiceWatcher::serviceUnregistered,
            this,
            &GlobalShortcutsController::onBrokerOwnerVanished);

    m_providerWatcher = new QDBusServiceWatcher(QString::fromLatin1(ProviderService),
                                                bus,
                                                QDBusServiceWatcher::WatchForUnregistration,
                                                this);
    connect(m_providerWatcher,
            &QDBusServiceWatcher::serviceUnregistered,
            this,
            &GlobalShortcutsController::onProviderVanished);

    m_serviceWatcher = new QDBusServiceWatcher(QString::fromLatin1(ShortcutService),
                                               bus,
                                               QDBusServiceWatcher::WatchForOwnerChange,
                                               this);
    connect(m_serviceWatcher,
            &QDBusServiceWatcher::serviceUnregistered,
            this,
            &GlobalShortcutsController::onServiceVanished);
    connect(m_serviceWatcher,
            &QDBusServiceWatcher::serviceRegistered,
            this,
            [this] {
                ++m_serviceGeneration;
                QTimer::singleShot(0, this, &GlobalShortcutsController::replayBoundSessions);
            });
}

uint GlobalShortcutsController::createSession(const QDBusObjectPath &handle,
                                              const QDBusObjectPath &sessionHandle,
                                              const QString &appId,
                                              const QVariantMap &options,
                                              QVariantMap &results,
                                              const QDBusMessage &message)
{
    Q_UNUSED(handle)
    Q_UNUSED(options)
    results.clear();

    const QString owner = caller(message);
    const QString path = sessionHandle.path();
    if (!verifyBrokerCaller(message) || owner.isEmpty() || appId.isEmpty()
            || appId.size() > MaximumAppIdLength
            || containsControlCharacter(appId) || path.isEmpty()
            || m_sessions.contains(path)
            || m_sessions.size() >= MaximumActiveSessions) {
        return OtherError;
    }

    auto *newSession = new GlobalShortcutSession(sessionHandle, appId, owner, this);
    if (!newSession->registerObject()) {
        delete newSession;
        return OtherError;
    }

    m_sessions.insert(path, newSession);
    newSession->setCloseHandler(
            [this](GlobalShortcutSession *session) { closeSession(session); });
    connect(newSession, &QObject::destroyed, this, [this, path, newSession] {
        if (m_sessions.value(path) && m_sessions.value(path) != newSession)
            return;
        m_sessions.remove(path);
        m_configureRequests.remove(path);
        m_sessionCommitRequests.remove(path);
        m_sessionReplayIds.remove(path);
        m_sessionPressedIds.remove(path);
    });
    return Success;
}

uint GlobalShortcutsController::bindShortcuts(
        const QDBusObjectPath &handle,
        const QDBusObjectPath &sessionHandle,
        const XdgShortcutList &shortcuts,
        const QString &parentWindow,
        const QVariantMap &options,
        QVariantMap &results,
        const QDBusMessage &message)
{
    Q_UNUSED(options)
    results.clear();

    GlobalShortcutSession *targetSession = checkedSession(sessionHandle, message);
    if (!targetSession || targetSession->bindAttempted())
        return OtherError;

    if (parentWindow.size() > MaximumParentWindowLength
            || containsControlCharacter(parentWindow)) {
        targetSession->beginBind({});
        targetSession->failBind();
        return OtherError;
    }

    if (!validateShortcuts(shortcuts)) {
        targetSession->beginBind({});
        targetSession->failBind();
        return OtherError;
    }

    targetSession->beginBind(shortcuts);
    if (shortcuts.isEmpty()) {
        targetSession->finishBind({});
        m_sessionCommitRequests.insert(targetSession->path(), {});
        m_sessionReplayIds.insert(targetSession->path(), {});
        results.insert(QStringLiteral("shortcuts"), QVariant::fromValue(XdgShortcutList{}));
        return Success;
    }

    const QString owner = caller(message);
    GlobalShortcutRequest *request = createRequest(handle, owner);
    if (!request) {
        targetSession->failBind();
        return OtherError;
    }

    message.setDelayedReply(true);
    auto pending = QSharedPointer<PendingBind>::create();
    pending->requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    pending->requestPath = handle.path();
    pending->sessionPath = targetSession->path();
    pending->appId = targetSession->appId();
    pending->sessionToken = targetSession->sessionToken();
    pending->parentWindow = parentWindow;
    pending->replyMessage = message;
    pending->request = request;
    pending->session = targetSession;
    pending->declared = shortcuts;
    pending->updates = initialUpdates(shortcuts);
    m_bindRequestByPath.insert(pending->requestPath, pending->requestId);
    m_bindRequests.insert(pending->requestId, pending);
    startBindHistory(pending->requestId);
    return Success;
}

uint GlobalShortcutsController::listShortcuts(const QDBusObjectPath &handle,
                                              const QDBusObjectPath &sessionHandle,
                                              QVariantMap &results,
                                              const QDBusMessage &message)
{
    results.clear();
    GlobalShortcutSession *targetSession = checkedSession(sessionHandle, message);
    if (!targetSession)
        return OtherError;

    if (targetSession->bindAttempted()) {
        results.insert(QStringLiteral("shortcuts"),
                       QVariant::fromValue(targetSession->activeShortcuts()));
        return Success;
    }

    GlobalShortcutRequest *request = createRequest(handle, caller(message));
    if (!request)
        return OtherError;

    message.setDelayedReply(true);
    auto pending = QSharedPointer<PendingList>::create();
    pending->requestPath = handle.path();
    pending->replyMessage = message;
    pending->request = request;
    pending->session = targetSession;
    m_listRequests.insert(pending->requestPath, pending);

    QDBusInterface service(QString::fromLatin1(ShortcutService),
                           QString::fromLatin1(ShortcutServicePath),
                           QString::fromLatin1(ShortcutServiceInterface),
                           QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
            service.asyncCall(QStringLiteral("ListPortalShortcuts"), targetSession->appId()),
            this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, pending] {
        QDBusPendingReply<PortalShortcutInfoList> reply = *watcher;
        watcher->deleteLater();
        if (m_listRequests.value(pending->requestPath) != pending)
            return;
        finishList(pending->requestPath,
                   reply.isError() ? OtherError : Success,
                   reply.isError() ? PortalShortcutInfoList{} : reply.value());
    });
    return Success;
}

void GlobalShortcutsController::configureShortcuts(
        const QDBusObjectPath &sessionHandle,
        const QString &parentWindow,
        const QVariantMap &options,
        const QDBusMessage &message)
{
    GlobalShortcutSession *targetSession = checkedSession(sessionHandle, message);
    if (!targetSession || targetSession->state() != GlobalShortcutSession::State::Bound) {
        sendError(message,
                  QString::fromLatin1(NotAllowedError),
                  QStringLiteral("The session has not completed BindShortcuts successfully"));
        return;
    }

    if (parentWindow.size() > MaximumParentWindowLength
            || containsControlCharacter(parentWindow)) {
        sendError(message,
                  QString::fromLatin1(InvalidArgsError),
                  QStringLiteral("The parent window identifier is invalid"));
        return;
    }

    QVariantMap providerOptions;
    const auto activationToken = options.constFind(QStringLiteral("activation_token"));
    if (activationToken != options.cend()) {
        if (activationToken->metaType() != QMetaType::fromType<QString>()
                || activationToken->toString().size() > MaximumActivationTokenLength
                || containsControlCharacter(activationToken->toString())) {
            sendError(message,
                      QString::fromLatin1(InvalidArgsError),
                      QStringLiteral("The activation token is invalid"));
            return;
        }
        providerOptions.insert(QStringLiteral("activation_token"), activationToken->toString());
    }

    const QString sessionPath = targetSession->path();
    if (m_configureRequests.value(sessionPath)) {
        sendError(message,
                  QString::fromLatin1(FailedError),
                  QStringLiteral("A ConfigureShortcuts request is already in progress"));
        return;
    }

    message.setDelayedReply(true);
    const QPointer<GlobalShortcutSession> guardedSession = targetSession;
    m_configureRequests.insert(sessionPath, guardedSession);
    QDBusInterface service(QString::fromLatin1(ShortcutService),
                           QString::fromLatin1(ShortcutServicePath),
                           QString::fromLatin1(ShortcutServiceInterface),
                           QDBusConnection::sessionBus());
    auto *listWatcher = new QDBusPendingCallWatcher(
            service.asyncCall(QStringLiteral("ListPortalShortcuts"), targetSession->appId()),
            this);
    connect(listWatcher,
            &QDBusPendingCallWatcher::finished,
            this,
            [this,
             listWatcher,
             guardedSession,
             sessionPath,
             parentWindow,
             providerOptions,
             message] {
                QDBusPendingReply<PortalShortcutInfoList> reply = *listWatcher;
                listWatcher->deleteLater();
                if (!guardedSession || guardedSession->state() != GlobalShortcutSession::State::Bound) {
                    if (m_configureRequests.value(sessionPath) == guardedSession)
                        m_configureRequests.remove(sessionPath);
                    sendError(message,
                              QString::fromLatin1(UnknownObjectError),
                              QStringLiteral("The GlobalShortcuts session was closed"));
                    return;
                }
                if (reply.isError()) {
                    if (m_configureRequests.value(sessionPath) == guardedSession)
                        m_configureRequests.remove(sessionPath);
                    sendError(message, reply.error().name(), reply.error().message());
                    return;
                }

                const auto records = byShortcutId(reply.value());
                PortalShortcutInfoList visible;
                for (const QString &id : guardedSession->declaredOrder()) {
                    const auto it = records.constFind(id);
                    if (it == records.cend())
                        continue;
                    PortalShortcutInfo record = *it;
                    record.description = guardedSession->declaredProperties()
                                                 .value(id)
                                                 .value(QStringLiteral("description"))
                                                 .toString();
                    visible.append(record);
                }

                QDBusInterface provider(QString::fromLatin1(ProviderService),
                                        QString::fromLatin1(ProviderPath),
                                        QString::fromLatin1(ProviderInterface),
                                        QDBusConnection::sessionBus());
                auto *providerWatcher = new QDBusPendingCallWatcher(
                        provider.asyncCall(QStringLiteral("ConfigureShortcuts"),
                                           QVariant::fromValue(QDBusObjectPath(guardedSession->path())),
                                           guardedSession->appId(),
                                           parentWindow,
                                           QVariant::fromValue(visible),
                                           providerOptions),
                        this);
                connect(providerWatcher,
                        &QDBusPendingCallWatcher::finished,
                        this,
                        [this, providerWatcher, guardedSession, sessionPath, message] {
                            QDBusPendingReply<> providerReply = *providerWatcher;
                            providerWatcher->deleteLater();
                            if (m_configureRequests.value(sessionPath) == guardedSession)
                                m_configureRequests.remove(sessionPath);
                            if (!guardedSession
                                    || guardedSession->state() == GlobalShortcutSession::State::Closed) {
                                if (guardedSession)
                                    cancelProviderSession(guardedSession->path());
                                sendError(message,
                                          QString::fromLatin1(UnknownObjectError),
                                          QStringLiteral("The GlobalShortcuts session was closed"));
                            } else if (providerReply.isError())
                                sendError(message,
                                          providerReply.error().name(),
                                          providerReply.error().message());
                            else
                                sendVoidReply(message);
                        });
            });
}

bool GlobalShortcutsController::completeBind(
        const QString &requestId,
        uint response,
        const PortalShortcutUpdateList &selected,
        const QDBusMessage &message,
        QString *errorName,
        QString *errorText)
{
    if (!verifyProviderCaller(message, errorName, errorText))
        return false;

    const auto pending = m_bindRequests.value(requestId);
    if (!pending || pending->phase != BindPhase::WaitingProvider) {
        if (errorName)
            *errorName = QString::fromLatin1(UnknownObjectError);
        if (errorText)
            *errorText = QStringLiteral("The bind request is no longer pending");
        return false;
    }
    if (response > Cancelled) {
        if (errorName)
            *errorName = QString::fromLatin1(InvalidArgsError);
        if (errorText)
            *errorText = QStringLiteral("The provider response must be success or cancelled");
        return false;
    }
    if (response == Cancelled) {
        finishBind(requestId, Cancelled);
        return true;
    }

    QHash<QString, PortalShortcutUpdate> choices;
    for (const PortalShortcutUpdate &choice : selected) {
        if (choice.shortcutId.isEmpty() || choices.contains(choice.shortcutId)
                || choice.hotkeys.size() > 1) {
            if (errorName)
                *errorName = QString::fromLatin1(InvalidArgsError);
            if (errorText)
                *errorText = QStringLiteral("The provider returned invalid shortcut choices");
            return false;
        }
        choices.insert(choice.shortcutId, choice);
    }
    if (QSet<QString>(choices.keyBegin(), choices.keyEnd()) != pending->pendingNewIds) {
        if (errorName)
            *errorName = QString::fromLatin1(InvalidArgsError);
        if (errorText)
            *errorText = QStringLiteral("The provider must return exactly the new shortcut ids");
        return false;
    }

    for (PortalShortcutUpdate &update : pending->updates) {
        if (!pending->pendingNewIds.contains(update.shortcutId))
            continue;
        const PortalShortcutUpdate choice = choices.value(update.shortcutId);
        update.hotkeys = choice.hotkeys;
        update.enabled = choice.enabled && !choice.hotkeys.isEmpty();
    }
    commitBind(requestId);
    return true;
}

void GlobalShortcutsController::applySessionShortcuts(
        const QDBusObjectPath &sessionHandle,
        const PortalShortcutUpdateList &updates,
        const QDBusMessage &message)
{
    QString errorName;
    QString errorText;
    if (!verifyProviderCaller(message, &errorName, &errorText)) {
        sendError(message, errorName, errorText);
        return;
    }
    GlobalShortcutSession *targetSession = session(sessionHandle);
    if (!targetSession || targetSession->state() != GlobalShortcutSession::State::Bound) {
        sendError(message,
                  QString::fromLatin1(NotAllowedError),
                  QStringLiteral("Configure is only allowed after a successful Bind"));
        return;
    }
    if (!validateSessionUpdates(targetSession, updates)) {
        sendError(message,
                  QString::fromLatin1(InvalidArgsError),
                  QStringLiteral("A Configure update is outside the session declaration"));
        return;
    }

    message.setDelayedReply(true);
    const QPointer<GlobalShortcutSession> guardedSession = targetSession;
    QDBusInterface service(QString::fromLatin1(ShortcutService),
                           QString::fromLatin1(ShortcutServicePath),
                           QString::fromLatin1(ShortcutServiceInterface),
                           QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
            service.asyncCall(QStringLiteral("ApplyPortalSessionUpdates"),
                              targetSession->appId(),
                              targetSession->sessionToken(),
                              QVariant::fromValue(updates)),
            this);
    connect(watcher,
            &QDBusPendingCallWatcher::finished,
            this,
            [this, watcher, guardedSession, message] {
                QDBusPendingReply<PortalShortcutInfoList> reply = *watcher;
                watcher->deleteLater();
                if (reply.isError()) {
                    sendError(message, reply.error().name(), reply.error().message());
                    return;
                }
                if (!guardedSession || guardedSession->state() == GlobalShortcutSession::State::Closed) {
                    if (guardedSession)
                        callDeactivate(guardedSession->appId(), guardedSession->sessionToken());
                    sendError(message,
                              QString::fromLatin1(UnknownObjectError),
                              QStringLiteral("The GlobalShortcuts session was closed"));
                    return;
                }
                const XdgShortcutList current = toXdgShortcuts(reply.value(), guardedSession, false);
                publishSessionShortcuts(guardedSession, current);
                m_sessionReplayIds.insert(guardedSession->path(), guardedSession->boundIds());
                sendTypedReply(message, QVariant::fromValue(reply.value()));
            });
}

void GlobalShortcutsController::checkSessionShortcutConflicts(
        const QDBusObjectPath &sessionHandle,
        const PortalShortcutUpdateList &shortcuts,
        const QDBusMessage &message)
{
    QString errorName;
    QString errorText;
    if (!verifyProviderCaller(message, &errorName, &errorText)) {
        sendError(message, errorName, errorText);
        return;
    }
    GlobalShortcutSession *targetSession = session(sessionHandle);
    if (!targetSession || targetSession->state() != GlobalShortcutSession::State::Bound) {
        sendError(message,
                  QString::fromLatin1(NotAllowedError),
                  QStringLiteral("The GlobalShortcuts session is not configurable"));
        return;
    }
    if (!validateSessionUpdates(targetSession, shortcuts)) {
        sendError(message,
                  QString::fromLatin1(InvalidArgsError),
                  QStringLiteral("A conflict query is outside the session declaration"));
        return;
    }

    message.setDelayedReply(true);
    QDBusInterface service(QString::fromLatin1(ShortcutService),
                           QString::fromLatin1(ShortcutServicePath),
                           QString::fromLatin1(ShortcutServiceInterface),
                           QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
            service.asyncCall(QStringLiteral("CheckPortalShortcutConflicts"),
                              targetSession->appId(),
                              QVariant::fromValue(shortcuts)),
            this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, message] {
        QDBusPendingReply<PortalShortcutConflictList> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError())
            sendError(message, reply.error().name(), reply.error().message());
        else
            sendTypedReply(message, QVariant::fromValue(reply.value()));
    });
}

GlobalShortcutSession *GlobalShortcutsController::session(
        const QDBusObjectPath &handle) const
{
    return m_sessions.value(handle.path());
}

QString GlobalShortcutsController::caller(const QDBusMessage &message) const
{
    return message.service();
}

bool GlobalShortcutsController::verifyBrokerCaller(const QDBusMessage &message) const
{
    QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
    const QDBusReply<QString> owner = interface
            ? interface->serviceOwner(QString::fromLatin1(PortalBrokerService))
            : QDBusReply<QString>();
    return owner.isValid() && !owner.value().isEmpty() && owner.value() == message.service();
}

GlobalShortcutSession *GlobalShortcutsController::checkedSession(
        const QDBusObjectPath &handle,
        const QDBusMessage &message) const
{
    if (!verifyBrokerCaller(message))
        return nullptr;
    GlobalShortcutSession *targetSession = session(handle);
    if (!targetSession || targetSession->state() == GlobalShortcutSession::State::Closed)
        return nullptr;
    const QString owner = caller(message);
    return !owner.isEmpty() && owner == targetSession->brokerOwner() ? targetSession : nullptr;
}

bool GlobalShortcutsController::validateShortcuts(const XdgShortcutList &shortcuts) const
{
    if (shortcuts.size() > MaximumShortcutsPerApplication)
        return false;

    QSet<QString> ids;
    for (const XdgShortcut &shortcut : shortcuts) {
        if (shortcut.id.isEmpty() || shortcut.id.size() > MaximumShortcutIdLength
                || containsControlCharacter(shortcut.id) || ids.contains(shortcut.id)) {
            return false;
        }
        ids.insert(shortcut.id);

        const auto description = shortcut.properties.constFind(QStringLiteral("description"));
        if (description == shortcut.properties.cend()
                || description->metaType() != QMetaType::fromType<QString>()
                || description->toString().size() > MaximumDescriptionLength
                || containsControlCharacter(description->toString())) {
            return false;
        }
        const auto preferred = shortcut.properties.constFind(QStringLiteral("preferred_trigger"));
        if (preferred != shortcut.properties.cend()
                && (preferred->metaType() != QMetaType::fromType<QString>()
                    || preferred->toString().isEmpty()
                    || preferred->toString().size() > MaximumPreferredTriggerLength
                    || containsControlCharacter(preferred->toString()))) {
            return false;
        }
    }
    return true;
}

bool GlobalShortcutsController::validateSessionUpdates(
        const GlobalShortcutSession *targetSession,
        const PortalShortcutUpdateList &updates) const
{
    if (!targetSession || updates.size() > MaximumShortcutsPerApplication)
        return false;

    QSet<QString> ids;
    for (const PortalShortcutUpdate &update : updates) {
        if (update.shortcutId.isEmpty() || ids.contains(update.shortcutId)
                || !targetSession->declaredProperties().contains(update.shortcutId)) {
            return false;
        }
        ids.insert(update.shortcutId);
    }
    return true;
}

bool GlobalShortcutsController::verifyProviderCaller(
        const QDBusMessage &message,
        QString *errorName,
        QString *errorText) const
{
    QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
    const QDBusReply<QString> owner = interface
            ? interface->serviceOwner(QString::fromLatin1(ProviderService))
            : QDBusReply<QString>();
    if (owner.isValid() && !owner.value().isEmpty() && owner.value() == message.service())
        return true;
    if (errorName)
        *errorName = QString::fromLatin1(AccessDeniedError);
    if (errorText)
        *errorText = QStringLiteral("Only the current GlobalShortcuts provider may use this method");
    return false;
}

GlobalShortcutRequest *GlobalShortcutsController::createRequest(
        const QDBusObjectPath &handle,
        const QString &owner)
{
    if (handle.path().isEmpty() || owner.isEmpty()
            || m_bindRequestByPath.contains(handle.path())
            || m_listRequests.contains(handle.path())
            || m_bindRequests.size() + m_listRequests.size()
                    >= MaximumPendingRequests) {
        return nullptr;
    }
    auto *request = new GlobalShortcutRequest(handle, owner, this);
    if (!request->registerObject()) {
        delete request;
        return nullptr;
    }
    connect(request,
            &GlobalShortcutRequest::closeRequested,
            this,
            &GlobalShortcutsController::closeRequest);
    return request;
}

void GlobalShortcutsController::closeSession(GlobalShortcutSession *targetSession)
{
    if (!targetSession)
        return;
    const QString path = targetSession->path();

    const QStringList bindIds = m_bindRequests.keys();
    for (const QString &requestId : bindIds) {
        const auto pending = m_bindRequests.value(requestId);
        if (!pending || pending->sessionPath != path)
            continue;
        pending->sessionClosed = true;
        const bool responseSuppressed = pending->requestClosed;
        if (pending->phase == BindPhase::WaitingProvider)
            cancelProviderRequest(requestId);
        if (pending->phase == BindPhase::Committing) {
            if (!responseSuppressed)
                sendBackendReply(pending->replyMessage, OtherError, {});
            // The service transaction is already in progress. Keep the
            // pending state until it finishes, but never complete the backend
            // invocation a second time.
            pending->requestClosed = true;
            if (pending->request)
                pending->request->deleteLater();
            pending->request.clear();
        } else if (responseSuppressed) {
            cancelBind(requestId, false);
        } else {
            finishBind(requestId, OtherError);
        }
    }

    const QStringList listPaths = m_listRequests.keys();
    for (const QString &requestPath : listPaths) {
        const auto pending = m_listRequests.value(requestPath);
        if (!pending || pending->session != targetSession)
            continue;
        finishList(requestPath, OtherError);
    }

    cancelProviderSession(path);
    callDeactivate(targetSession->appId(), targetSession->sessionToken());
    if (m_configureRequests.value(path) == targetSession)
        m_configureRequests.remove(path);
    m_sessionCommitRequests.remove(path);
    m_sessionReplayIds.remove(path);
    m_sessionPressedIds.remove(path);
    m_sessions.remove(path);
    targetSession->deleteLater();
}

void GlobalShortcutsController::closeRequest(GlobalShortcutRequest *request)
{
    if (!request)
        return;
    const QString path = request->path();
    const QString bindId = m_bindRequestByPath.value(path);
    if (!bindId.isEmpty()) {
        const auto pending = m_bindRequests.value(bindId);
        if (!pending)
            return;
        sendBackendReply(pending->replyMessage, Cancelled, {});
        pending->requestClosed = true;
        pending->request.clear();
        request->deleteLater();
        if (pending->phase != BindPhase::Committing)
            cancelBind(bindId, true);
        return;
    }

    const auto list = m_listRequests.take(path);
    if (list) {
        sendBackendReply(list->replyMessage, Cancelled, {});
        list->closed = true;
        list->request.clear();
    }
    request->deleteLater();
}

void GlobalShortcutsController::cancelBind(const QString &requestId, bool closeProvider)
{
    const auto pending = m_bindRequests.value(requestId);
    if (!pending)
        return;
    if (closeProvider && pending->phase == BindPhase::WaitingProvider)
        cancelProviderRequest(requestId);
    if (pending->session && pending->session->state() != GlobalShortcutSession::State::Closed)
        pending->session->failBind();
    m_bindRequests.remove(requestId);
    m_bindRequestByPath.remove(pending->requestPath);
    if (pending->request)
        pending->request->deleteLater();
}

void GlobalShortcutsController::startBindHistory(const QString &requestId)
{
    const auto pending = m_bindRequests.value(requestId);
    if (!pending)
        return;
    pending->phase = BindPhase::LoadingHistory;
    QDBusInterface service(QString::fromLatin1(ShortcutService),
                           QString::fromLatin1(ShortcutServicePath),
                           QString::fromLatin1(ShortcutServiceInterface),
                           QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
            service.asyncCall(QStringLiteral("ListPortalShortcuts"), pending->appId),
            this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, requestId] {
        QDBusPendingReply<PortalShortcutInfoList> reply = *watcher;
        watcher->deleteLater();
        const auto pending = m_bindRequests.value(requestId);
        if (!pending)
            return;
        if (reply.isError()) {
            finishBind(requestId, OtherError);
            return;
        }
        pending->history = reply.value();
        startTriggerNormalization(requestId);
    });
}

void GlobalShortcutsController::startTriggerNormalization(const QString &requestId)
{
    const auto pending = m_bindRequests.value(requestId);
    if (!pending)
        return;
    PortalTriggerList requested;
    for (const XdgShortcut &shortcut : pending->declared) {
        const auto preferred = shortcut.properties.constFind(QStringLiteral("preferred_trigger"));
        if (preferred != shortcut.properties.cend())
            requested.append({shortcut.id, preferred->toString()});
    }
    if (requested.isEmpty()) {
        continueBindAfterNormalization(requestId, {});
        return;
    }

    pending->phase = BindPhase::Normalizing;
    QDBusInterface service(QString::fromLatin1(ShortcutService),
                           QString::fromLatin1(ShortcutServicePath),
                           QString::fromLatin1(ShortcutServiceInterface),
                           QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
            service.asyncCall(QStringLiteral("NormalizeXdgPortalTriggers"),
                              QVariant::fromValue(requested)),
            this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, requestId] {
        QDBusPendingReply<PortalTriggerList> reply = *watcher;
        watcher->deleteLater();
        if (!m_bindRequests.contains(requestId))
            return;
        if (reply.isError())
            finishBind(requestId, OtherError);
        else
            continueBindAfterNormalization(requestId, reply.value());
    });
}

void GlobalShortcutsController::continueBindAfterNormalization(
        const QString &requestId,
        const PortalTriggerList &normalized)
{
    const auto pending = m_bindRequests.value(requestId);
    if (!pending)
        return;
    QHash<QString, QString> preferred;
    for (const PortalTrigger &trigger : normalized)
        preferred.insert(trigger.shortcutId, trigger.trigger);
    const auto history = byShortcutId(pending->history);

    pending->pendingNewIds.clear();
    for (PortalShortcutUpdate &update : pending->updates) {
        const QString preferredKey = preferred.value(update.shortcutId);
        if (!preferredKey.isEmpty())
            update.preferredKeys = {preferredKey};
        const auto stored = history.constFind(update.shortcutId);
        if (stored != history.cend() && stored->enabled && !stored->hotkeys.isEmpty())
            continue;
        pending->pendingNewIds.insert(update.shortcutId);
        if (stored != history.cend()) {
            update.hotkeys = stored->hotkeys;
            update.enabled = stored->enabled;
        } else {
            update.hotkeys = update.preferredKeys;
            update.enabled = !update.hotkeys.isEmpty();
        }
    }

    if (pending->pendingNewIds.isEmpty())
        commitBind(requestId);
    else
        startProviderBind(requestId);
}

void GlobalShortcutsController::startProviderBind(const QString &requestId)
{
    const auto pending = m_bindRequests.value(requestId);
    if (!pending)
        return;
    PortalShortcutUpdateList newShortcuts;
    for (const PortalShortcutUpdate &update : pending->updates) {
        if (pending->pendingNewIds.contains(update.shortcutId))
            newShortcuts.append(update);
    }

    pending->phase = BindPhase::WaitingProvider;
    QDBusInterface provider(QString::fromLatin1(ProviderService),
                            QString::fromLatin1(ProviderPath),
                            QString::fromLatin1(ProviderInterface),
                            QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
            provider.asyncCall(QStringLiteral("BeginBind"),
                               requestId,
                               QVariant::fromValue(QDBusObjectPath(pending->sessionPath)),
                               pending->appId,
                               pending->parentWindow,
                               QVariant::fromValue(newShortcuts)),
            this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, requestId] {
        QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError() && m_bindRequests.contains(requestId))
            finishBind(requestId, OtherError);
    });
}

void GlobalShortcutsController::commitBind(const QString &requestId)
{
    const auto pending = m_bindRequests.value(requestId);
    if (!pending)
        return;
    pending->phase = BindPhase::Committing;

    QDBusInterface service(QString::fromLatin1(ShortcutService),
                           QString::fromLatin1(ShortcutServicePath),
                           QString::fromLatin1(ShortcutServiceInterface),
                           QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
            service.asyncCall(QStringLiteral("CommitPortalSessionBindings"),
                              pending->appId,
                              pending->sessionToken,
                              QVariant::fromValue(pending->updates)),
            this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, requestId] {
        QDBusPendingReply<PortalShortcutInfoList> reply = *watcher;
        watcher->deleteLater();
        if (!m_bindRequests.contains(requestId))
            return;
        finishBind(requestId,
                   reply.isError() ? OtherError : Success,
                   reply.isError() ? PortalShortcutInfoList{} : reply.value());
    });
}

void GlobalShortcutsController::finishBind(
        const QString &requestId,
        uint response,
        const PortalShortcutInfoList &bound)
{
    const auto pending = m_bindRequests.take(requestId);
    if (!pending)
        return;
    m_bindRequestByPath.remove(pending->requestPath);

    QVariantMap results;
    if (response == Success && pending->session && !pending->sessionClosed
            && pending->session->state() != GlobalShortcutSession::State::Closed) {
        const XdgShortcutList shortcuts = toXdgShortcuts(bound, pending->session, false);
        pending->session->finishBind(shortcuts);
        m_sessionCommitRequests.insert(pending->sessionPath, pending->updates);
        m_sessionReplayIds.insert(pending->sessionPath, pending->session->boundIds());
        results.insert(QStringLiteral("shortcuts"), QVariant::fromValue(shortcuts));
    } else {
        if (pending->session && pending->session->state() != GlobalShortcutSession::State::Closed)
            pending->session->failBind();
        if (response == Success)
            callDeactivate(pending->appId, pending->sessionToken);
    }

    if (!pending->requestClosed)
        sendBackendReply(pending->replyMessage, response, results);
    if (pending->request)
        pending->request->deleteLater();
}

void GlobalShortcutsController::finishList(
        const QString &requestPath,
        uint response,
        const PortalShortcutInfoList &shortcuts)
{
    const auto pending = m_listRequests.take(requestPath);
    if (!pending)
        return;
    QVariantMap results;
    if (response == Success) {
        results.insert(QStringLiteral("shortcuts"),
                       QVariant::fromValue(toXdgShortcuts(shortcuts, nullptr, false)));
    }
    if (!pending->closed)
        sendBackendReply(pending->replyMessage, response, results);
    if (pending->request)
        pending->request->deleteLater();
}

void GlobalShortcutsController::callDeactivate(const QString &appId,
                                               const QString &sessionToken)
{
    if (appId.isEmpty() || sessionToken.isEmpty())
        return;
    QDBusInterface service(QString::fromLatin1(ShortcutService),
                           QString::fromLatin1(ShortcutServicePath),
                           QString::fromLatin1(ShortcutServiceInterface),
                           QDBusConnection::sessionBus());
    service.asyncCall(QStringLiteral("DeactivatePortalShortcuts"), appId, sessionToken);
}

void GlobalShortcutsController::cancelProviderRequest(const QString &requestId)
{
    QDBusInterface provider(QString::fromLatin1(ProviderService),
                            QString::fromLatin1(ProviderPath),
                            QString::fromLatin1(ProviderInterface),
                            QDBusConnection::sessionBus());
    provider.asyncCall(QStringLiteral("CancelRequest"), requestId);
}

void GlobalShortcutsController::cancelProviderSession(const QString &sessionPath)
{
    // CancelRequest also accepts a session path in the DDE provider so a
    // Configure window can be closed without adding a second wire method.
    cancelProviderRequest(sessionPath);
}

void GlobalShortcutsController::sendBackendReply(
        const QDBusMessage &message,
        uint response,
        const QVariantMap &results) const
{
    QDBusConnection::sessionBus().send(
            message.createReply({QVariant::fromValue(response), QVariant::fromValue(results)}));
}

void GlobalShortcutsController::sendVoidReply(const QDBusMessage &message) const
{
    QDBusConnection::sessionBus().send(message.createReply());
}

void GlobalShortcutsController::sendError(const QDBusMessage &message,
                                          const QString &name,
                                          const QString &text) const
{
    if (message.type() != QDBusMessage::MethodCallMessage)
        return;
    message.setDelayedReply(true);
    QDBusConnection::sessionBus().send(message.createErrorReply(name, text));
}

void GlobalShortcutsController::sendTypedReply(const QDBusMessage &message,
                                               const QVariant &result) const
{
    QDBusConnection::sessionBus().send(message.createReply({result}));
}

XdgShortcutList GlobalShortcutsController::toXdgShortcuts(
        const PortalShortcutInfoList &shortcuts,
        const GlobalShortcutSession *targetSession,
        bool requireActive) const
{
    const auto records = byShortcutId(shortcuts);
    XdgShortcutList result;
    const auto appendRecord = [&](const QString &shortcutId, const PortalShortcutInfo &record) {
        if (!record.enabled || record.hotkeys.isEmpty() || (requireActive && !record.active))
            return;
        QVariantMap properties;
        QString description = record.description;
        if (targetSession) {
            description = targetSession->declaredProperties()
                                  .value(shortcutId)
                                  .value(QStringLiteral("description"))
                                  .toString();
        }
        properties.insert(QStringLiteral("description"), description);
        properties.insert(QStringLiteral("trigger_description"), triggerDescription(record));
        result.append({shortcutId, properties});
    };

    if (targetSession) {
        for (const QString &shortcutId : targetSession->declaredOrder()) {
            const auto it = records.constFind(shortcutId);
            if (it != records.cend())
                appendRecord(shortcutId, *it);
        }
    } else {
        for (const PortalShortcutInfo &record : shortcuts)
            appendRecord(record.shortcutId, record);
    }
    return result;
}

PortalShortcutUpdateList GlobalShortcutsController::initialUpdates(
        const XdgShortcutList &shortcuts) const
{
    PortalShortcutUpdateList updates;
    updates.reserve(shortcuts.size());
    for (const XdgShortcut &shortcut : shortcuts) {
        PortalShortcutUpdate update;
        update.shortcutId = shortcut.id;
        update.description = shortcut.properties.value(QStringLiteral("description")).toString();
        updates.append(update);
    }
    return updates;
}

QString GlobalShortcutsController::triggerDescription(const PortalShortcutInfo &shortcut)
{
    if (shortcut.hotkeys.isEmpty())
        return {};
    const QKeySequence sequence = QKeySequence::fromString(shortcut.hotkeys.constFirst(),
                                                            QKeySequence::PortableText);
    return sequence.isEmpty() ? shortcut.hotkeys.constFirst()
                              : sequence.toString(QKeySequence::NativeText);
}

QHash<QString, PortalShortcutInfo> GlobalShortcutsController::byShortcutId(
        const PortalShortcutInfoList &shortcuts)
{
    QHash<QString, PortalShortcutInfo> result;
    for (const PortalShortcutInfo &shortcut : shortcuts)
        result.insert(shortcut.shortcutId, shortcut);
    return result;
}

void GlobalShortcutsController::publishSessionShortcuts(
        GlobalShortcutSession *targetSession,
        const XdgShortcutList &shortcuts)
{
    if (!targetSession || shortcutsEqual(targetSession->activeShortcuts(), shortcuts))
        return;
    targetSession->updateBoundShortcuts(shortcuts);
    Q_EMIT shortcutsChanged(QDBusObjectPath(targetSession->path()), shortcuts);
}

void GlobalShortcutsController::onPortalShortcutPressed(
        const QString &appId,
        const QString &shortcutId,
        qulonglong timestamp)
{
    for (GlobalShortcutSession *targetSession : std::as_const(m_sessions)) {
        if (targetSession && targetSession->state() == GlobalShortcutSession::State::Bound
                && targetSession->appId() == appId
                && targetSession->boundIds().contains(shortcutId)
                && !m_sessionPressedIds.value(targetSession->path()).contains(shortcutId)) {
            m_sessionPressedIds[targetSession->path()].insert(shortcutId);
            Q_EMIT activated(QDBusObjectPath(targetSession->path()),
                             shortcutId,
                             timestamp,
                             {});
        }
    }
}

void GlobalShortcutsController::onPortalShortcutReleased(
        const QString &appId,
        const QString &shortcutId,
        qulonglong timestamp)
{
    for (GlobalShortcutSession *targetSession : std::as_const(m_sessions)) {
        if (targetSession && targetSession->state() == GlobalShortcutSession::State::Bound
                && targetSession->appId() == appId
                && m_sessionPressedIds[targetSession->path()].remove(shortcutId)) {
            Q_EMIT deactivated(QDBusObjectPath(targetSession->path()),
                               shortcutId,
                               timestamp,
                               {});
        }
    }
}

void GlobalShortcutsController::onPortalShortcutsChanged(
        const QString &appId,
        const PortalShortcutInfoList &shortcuts)
{
    for (GlobalShortcutSession *targetSession : std::as_const(m_sessions)) {
        if (!targetSession || targetSession->state() != GlobalShortcutSession::State::Bound
                || targetSession->appId() != appId) {
            continue;
        }
        PortalShortcutInfoList owned;
        for (const PortalShortcutInfo &shortcut : shortcuts) {
            if (targetSession->boundIds().contains(shortcut.shortcutId))
                owned.append(shortcut);
        }
        const XdgShortcutList current = toXdgShortcuts(owned, targetSession, true);
        // During service replay an empty public snapshot is temporary; unrelated
        // application changes must not overwrite its remembered recovery set.
        if (shortcutsEqual(targetSession->activeShortcuts(), current))
            continue;
        publishSessionShortcuts(targetSession, current);
        m_sessionReplayIds.insert(targetSession->path(), targetSession->boundIds());
    }
}

void GlobalShortcutsController::onPortalSessionShortcutsChanged(
        const QString &appId,
        const QString &sessionToken,
        const PortalShortcutInfoList &shortcuts)
{
    for (GlobalShortcutSession *targetSession : std::as_const(m_sessions)) {
        if (targetSession && targetSession->state() == GlobalShortcutSession::State::Bound
                && targetSession->appId() == appId
                && targetSession->sessionToken() == sessionToken) {
            publishSessionShortcuts(targetSession, toXdgShortcuts(shortcuts, targetSession, true));
            m_sessionReplayIds.insert(targetSession->path(), targetSession->boundIds());
            return;
        }
    }
}

void GlobalShortcutsController::onBrokerOwnerVanished(const QString &service)
{
    const QList<QPointer<GlobalShortcutSession>> sessions = m_sessions.values();
    for (GlobalShortcutSession *targetSession : sessions) {
        if (targetSession
                && (service == QLatin1String(PortalBrokerService)
                    || targetSession->brokerOwner() == service)) {
            closeSession(targetSession);
        }
    }
}

void GlobalShortcutsController::onProviderVanished()
{
    const QStringList requestIds = m_bindRequests.keys();
    for (const QString &requestId : requestIds) {
        const auto pending = m_bindRequests.value(requestId);
        if (pending && pending->phase == BindPhase::WaitingProvider)
            finishBind(requestId, OtherError);
    }
}

void GlobalShortcutsController::onServiceVanished()
{
    ++m_serviceGeneration;
    m_replayQueue.clear();
    m_replayInProgress = false;
    m_sessionPressedIds.clear();
    const QStringList requestIds = m_bindRequests.keys();
    for (const QString &requestId : requestIds) {
        const auto pending = m_bindRequests.value(requestId);
        if (pending && pending->phase == BindPhase::WaitingProvider)
            cancelProviderRequest(requestId);
        finishBind(requestId, OtherError);
    }
    const QStringList listPaths = m_listRequests.keys();
    for (const QString &requestPath : listPaths)
        finishList(requestPath, OtherError);

    for (GlobalShortcutSession *targetSession : std::as_const(m_sessions)) {
        if (!targetSession || targetSession->state() != GlobalShortcutSession::State::Bound)
            continue;
        // The replay set already tracks the last bindings confirmed by the
        // live service. After the first outage publishSessionShortcuts()
        // intentionally clears the public snapshot; a second outage before
        // replay finishes must not replace the remembered set with that
        // synthetic empty snapshot.
        if (!m_sessionReplayIds.contains(targetSession->path())) {
            m_sessionReplayIds.insert(targetSession->path(), targetSession->boundIds());
        }
        publishSessionShortcuts(targetSession, {});
    }
}

void GlobalShortcutsController::replayBoundSessions()
{
    if (m_replayInProgress)
        return;
    m_replayInProgress = true;
    for (GlobalShortcutSession *targetSession : std::as_const(m_sessions)) {
        if (targetSession && targetSession->state() == GlobalShortcutSession::State::Bound)
            m_replayQueue.enqueue(targetSession);
    }
    replayNextSession(m_serviceGeneration);
}

void GlobalShortcutsController::replayNextSession(quint64 generation, int retryCount)
{
    if (generation != m_serviceGeneration)
        return;
    while (!m_replayQueue.isEmpty()) {
        const auto target = m_replayQueue.head();
        if (target && target->state() == GlobalShortcutSession::State::Bound
                && m_sessions.value(target->path()) == target) {
            break;
        }
        m_replayQueue.dequeue();
        retryCount = 0;
    }
    if (m_replayQueue.isEmpty()) {
        m_replayInProgress = false;
        return;
    }

    const QPointer<GlobalShortcutSession> target = m_replayQueue.head();
    const QString appId = target->appId();
    const QString token = target->sessionToken();
    const auto replayIds = m_sessionReplayIds.value(target->path());
    QStringList activeIds(replayIds.cbegin(), replayIds.cend());
    std::sort(activeIds.begin(), activeIds.end());
    auto message = QDBusMessage::createMethodCall(QString::fromLatin1(ShortcutService),
                                                 QString::fromLatin1(ShortcutServicePath),
                                                 QString::fromLatin1(ShortcutServiceInterface),
                                                 QStringLiteral("RestorePortalSessionBindings"));
    message.setArguments({appId, token,
                          QVariant::fromValue(m_sessionCommitRequests.value(target->path())),
                          activeIds});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message),
                                               this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, target, appId, token, generation, retryCount] {
        const QDBusPendingReply<PortalShortcutInfoList> reply = *watcher;
        watcher->deleteLater();
        if (generation != m_serviceGeneration)
            return;

        const bool alive = target && target->state() == GlobalShortcutSession::State::Bound
                && m_sessions.value(target->path()) == target;
        constexpr int MaximumBusyRetries = 5;
        if (alive && reply.isError()
                && reply.error().name() == QLatin1String("org.deepin.dde.Keybinding1.Error.Busy")
                && retryCount < MaximumBusyRetries) {
            QTimer::singleShot(100 * (1 << retryCount), this, [this, generation, retryCount] {
                replayNextSession(generation, retryCount + 1);
            });
            return;
        }

        if (alive) {
            if (reply.isError()) {
                qCWarning(GlobalShortcutsControllerLog)
                        << "Failed to restore Portal session:" << reply.error().name();
            } else {
                publishSessionShortcuts(target, toXdgShortcuts(reply.value(), target, false));
                m_sessionReplayIds.insert(target->path(), target->boundIds());
            }
        } else if (!reply.isError()) {
            callDeactivate(appId, token);
        }
        m_replayQueue.dequeue();
        replayNextSession(generation);
    });
}
