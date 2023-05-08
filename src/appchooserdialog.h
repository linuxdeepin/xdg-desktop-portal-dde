// Copyright © 2016-2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_APPCHOOSER_DIALOG_H
#define XDG_DESKTOP_PORTAL_KDE_APPCHOOSER_DIALOG_H

#include <QAbstractListModel>
#include <QDialog>

class AppChooserDialogItem;
class QGridLayout;

class AppChooserDialog : public QDialog
{
    Q_OBJECT
public:
    AppChooserDialog(const QStringList &choices, const QString &defaultApp, const QString &fileName, QDialog *parent = nullptr, Qt::WindowFlags flags = {});
    ~AppChooserDialog();

    void updateChoices(const QStringList &choices);

    QString selectedApplication() const;

private:
    void addDialogItems();

    QStringList m_choices;
    QString m_defaultApp;
    QString m_selectedApplication;
    QGridLayout *m_gridLayout;
};

#endif // XDG_DESKTOP_PORTAL_KDE_APPCHOOSER_DIALOG_H


