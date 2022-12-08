// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "appchooserdialog.h"

AppChooserDialog::AppChooserDialog(QWidget *parent)
    : QDialog(parent)
{

}

const QStringList &AppChooserDialog::selectChoices() const
{
    return m_selectedChoices;
}

void AppChooserDialog::updateChoices(const QStringList &choices)
{

}

void AppChooserDialog::setCurrentChoice(const QString &choice)
{

}
