// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "secret.h"

#include <QLoggingCategory>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QDBusMetaType>
#include <QDBusUnixFileDescriptor>
#include <QRandomGenerator>

#include <unistd.h>

Q_LOGGING_CATEGORY(XdgDesktopDDESecret, "xdg-dde-secret")

// Secret Service D-Bus API secret struct: (o, ay, ay, s)
struct SecretStruct
{
    QDBusObjectPath session;
    QByteArray parameters;
    QByteArray value;
    QString contentType;
};
Q_DECLARE_METATYPE(SecretStruct)

QDBusArgument &operator<<(QDBusArgument &argument, const SecretStruct &secret)
{
    argument.beginStructure();
    argument << secret.session << secret.parameters << secret.value << secret.contentType;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SecretStruct &secret)
{
    argument.beginStructure();
    argument >> secret.session >> secret.parameters >> secret.value >> secret.contentType;
    argument.endStructure();
    return argument;
}

SecretPortal::SecretPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qRegisterMetaType<SecretStruct>();
    qDBusRegisterMetaType<SecretStruct>();
    qCDebug(XdgDesktopDDESecret) << "secret init";
}

static void closeSecretSession(const QDBusObjectPath &sessionPath)
{
    QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.secrets"),
        sessionPath.path(),
        QStringLiteral("org.freedesktop.Secret.Session"),
        QStringLiteral("Close")));
}

static QList<QDBusObjectPath> extractObjectPathList(const QVariant &variant)
{
    QList<QDBusObjectPath> result;
    if (!variant.isValid())
        return result;

    if (variant.canConvert<QList<QDBusObjectPath>>()) {
        return variant.value<QList<QDBusObjectPath>>();
    }

    const QDBusArgument &arg = variant.value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd()) {
        QDBusObjectPath path;
        arg >> path;
        result.append(path);
    }
    arg.endArray();
    return result;
}

