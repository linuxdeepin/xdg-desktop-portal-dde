// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QStringList>

class AppChooserDialog : public QDialog
{
public:
    explicit AppChooserDialog(QWidget *parent = nullptr);

    const QStringList &selectChoices() const ;

    void updateChoices(const QStringList &choices);
    void setCurrentChoice(const QString &choice);

private:
    QStringList m_choices;
    QStringList m_selectedChoices;
};
