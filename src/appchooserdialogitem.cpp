// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "appchooserdialogitem.h"

#include <QMouseEvent>
#include <QFontMetrics>
#include <QVBoxLayout>

#include <QDebug>

AppChooserDialogItem::AppChooserDialogItem(const QString &applicationName, const QString &icon, const QString &applicationExec, QWidget *parent)
    : QToolButton(parent)
    , m_applicationName(applicationExec)
{
    setAutoRaise(true);
    setAutoExclusive(true);
    setStyleSheet(QStringLiteral("text-align: center"));
    setIcon(QIcon::fromTheme(icon));
    setIconSize(QSize(64, 64));
    setCheckable(true);
    setFixedWidth(150);
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    QFontMetrics metrics(font());
    QString elidedText = metrics.elidedText(applicationName, Qt::ElideRight, 128);
    setText(elidedText);

    connect(this, &QToolButton::toggled, this, [this] (bool toggled) {
        if (!toggled) {
            setDown(false);
        }
    });
}

AppChooserDialogItem::~AppChooserDialogItem()
{
}

QString AppChooserDialogItem::applicationName() const
{
    return m_applicationName;
}

void AppChooserDialogItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_EMIT doubleClicked(m_applicationName);

    QToolButton::mouseDoubleClickEvent(event);
}

void AppChooserDialogItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setDown(true);
        setChecked(true);
    }
}

void AppChooserDialogItem::mouseReleaseEvent(QMouseEvent *event)
{
    event->ignore();
}
