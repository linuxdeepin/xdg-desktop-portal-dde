// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

//
// Created by uos on 24-9-9.
//

#ifndef XDG_DESKTOP_PORTAL_DDE_ACCESSDIALOG_H
#define XDG_DESKTOP_PORTAL_DDE_ACCESSDIALOG_H

#include <ddialog.h>
#include <DWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QDialogButtonBox>

DWIDGET_USE_NAMESPACE
class LargeLabel;

class accessDialog : public DDialog
{
    Q_OBJECT
public:
    explicit accessDialog(const QString &app_id,const QString &parent_window,const QString &title,const QString &subtitle,const QString &body, const QVariantMap &options);
    ~accessDialog();
private:
    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    QLabel *m_bodyLabel;
};

#endif // XDG_DESKTOP_PORTAL_DDE_ACCESSDIALOG_H
