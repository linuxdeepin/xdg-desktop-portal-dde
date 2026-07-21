// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "accessdialogtypes.h"

namespace AccessDialogTypes
{

QDBusArgument &operator<<(QDBusArgument &argument, const Choice &choice)
{
    argument.beginStructure();
    argument << choice.id << choice.value;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Choice &choice)
{
    argument.beginStructure();
    argument >> choice.id >> choice.value;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Option &option)
{
    argument.beginStructure();
    argument << option.id << option.label << option.choices << option.initialChoiceId;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Option &option)
{
    argument.beginStructure();
    argument >> option.id >> option.label >> option.choices >> option.initialChoiceId;
    argument.endStructure();
    return argument;
}

}
