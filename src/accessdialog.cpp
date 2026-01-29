// Copyright (C) 2024 Wenhao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "accessdialog.h"
#include <QWindow>
#include <QMessageBox>
#include <QTimer>
#include <QPushButton>
// X11的声明放在下面，防止编译报错
#include <X11/Xlib.h>

AccessDialog::AccessDialog(const QString &app_id, const QString &parent_window, const QString &title, const QString &subtitle, const QString &body, const QVariantMap &options) :
    DDialog(),
    m_titleLabel(new QLabel(this)),
    m_subtitleLabel(new QLabel(this)),
    m_bodyLabel(new QLabel(this)),
    m_countdownTimer(new QTimer(this)),
    m_denyButton(nullptr),
    m_remainingSeconds(15)
{
    setAccessibleName("AccessDialog");
    setIcon(QIcon::fromTheme("dialog-warning"));
    setAttribute(Qt::WA_QuitOnClose);
    // 设置tittle
    m_titleLabel->setObjectName("TitileText");
    m_titleLabel->setAccessibleName("TitileText");
    addContent(m_titleLabel, Qt::AlignTop | Qt::AlignHCenter);
    QFont font = m_titleLabel->font();
    font.setBold(true);
    font.setPixelSize(16);
    m_titleLabel->setFont(font);
    m_titleLabel->setText(title);
    // 设置subtitle
    m_subtitleLabel->setObjectName("SubtitleText");
    m_subtitleLabel->setAccessibleName("SubtitleText");
    addContent(m_subtitleLabel, Qt::AlignTop | Qt::AlignHCenter);
    m_subtitleLabel->setText(subtitle+"\n");
    // 设置body
    m_bodyLabel->setObjectName("BodyText");
    m_bodyLabel->setAccessibleName("BodyText");
    addContent(m_bodyLabel, Qt::AlignTop | Qt::AlignHCenter);
    m_bodyLabel->setText(body);

    if (options.contains(QStringLiteral("modal"))) {
        setModal(options.value(QStringLiteral("modal")).toBool());
    }

    if (options.contains(QStringLiteral("deny_label"))) {
        m_denyLabel = options.value(QStringLiteral("deny_label")).toString();
    } else {
        m_denyLabel = tr("Deny Access");
    }
    
    int denyButtonIndex = addButton(tr("%1 (%2s)", "e.g. Deny Access (15s)").arg(m_denyLabel).arg(m_remainingSeconds), QMessageBox::RejectRole);
    m_denyButton = qobject_cast<QPushButton*>(getButton(denyButtonIndex));
    
    // 设置定时器
    connect(m_countdownTimer, &QTimer::timeout, this, &AccessDialog::updateDenyButtonText);
    m_countdownTimer->start(1000); // 每秒更新一次


    int allowButton;
    if (options.contains(QStringLiteral("grant_label"))) {
        addButton(options.value(QStringLiteral("grant_label")).toString(), QMessageBox::AcceptRole);
    } else {
        addButton(tr("Grant Access"), QMessageBox::AcceptRole);
    }

    setWindowFlag(Qt::WindowStaysOnTopHint);
}

AccessDialog::~AccessDialog(){
    if (m_countdownTimer) {
        m_countdownTimer->stop();
    }
}

void AccessDialog::updateDenyButtonText()
{
    m_remainingSeconds--;
    
    if (m_remainingSeconds <= 0) {
        m_countdownTimer->stop();
        onTimeout();
    } else if (m_denyButton) {
        m_denyButton->setText(tr("%1 (%2s)", "e.g. Deny Access (15s)").arg(m_denyLabel).arg(m_remainingSeconds));
    }
}

void AccessDialog::onTimeout()
{
    // 倒计时结束，自动拒绝
    reject();
}