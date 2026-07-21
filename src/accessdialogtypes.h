// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QDBusArgument>
#include <QList>
#include <QString>

namespace AccessDialogTypes
{

struct Choice
{
    QString id;
    QString value;
};
using Choices = QList<Choice>;

struct Option
{
    QString id;
    QString label;
    Choices choices;
    QString initialChoiceId;
};
using OptionList = QList<Option>;

QDBusArgument &operator<<(QDBusArgument &argument, const Choice &choice);
const QDBusArgument &operator>>(const QDBusArgument &argument, Choice &choice);
QDBusArgument &operator<<(QDBusArgument &argument, const Option &option);
const QDBusArgument &operator>>(const QDBusArgument &argument, Option &option);

}

Q_DECLARE_METATYPE(AccessDialogTypes::Choice)
Q_DECLARE_METATYPE(AccessDialogTypes::Choices)
Q_DECLARE_METATYPE(AccessDialogTypes::Option)
Q_DECLARE_METATYPE(AccessDialogTypes::OptionList)
