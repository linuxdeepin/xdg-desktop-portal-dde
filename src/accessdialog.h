// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "accessdialogtypes.h"

#include <DDialog>

#include <QMap>

DWIDGET_USE_NAMESPACE

class AccessDialog : public DDialog
{
    Q_OBJECT

public:
    AccessDialog(const QString &title,
                 const QString &subtitle,
                 const QString &body,
                 const QVariantMap &options,
                 const AccessDialogTypes::OptionList &choices,
                 QWidget *parent = nullptr);

    AccessDialogTypes::Choices selectedChoices() const;

private:
    AccessDialogTypes::OptionList m_choices;
    QMap<QString, QString> m_selectedChoices;
};
