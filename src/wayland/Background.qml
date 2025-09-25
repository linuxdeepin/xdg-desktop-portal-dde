// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick

import org.deepin.dtk 1.0 as D

Rectangle {
    property color darkColor: "#0DFFFFFF"
    property color lightColor: "#0D000000"
    color: D.DTK.themeType === D.ApplicationHelper.DarkType ? darkColor : lightColor
}
