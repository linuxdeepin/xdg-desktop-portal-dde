// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_DDE_SCREENCAST_WIDGET_H
#define XDG_DESKTOP_PORTAL_DDE_SCREENCAST_WIDGET_H

#include <QListWidget>

class ScreenCastWidget : public QListWidget
{
    Q_OBJECT
public:
    ScreenCastWidget(QWidget *parent = nullptr);
    ~ScreenCastWidget();

    QList<quint32> selectedScreens() const;
};

#endif // XDG_DESKTOP_PORTAL_DDE_SCREENCAST_WIDGET_H

