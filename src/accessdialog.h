// Copyright (C) 2024 Wenhao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef XDG_DESKTOP_PORTAL_DDE_ACCESSDIALOG_H
#define XDG_DESKTOP_PORTAL_DDE_ACCESSDIALOG_H

#include <ddialog.h>
#include <DWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QTimer>
#include <QPushButton>

DWIDGET_USE_NAMESPACE
class LargeLabel;

class AccessDialog : public DDialog
{
    Q_OBJECT
public:
    explicit AccessDialog(const QString &app_id,const QString &parent_window,const QString &title,const QString &subtitle,const QString &body, const QVariantMap &options);
    ~AccessDialog();
private slots:
    void updateDenyButtonText();
    void onTimeout();
private:
    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    QLabel *m_bodyLabel;
    QTimer *m_countdownTimer;
    QPushButton *m_denyButton;
    int m_remainingSeconds;
    QString m_denyLabel;
};

#endif // XDG_DESKTOP_PORTAL_DDE_ACCESSDIALOG_H