uint SecretPortal::RetrieveSecret(const QDBusObjectPath &handle,
                                  const QString &app_id,
                                  const QDBusUnixFileDescriptor &fd,
                                  const QVariantMap &options,
                                  QVariantMap &result)
{
    qCDebug(XdgDesktopDDESecret) << "RetrieveSecret for app_id:" << app_id;

    const QString service = QStringLiteral("org.freedesktop.secrets");
    const QString servicePath = QStringLiteral("/org/freedesktop/secrets");
    const QString serviceIface = QStringLiteral("org.freedesktop.Secret.Service");

    // 1. Open a plain session
    QDBusMessage openMsg = QDBusMessage::createMethodCall(
        service, servicePath, serviceIface, QStringLiteral("OpenSession"));
    openMsg << QVariant(QStringLiteral("plain"))
            << QVariant::fromValue(QDBusVariant(QByteArray()));

    QDBusMessage openReply = QDBusConnection::sessionBus().call(openMsg);
    if (openReply.type() != QDBusMessage::ReplyMessage || openReply.arguments().size() < 2) {
        qCWarning(XdgDesktopDDESecret) << "Failed to open session:" << openReply.errorMessage();
        return 1;
    }

    QDBusObjectPath sessionPath = openReply.arguments().at(1).value<QDBusObjectPath>();
    if (sessionPath.path().isEmpty()) {
        qCWarning(XdgDesktopDDESecret) << "Empty session path from OpenSession";
        return 1;
    }

    QByteArray secretValue;

    // 2. Search for items with {"app_id": app_id}
    QVariantMap attributes;
    attributes.insert(QStringLiteral("app_id"), app_id);

    QDBusMessage searchMsg = QDBusMessage::createMethodCall(
        service, servicePath, serviceIface, QStringLiteral("SearchItems"));
    searchMsg << QVariant(attributes);

    QDBusMessage searchReply = QDBusConnection::sessionBus().call(searchMsg);
    if (searchReply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(XdgDesktopDDESecret) << "Failed to search items:" << searchReply.errorMessage();
        closeSecretSession(sessionPath);
        return 1;
    }

    QList<QDBusObjectPath> unlockedItems;
    if (searchReply.arguments().size() >= 1) {
        unlockedItems = extractObjectPathList(searchReply.arguments().at(0));
    }

    if (!unlockedItems.isEmpty()) {
        // 3a. Get the secret from the first found item
        QList<QDBusObjectPath> items;
        items.append(unlockedItems.first());

        QDBusMessage getSecretsMsg = QDBusMessage::createMethodCall(
            service, servicePath, serviceIface, QStringLiteral("GetSecrets"));
        getSecretsMsg << QVariant::fromValue(items) << QVariant::fromValue(sessionPath);

        QDBusMessage getSecretsReply = QDBusConnection::sessionBus().call(getSecretsMsg);
        if (getSecretsReply.type() == QDBusMessage::ReplyMessage
            && !getSecretsReply.arguments().isEmpty()) {
            // Parse a{o(oayays)}
            const QDBusArgument &secretsArg =
                getSecretsReply.arguments().at(0).value<QDBusArgument>();
            secretsArg.beginMap();
            while (!secretsArg.atEnd()) {
                QDBusObjectPath itemPath;
                SecretStruct secret;
                secretsArg.beginMapEntry();
                secretsArg >> itemPath >> secret;
                secretsArg.endMapEntry();
                secretValue = secret.value;
                break;
            }
            secretsArg.endMap();
        }
    }

    if (secretValue.isEmpty()) {
        // 3b. Create a new random 64-byte secret
        secretValue.resize(64);
        QRandomGenerator::system()->fillRange(
            reinterpret_cast<quint32 *>(secretValue.data()), 16);

        // Read default collection alias
        QDBusMessage readAliasMsg = QDBusMessage::createMethodCall(
            service, servicePath, serviceIface, QStringLiteral("ReadAlias"));
        readAliasMsg << QVariant(QStringLiteral("default"));

        QDBusMessage readAliasReply = QDBusConnection::sessionBus().call(readAliasMsg);
        if (readAliasReply.type() != QDBusMessage::ReplyMessage
            || readAliasReply.arguments().isEmpty()) {
            qCWarning(XdgDesktopDDESecret)
                << "Failed to read default collection:" << readAliasReply.errorMessage();
            closeSecretSession(sessionPath);
            return 1;
        }

        QDBusObjectPath collectionPath =
            readAliasReply.arguments().at(0).value<QDBusObjectPath>();
        if (collectionPath.path().isEmpty() || collectionPath.path() == QStringLiteral("/")) {
            qCWarning(XdgDesktopDDESecret) << "No default collection available";
            closeSecretSession(sessionPath);
            return 1;
        }

        // Build item properties
        QMap<QString, QString> itemAttrs;
        itemAttrs.insert(QStringLiteral("app_id"), app_id);

        QVariantMap properties;
        properties.insert(QStringLiteral("org.freedesktop.Secret.Item.Attributes"),
                          QVariant::fromValue(itemAttrs));
        properties.insert(QStringLiteral("org.freedesktop.Secret.Item.Label"),
                          QVariant(app_id));

        // Build secret struct
        SecretStruct secretStruct;
        secretStruct.session = sessionPath;
        secretStruct.parameters = QByteArray();
        secretStruct.value = secretValue;
        secretStruct.contentType = QStringLiteral("application/octet-stream");

        // Create item in collection
        QDBusMessage createItemMsg = QDBusMessage::createMethodCall(
            service, collectionPath.path(),
            QStringLiteral("org.freedesktop.Secret.Collection"),
            QStringLiteral("CreateItem"));
        createItemMsg << QVariant::fromValue(properties)
                      << QVariant::fromValue(secretStruct)
                      << QVariant(true);

        QDBusMessage createItemReply = QDBusConnection::sessionBus().call(createItemMsg);
        if (createItemReply.type() != QDBusMessage::ReplyMessage) {
            qCWarning(XdgDesktopDDESecret)
                << "Failed to create secret item:" << createItemReply.errorMessage();
            closeSecretSession(sessionPath);
            return 1;
        }
    }

    // 4. Write secret to fd
    int fdNum = fd.fileDescriptor();
    if (fdNum < 0) {
        qCWarning(XdgDesktopDDESecret) << "Invalid file descriptor";
        closeSecretSession(sessionPath);
        return 1;
    }

    ssize_t written = ::write(fdNum, secretValue.constData(), secretValue.size());
    if (written != secretValue.size()) {
        qCWarning(XdgDesktopDDESecret)
            << "Failed to write secret to fd, wrote" << written << "of" << secretValue.size();
        closeSecretSession(sessionPath);
        return 1;
    }

    // 5. Close session
    closeSecretSession(sessionPath);

    qCDebug(XdgDesktopDDESecret) << "RetrieveSecret succeeded for app_id:" << app_id;
    return 0;
}
