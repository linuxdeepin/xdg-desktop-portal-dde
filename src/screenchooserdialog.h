// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_SCREENCHOOSER_DIALOG_H
#define XDG_DESKTOP_PORTAL_KDE_SCREENCHOOSER_DIALOG_H

#include <QDialog>

namespace Ui
{
class ScreenChooserDialog;
}

class ScreenChooserDialog : public QDialog
{
    Q_OBJECT
public:
    ScreenChooserDialog(const QString &appName, bool multiple, QDialog *parent = nullptr, Qt::WindowFlags flags = {});
    ~ScreenChooserDialog();

    QList<quint32> selectedScreens() const;

private:
    Ui::ScreenChooserDialog *m_dialog;
};

#endif // XDG_DESKTOP_PORTAL_KDE_SCREENCHOOSER_DIALOG_H
