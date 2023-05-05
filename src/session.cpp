// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "session.h"

#include "kglobalaccel_interface.h"
#include "kglobalaccel_component_interface.h"

#include <QLoggingCategory>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

Q_LOGGING_CATEGORY(XdgDesktopDDESession, "xdg-dde-session")
static QMap<QString, Session *> sessionList;

Session::Session(QObject *parent, const QString &appId, const QString &path)
    : QDBusVirtualObject(parent)
    , m_appId(appId)
    , m_path(path)
{
}

Session::~Session() {}

bool Session::handleMessage(const QDBusMessage &message, const QDBusConnection &connection)
{
    Q_UNUSED(connection);

    if (message.path() != m_path) {
        return false;
    }

    /* Check to make sure we're getting properties on our interface */
    if (message.type() != QDBusMessage::MessageType::MethodCallMessage) {
        return false;
    }

    qCDebug(XdgDesktopDDESession) << message.interface();
    qCDebug(XdgDesktopDDESession) << message.member();
    qCDebug(XdgDesktopDDESession) << message.path();

    if (message.interface() == QLatin1String("org.freedesktop.impl.portal.Session")) {
        if (message.member() == QLatin1String("Close")) {
            close();
            Q_EMIT closed();
            QDBusMessage reply = message.createReply();
            return connection.send(reply);
        }
    } else if (message.interface() == QLatin1String("org.freedesktop.DBus.Properties")) {
        if (message.member() == QLatin1String("Get")) {
            if (message.arguments().count() == 2) {
                const QString interface = message.arguments().at(0).toString();
                const QString property = message.arguments().at(1).toString();

                if (interface == QLatin1String("org.freedesktop.impl.portal.Session") && property == QLatin1String("version")) {
                    QList<QVariant> arguments;
                    arguments << 1;

                    QDBusMessage reply = message.createReply();
                    reply.setArguments(arguments);
                    return connection.send(reply);
                }
            }
        }
    }

    return false;
}

QString Session::introspect(const QString &path) const
{
    QString nodes;

    if (path.startsWith(QLatin1String("/org/freedesktop/portal/desktop/session/"))) {
        nodes = QStringLiteral("<interface name=\"org.freedesktop.impl.portal.Session\">"
                               "    <method name=\"Close\">"
                               "    </method>"
                               "<signal name=\"Closed\">"
                               "</signal>"
                               "<property name=\"version\" type=\"u\" access=\"read\"/>"
                               "</interface>");
    }

    qCDebug(XdgDesktopDDESession) << nodes;

    return nodes;
}

bool Session::close()
{
    QDBusMessage reply{
        QDBusMessage::createSignal(m_path, QStringLiteral("org.freedesktop.impl.portal.Session"), QStringLiteral("Closed"))};
    const bool result{QDBusConnection::sessionBus().send(reply)};

    sessionList.remove(m_path);
    QDBusConnection::sessionBus().unregisterObject(m_path);

    deleteLater();

    return result;
}

Session *Session::createSession(QObject *parent, SessionType type, const QString &appId, const QString &path)
{
    QDBusConnection sessionBus = QDBusConnection::sessionBus();

    Session *session = nullptr;
    switch (type) {
        case GlobalShortcuts:
            session = new GlobalShortcutsSession(parent, appId, path);
            break;
    }

    if (sessionBus.registerVirtualObject(path, session, QDBusConnection::VirtualObjectRegisterOption::SubPath)) {
        connect(session, &Session::closed, [session, path]() {
            sessionList.remove(path);
            QDBusConnection::sessionBus().unregisterObject(path);
            session->deleteLater();
        });
        sessionList.insert(path, session);
        return session;
    } else {
        qCDebug(XdgDesktopDDESession) << sessionBus.lastError().message();
        qCDebug(XdgDesktopDDESession) << "Failed to register session object: " << path;
        session->deleteLater();
        return nullptr;
    }
}

Session *Session::getSession(const QString &sessionHandle)
{
    return sessionList.value(sessionHandle);
}

