// Copyright © 2018 Alexander Volkov <a.volkov@rusbitech.ru>
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "utils.h"

#include <KWindowSystem>

#include <QString>
#include <QWidget>
#include <QWindow>

void Utils::setParentWindow(QWidget *w, const QString &parent_window)
{
    if (parent_window.startsWith(QLatin1String("x11:"))) {
        KWindowSystem::setMainWindow(w, parent_window.midRef(4).toULongLong(nullptr, 16));
    }
}
