// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "accessdialogtypes.h"

#include <QCoreApplication>
#include <QDBusMetaType>

#include <cstring>
#include <iostream>

namespace
{

bool expectSignature(QMetaType type, const char *expected)
{
    const char *signature = QDBusMetaType::typeToSignature(type);
    if (!signature || std::strcmp(signature, expected) != 0) {
        std::cerr << "Expected D-Bus signature " << expected << ", got "
                  << (signature ? signature : "<unregistered>") << '\n';
        return false;
    }
    return true;
}

}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    qDBusRegisterMetaType<AccessDialogTypes::Choice>();
    qDBusRegisterMetaType<AccessDialogTypes::Choices>();
    qDBusRegisterMetaType<AccessDialogTypes::Option>();
    qDBusRegisterMetaType<AccessDialogTypes::OptionList>();

    return expectSignature(QMetaType::fromType<AccessDialogTypes::Choices>(), "a(ss)")
                    && expectSignature(QMetaType::fromType<AccessDialogTypes::OptionList>(),
                                       "a(ssa(ss)s)")
            ? 0
            : 1;
}
