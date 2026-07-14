// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dbushelpers.h"
#include "settings.h"

#include <QApplication>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QMetaMethod>

#include <cstring>
#include <iostream>

namespace {

bool expectMethodReturnSignature(const QMetaObject &metaObject,
                                 const char *methodName,
                                 const char *expectedSignature)
{
    for (int index = metaObject.methodOffset(); index < metaObject.methodCount(); ++index) {
        const QMetaMethod method = metaObject.method(index);
        if (method.methodType() != QMetaMethod::Slot || method.name() != methodName) {
            continue;
        }

        const char *signature = QDBusMetaType::typeToSignature(method.returnMetaType());
        const char *actualSignature = signature && *signature ? signature : "<none>";
        if (std::strcmp(actualSignature, expectedSignature) != 0) {
            std::cerr << "Expected " << methodName << " D-Bus return signature "
                      << expectedSignature << ", got " << actualSignature << '\n';
            return false;
        }
        return true;
    }

    std::cerr << "Could not find Settings slot " << methodName << '\n';
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    qunsetenv("QT_SCALE_FACTOR_ROUNDING_POLICY");
    QApplication app(argc, argv);
    QObject parent;
    SettingsPortal portal(&parent);

    if (!expectMethodReturnSignature(SettingsPortal::staticMetaObject, "ReadAll", "a{sa{sv}}")
        || !expectMethodReturnSignature(SettingsPortal::staticMetaObject, "Read", "v")) {
        return 1;
    }

    VariantMapMap settings;
    const bool invoked = QMetaObject::invokeMethod(
            &portal,
            "ReadAll",
            Qt::DirectConnection,
            Q_RETURN_ARG(VariantMapMap, settings),
            Q_ARG(QStringList, QStringList{ QStringLiteral("org.freedesktop.appearance") }));
    if (!invoked) {
        std::cerr << "Could not invoke Settings.ReadAll with a VariantMapMap return value\n";
        return 1;
    }

    const QVariantMap appearance = settings.value(QStringLiteral("org.freedesktop.appearance"));
    if (!appearance.contains(QStringLiteral("color-scheme"))
        || !appearance.contains(QStringLiteral("accent-color"))) {
        std::cerr << "ReadAll did not return both appearance settings\n";
        return 1;
    }

    const QMetaType accentType = appearance.value(QStringLiteral("accent-color")).metaType();
    const char *accentSignature = QDBusMetaType::typeToSignature(accentType);
    const char *actualAccentSignature =
            accentSignature && *accentSignature ? accentSignature : "<unregistered>";
    if (std::strcmp(actualAccentSignature, "(ddd)") != 0) {
        std::cerr << "Expected accent-color D-Bus signature (ddd), got " << actualAccentSignature
                  << '\n';
        return 1;
    }

    QDBusVariant colorScheme;
    const bool readInvoked =
            QMetaObject::invokeMethod(&portal,
                                      "Read",
                                      Qt::DirectConnection,
                                      Q_RETURN_ARG(QDBusVariant, colorScheme),
                                      Q_ARG(QString, QStringLiteral("org.freedesktop.appearance")),
                                      Q_ARG(QString, QStringLiteral("color-scheme")),
                                      Q_ARG(QDBusMessage, QDBusMessage()));
    if (!readInvoked || colorScheme.variant().metaType() != QMetaType::fromType<uint>()) {
        std::cerr << "Settings.Read did not return an unsigned color-scheme variant\n";
        return 1;
    }

    return 0;
}
