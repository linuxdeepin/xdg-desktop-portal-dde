// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_REMOTEDESKTOP_DIALOG_H
#define XDG_DESKTOP_PORTAL_KDE_REMOTEDESKTOP_DIALOG_H

#include <QAbstractListModel>
#include <QDialog>

#include "remotedesktop.h"

namespace Ui
{
class RemoteDesktopDialog;
}

class RemoteDesktopDialog : public QDialog
{
    Q_OBJECT
public:
    RemoteDesktopDialog(const QString &appName, RemoteDesktopPortal::DeviceTypes deviceTypes, bool screenSharingEnabled = false,
                        bool multiple = false, QDialog *parent = nullptr, Qt::WindowFlags flags = {});
    ~RemoteDesktopDialog();

    QList<quint32> selectedScreens() const;
    RemoteDesktopPortal::DeviceTypes deviceTypes() const;

private:
    Ui::RemoteDesktopDialog * m_dialog;
};

#endif // XDG_DESKTOP_PORTAL_KDE_REMOTEDESKTOP_DIALOG_H
