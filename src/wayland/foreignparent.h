// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QString>

class QWindow;

namespace WaylandForeignParent
{

bool setParent(QWindow *window, const QString &handle);

}