GlobalShortcutsSession::GlobalShortcutsSession(QObject *parent, const QString &appId, const QString &path)
    : Session(parent, appId, path)
    , m_token(path.mid(path.lastIndexOf('/') + 1))
    , m_globalAccelInterface(new KGlobalAccelInterface(
          QStringLiteral("org.kde.kglobalaccel"), QStringLiteral("/kglobalaccel"), QDBusConnection::sessionBus(), this))
    , m_component(new KGlobalAccelComponentInterface(m_globalAccelInterface->service(),
                                                     m_globalAccelInterface->getComponent(componentName()).value().path(),
                                                     m_globalAccelInterface->connection(),
                                                     this))
{
    qDBusRegisterMetaType<KGlobalShortcutInfo>();
    qDBusRegisterMetaType<QList<KGlobalShortcutInfo>>();
    qDBusRegisterMetaType<QKeySequence>();
    qDBusRegisterMetaType<QList<QKeySequence>>();

    connect(m_globalAccelInterface,
            &KGlobalAccelInterface::yourShortcutsChanged,
            this,
            [this](const QStringList &actionId, const QList<QKeySequence> &newKeys) {
                Q_UNUSED(newKeys);
                if (actionId[KGlobalAccel::ComponentUnique] == componentName()) {
                    m_shortcuts[actionId[KGlobalAccel::ActionUnique]]->setShortcuts(newKeys);
                    Q_EMIT shortcutsChanged();
                }
            });
    connect(m_component,
            &KGlobalAccelComponentInterface::globalShortcutPressed,
            this,
            [this](const QString &componentUnique, const QString &actionUnique, qlonglong timestamp) {
                if (componentUnique != componentName()) {
                    return;
                }

                Q_EMIT shortcutActivated(actionUnique, timestamp);
            });
    connect(m_component,
            &KGlobalAccelComponentInterface::globalShortcutReleased,
            this,
            [this](const QString &componentUnique, const QString &actionUnique, qlonglong timestamp) {
                if (componentUnique != componentName()) {
                    return;
                }

                Q_EMIT shortcutDeactivated(actionUnique, timestamp);
            });
}

GlobalShortcutsSession::~GlobalShortcutsSession() = default;

void GlobalShortcutsSession::restoreActions(const QVariant &shortcutsVariant)
{
    Shortcuts shortcuts;
    auto arg = shortcutsVariant.value<QDBusArgument>();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    static const auto ShortcutsSignature = QDBusMetaType::typeToSignature(qMetaTypeId<Shortcuts>());
#else
    static const auto ShortcutsSignature = QDBusMetaType::typeToSignature(QMetaType(qMetaTypeId<Shortcuts>()));
#endif
    if (arg.currentSignature() != QLatin1String(ShortcutsSignature)) {
        qCWarning(XdgDesktopDDESession) << "Wrong global shortcuts type, should be " << ShortcutsSignature << "instead of "
                                        << arg.currentSignature();
        return;
    }
    arg >> shortcuts;

    const QList<KGlobalShortcutInfo> shortcutInfos = m_component->allShortcutInfos();
    QHash<QString, KGlobalShortcutInfo> shortcutInfosByName;
    shortcutInfosByName.reserve(shortcutInfos.size());
    for (const auto &shortcutInfo : shortcutInfos) {
        shortcutInfosByName[shortcutInfo.uniqueName()] = shortcutInfo;
    }

    for (const auto &shortcut : shortcuts) {
        const QString description = shortcut.second["description"].toString();
        if (description.isEmpty() || shortcut.first.isEmpty()) {
            qCWarning(XdgDesktopDDESession) << "Shortcut without name or description" << shortcut.first << "for"
                                            << componentName();
            continue;
        }

        QAction *&action = m_shortcuts[shortcut.first];
        if (!action) {
            action = new QAction(this);
        }
        action->setProperty("componentName", componentName());
        action->setProperty("componentDisplayName", componentName());
        action->setObjectName(shortcut.first);
        action->setText(description);
        const auto itShortcut = shortcutInfosByName.constFind(shortcut.first);
        if (itShortcut != shortcutInfosByName.constEnd()) {
            action->setShortcuts(itShortcut->keys());
        }
        KGlobalAccel::self()->setGlobalShortcut(action, action->shortcuts());

        shortcutInfosByName.remove(shortcut.first);
    }

    // We can forget the shortcuts that aren't around anymore
    while (!shortcutInfosByName.isEmpty()) {
        const QString shortcutName = shortcutInfosByName.begin().key();
        auto action = m_shortcuts.take(shortcutName);
        KGlobalAccel::self()->removeAllShortcuts(action);
        shortcutInfosByName.erase(shortcutInfosByName.begin());
    }

    Q_ASSERT(m_shortcuts.size() == shortcuts.size());
}

QVariant GlobalShortcutsSession::shortcutDescriptionsVariant() const
{
    QDBusArgument retVar;
    retVar << shortcutDescriptions();
    return QVariant::fromValue(retVar);
}

Shortcuts GlobalShortcutsSession::shortcutDescriptions() const
{
    Shortcuts ret;
    for (auto it = m_shortcuts.cbegin(), itEnd = m_shortcuts.cend(); it != itEnd; ++it) {
        QStringList triggers;
        triggers.reserve((*it)->shortcuts().size());
        const auto shortcuts = (*it)->shortcuts();
        for (const auto &shortcut : shortcuts) {
            triggers += shortcut.toString(QKeySequence::NativeText);
        }

        ret.append({it.key(),
                    QVariantMap{
                        {QStringLiteral("description"), (*it)->text()},
                        {QStringLiteral("trigger_description"), triggers.join(", ")},
                    }});
    }
    Q_ASSERT(ret.size() == m_shortcuts.size());
    return ret;
}
