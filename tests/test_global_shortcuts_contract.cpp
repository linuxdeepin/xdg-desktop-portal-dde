// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "globalshortcut.h"
#include "globalshortcuttypes.h"

#include <QCoreApplication>
#include <QDBusMetaType>
#include <QFile>
#include <QMetaMethod>
#include <QXmlStreamReader>

#include <iostream>

namespace {

bool expectTypeSignature(QMetaType type, const char *expected)
{
    const char *signature = QDBusMetaType::typeToSignature(type);
    const QByteArray actual = signature ? QByteArray(signature) : QByteArray();
    if (actual != expected) {
        std::cerr << "Expected D-Bus signature " << expected << ", got "
                  << (actual.isEmpty() ? "<unregistered>" : actual.constData()) << '\n';
        return false;
    }
    return true;
}

bool hasMethod(const char *name, bool reportMissing = true)
{
    const QMetaObject &metaObject = GlobalShortcutPortal::staticMetaObject;
    for (int index = metaObject.methodOffset(); index < metaObject.methodCount(); ++index) {
        const QMetaMethod method = metaObject.method(index);
        if (method.methodType() == QMetaMethod::Slot && method.name() == name)
            return true;
    }
    if (reportMissing)
        std::cerr << "Missing GlobalShortcuts method " << name << '\n';
    return false;
}

bool hasSignal(const char *name)
{
    const QMetaObject &metaObject = GlobalShortcutPortal::staticMetaObject;
    for (int index = metaObject.methodOffset(); index < metaObject.methodCount(); ++index) {
        const QMetaMethod method = metaObject.method(index);
        if (method.methodType() == QMetaMethod::Signal && method.name() == name)
            return true;
    }
    std::cerr << "Missing GlobalShortcuts signal " << name << '\n';
    return false;
}

struct InterfaceContract
{
    QHash<QString, QString> methods;
    QHash<QString, QString> signalEntries;
    QHash<QString, QString> properties;
};

InterfaceContract readInterface(const QString &path, const QString &interfaceName, bool &ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "Could not open " << path.toStdString() << '\n';
        ok = false;
        return {};
    }

    QXmlStreamReader xml(&file);
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

    if (xml.hasError()) {
        std::cerr << "Invalid XML " << path.toStdString() << ": "
                  << xml.errorString().toStdString() << '\n';
        ok = false;
    }
    return contract;
}

bool expectEntry(const QHash<QString, QString> &entries,
                 const QString &name,
                 const QString &signature)
{
    if (entries.value(name) != signature) {
        std::cerr << "Expected " << name.toStdString() << " signature "
                  << signature.toStdString() << ", got "
                  << entries.value(name, QStringLiteral("<missing>")).toStdString() << '\n';
        return false;
    }
    return true;
}

bool verifyBackendXml()
{
    bool ok = true;
    const InterfaceContract contract = readInterface(
            QStringLiteral(GLOBAL_SHORTCUTS_XML_PATH),
            QStringLiteral("org.freedesktop.impl.portal.GlobalShortcuts"),
            ok);
    return ok
            && contract.methods.size() == 4
            && contract.signalEntries.size() == 3
            && expectEntry(contract.methods,
                           QStringLiteral("CreateSession"),
                           QStringLiteral("oosa{sv}->ua{sv}"))
            && expectEntry(contract.methods,
                           QStringLiteral("BindShortcuts"),
                           QStringLiteral("ooa(sa{sv})sa{sv}->ua{sv}"))
            && expectEntry(contract.methods,
                           QStringLiteral("ListShortcuts"),
                           QStringLiteral("oo->ua{sv}"))
            && expectEntry(contract.methods,
                           QStringLiteral("ConfigureShortcuts"),
                           QStringLiteral("osa{sv}->"))
            && expectEntry(contract.signalEntries,
                           QStringLiteral("Activated"),
                           QStringLiteral("osta{sv}"))
            && expectEntry(contract.signalEntries,
                           QStringLiteral("Deactivated"),
                           QStringLiteral("osta{sv}"))
            && expectEntry(contract.signalEntries,
                           QStringLiteral("ShortcutsChanged"),
                           QStringLiteral("oa(sa{sv})"))
            && expectEntry(contract.properties, QStringLiteral("version"), QStringLiteral("u"));
}

