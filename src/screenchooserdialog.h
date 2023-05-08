// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_DDE_SCREENCHOOSER_DIALOG_H
#define XDG_DESKTOP_PORTAL_DDE_SCREENCHOOSER_DIALOG_H

#include <QDialog>
#include "screencast.h"

namespace Ui
{
class ScreenChooserDialog;
}
class QItemSelection;

class ScreenChooserDialog : public QDialog
{
    Q_OBJECT
public:
    ScreenChooserDialog(const QString &appName, bool multiple, QDialog *parent = nullptr, Qt::WindowFlags flags = {});
    ~ScreenChooserDialog();

    void setSourceTypes(ScreenCastPortal::SourceTypes types);

    QList<quint32> selectedScreens() const;
    QList<QByteArray> selectedWindows() const;

private:
    void selectionChanged(const QItemSelection &selected);

    const bool m_multiple;
    Ui::ScreenChooserDialog *m_dialog;
};

#endif // XDG_DESKTOP_PORTAL_DDE_SCREENCHOOSER_DIALOG_H
