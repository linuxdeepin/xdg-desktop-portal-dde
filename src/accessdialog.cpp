// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

//
// Created by uos on 24-9-9.
//

#include "accessdialog.h"
#include <QWindow>
#include <QMessageBox>
// X11的声明放在下面，防止编译报错
#include <X11/Xlib.h>

Display* getX11Display() {
    // 使用 XOpenDisplay 获取 X11 Display
    Display *display = XOpenDisplay(nullptr); // 使用默认的 Display
    if (!display) {
        qWarning() << "Failed to open X11 display";
    }
    return display;
}

// 从 X11 Display 获取屏幕几何信息
QRect getScreenGeometryFromX11(Display *display, Window screenNumber) {
    if (!display) {
        return QRect();
    }
    QRect geometry;
    geometry.setLeft(0);
    geometry.setTop(0);
    geometry.setWidth(DisplayWidth(display, screenNumber));
    geometry.setHeight(DisplayHeight(display, screenNumber));
    return geometry;
}

accessDialog::accessDialog(const QString &app_id, const QString &parent_window, const QString &title, const QString &subtitle, const QString &body, const QVariantMap &options) :
    DDialog(),
    m_titleLabel(new QLabel(this)),
    m_subtitleLabel(new QLabel(this)),
    m_bodyLabel(new QLabel(this))
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
        addButton(options.value(QStringLiteral("deny_label")).toString(), QMessageBox::RejectRole);
    } else {
        addButton("不允许", QMessageBox::RejectRole);
    }


    int allowButton;
    if (options.contains(QStringLiteral("grant_label"))) {
        addButton(options.value(QStringLiteral("grant_label")).toString(), QMessageBox::AcceptRole);
    } else {
        addButton("好", QMessageBox::AcceptRole);
    }

    setWindowFlag(Qt::WindowStaysOnTopHint);
}

accessDialog::~accessDialog(){

}