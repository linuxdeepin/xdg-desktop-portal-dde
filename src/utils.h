// Copyright © 2018 Alexander Volkov <a.volkov@rusbitech.ru>
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_UTILS_H
#define XDG_DESKTOP_PORTAL_KDE_UTILS_H

class QString;
class QWidget;

class Utils
{
public:
    static void setParentWindow(QWidget *w, const QString &parent_window);
};

#endif // XDG_DESKTOP_PORTAL_KDE_UTILS_H