bool verifyPrivateXml()
{
    bool ok = true;
    const InterfaceContract management = readInterface(
            QStringLiteral(GLOBAL_SHORTCUTS_MANAGEMENT_XML_PATH),
            QStringLiteral("org.deepin.dde.impl.portal.GlobalShortcutsManagement1"),
            ok);
    const InterfaceContract provider = readInterface(
            QStringLiteral(GLOBAL_SHORTCUTS_PROVIDER_XML_PATH),
            QStringLiteral("org.deepin.dde.GlobalShortcutsProvider1"),
            ok);
    return ok
            && expectEntry(management.methods,
                           QStringLiteral("CompleteBind"),
                           QStringLiteral("sua(ssasasb)->"))
            && expectEntry(management.methods,
                           QStringLiteral("ApplySessionShortcuts"),
                           QStringLiteral("oa(ssasasb)->a(ssssasasbb)"))
            && expectEntry(management.methods,
                           QStringLiteral("CheckSessionShortcutConflicts"),
                           QStringLiteral("oa(ssasasb)->a(sssss)"))
            && expectEntry(provider.methods,
                           QStringLiteral("BeginBind"),
                           QStringLiteral("sossa(ssasasb)->"))
            && expectEntry(provider.methods,
                           QStringLiteral("ConfigureShortcuts"),
                           QStringLiteral("ossa(ssssasasbb)a{sv}->"))
            && expectEntry(provider.methods,
                           QStringLiteral("CancelRequest"),
                           QStringLiteral("s->"));
}

bool verifyPortalDeclaration()
{
    QFile file(QStringLiteral(DDE_PORTAL_DECLARATION_PATH));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = file.readAll();
    if (!data.contains("org.freedesktop.impl.portal.GlobalShortcuts")) {
        std::cerr << "dde.portal does not advertise GlobalShortcuts\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    registerGlobalShortcutMetaTypes();

    QObject parent;
    GlobalShortcutPortal portal(&parent);
    if (portal.version() != 2) {
        std::cerr << "GlobalShortcuts backend version is not 2\n";
        return 1;
    }

    const bool typeContracts =
            expectTypeSignature(QMetaType::fromType<XdgShortcut>(), "(sa{sv})")
            && expectTypeSignature(QMetaType::fromType<XdgShortcutList>(), "a(sa{sv})")
            && expectTypeSignature(QMetaType::fromType<PortalShortcutInfo>(), "(ssssasasbb)")
            && expectTypeSignature(QMetaType::fromType<PortalShortcutInfoList>(),
                                   "a(ssssasasbb)")
            && expectTypeSignature(QMetaType::fromType<PortalShortcutUpdate>(), "(ssasasb)")
            && expectTypeSignature(QMetaType::fromType<PortalShortcutUpdateList>(),
                                   "a(ssasasb)")
            && expectTypeSignature(QMetaType::fromType<PortalShortcutConflict>(), "(sssss)")
            && expectTypeSignature(QMetaType::fromType<PortalShortcutConflictList>(),
                                   "a(sssss)")
            && expectTypeSignature(QMetaType::fromType<PortalTrigger>(), "(ss)")
            && expectTypeSignature(QMetaType::fromType<PortalTriggerList>(), "a(ss)");

    const bool metaObjectContracts = hasMethod("CreateSession")
            && hasMethod("BindShortcuts")
            && hasMethod("ListShortcuts")
            && hasMethod("ConfigureShortcuts")
            && hasSignal("Activated")
            && hasSignal("Deactivated")
            && hasSignal("ShortcutsChanged")
            && !hasMethod("BindShortCuts", false)
            && !hasMethod("ListShortCuts", false);

    return typeContracts && metaObjectContracts && verifyBackendXml() && verifyPrivateXml()
                    && verifyPortalDeclaration()
            ? 0
            : 1;
}
