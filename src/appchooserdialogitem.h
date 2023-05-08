// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_APPCHOOSER_DIALOG_ITEM_H
#define XDG_DESKTOP_PORTAL_KDE_APPCHOOSER_DIALOG_ITEM_H

class QMouseEvent;

#include <QToolButton>

class AppChooserDialogItem : public QToolButton
{
    Q_OBJECT
public:
    explicit AppChooserDialogItem(const QString &applicationName, const QString &icon, const QString &applicationExec, QWidget *parent = nullptr);
    ~AppChooserDialogItem() override;

    QString applicationName() const;

    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
Q_SIGNALS:
    void doubleClicked(const QString &applicationName);

private:
    QString m_applicationName;
};

#endif // XDG_DESKTOP_PORTAL_KDE_APPCHOOSER_DIALOG_ITEM_H



